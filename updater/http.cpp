#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>

#include <string>
#include <cstring>
#include <cstdio>
#include <list>
#include <vector>

#include <bearssl.h>

#include "http.h"

using namespace std;

#define MAX_REDIRECTS		5
#define MAX_RESPONSE_SIZE	(32 * 1024 * 1024)

extern "C" {
	extern const br_x509_trust_anchor g_PCMDSTrustAnchors[];
	extern const size_t g_iPCMDSTrustAnchors;
}

/*
 * Trust anchors: built-in roots for GitHub + system CA bundle (if any),
 * so a root change on GitHub side doesn't break the updater.
 */
static const char *g_pSystemCABundles[] = {
	"/etc/ssl/certs/ca-certificates.crt",	//Debian/Ubuntu
	"/etc/pki/tls/certs/ca-bundle.crt",		//CentOS/RHEL
	"/etc/ssl/ca-bundle.pem",				//OpenSUSE
	"/etc/ssl/cert.pem"						//Alpine
};

static vector<br_x509_trust_anchor> g_TrustAnchors;
static list<string> g_TrustAnchorsData;
static bool g_bTrustAnchorsLoaded = false;

static unsigned char *StoreData(const void *pData, size_t iLength) {
	g_TrustAnchorsData.push_back(string((const char *)pData, iLength));

	return (unsigned char *)g_TrustAnchorsData.back().data();
}

static void AppendToString(void *pCtx, const void *pData, size_t iLength) {
	((string *)pCtx)->append((const char *)pData, iLength);
}

static void AddTrustAnchor(const string &sDER) {
	string sDN;
	br_x509_decoder_context Decoder;
	br_x509_decoder_init(&Decoder, AppendToString, &sDN);
	br_x509_decoder_push(&Decoder, sDER.data(), sDER.length());

	br_x509_pkey *pKey = br_x509_decoder_get_pkey(&Decoder);
	if (!pKey) return;

	br_x509_trust_anchor TA;
	memset(&TA, 0, sizeof(TA));

	TA.dn.data = StoreData(sDN.data(), sDN.length());
	TA.dn.len = sDN.length();
	TA.flags = br_x509_decoder_isCA(&Decoder) ? BR_X509_TA_CA : 0;
	TA.pkey.key_type = pKey->key_type;

	switch (pKey->key_type) {
		case BR_KEYTYPE_RSA:
			TA.pkey.key.rsa.n = StoreData(pKey->key.rsa.n, pKey->key.rsa.nlen);
			TA.pkey.key.rsa.nlen = pKey->key.rsa.nlen;
			TA.pkey.key.rsa.e = StoreData(pKey->key.rsa.e, pKey->key.rsa.elen);
			TA.pkey.key.rsa.elen = pKey->key.rsa.elen;
			break;
		case BR_KEYTYPE_EC:
			TA.pkey.key.ec.curve = pKey->key.ec.curve;
			TA.pkey.key.ec.q = StoreData(pKey->key.ec.q, pKey->key.ec.qlen);
			TA.pkey.key.ec.qlen = pKey->key.ec.qlen;
			break;
		default:
			return;
	}

	g_TrustAnchors.push_back(TA);
}

static void LoadSystemTrustAnchors(const char *pFilename) {
	FILE *pFile = fopen(pFilename, "rb");
	if (!pFile) return;

	br_pem_decoder_context PEM;
	br_pem_decoder_init(&PEM);

	string sDER;
	bool bCertificate = false;

	unsigned char buffer[4096];
	size_t iLength;
	while ((iLength = fread(buffer, 1, sizeof(buffer), pFile)) > 0) {
		unsigned char *pBuffer = buffer;

		while (iLength > 0) {
			size_t iPushed = br_pem_decoder_push(&PEM, pBuffer, iLength);
			pBuffer += iPushed;
			iLength -= iPushed;

			switch (br_pem_decoder_event(&PEM)) {
				case BR_PEM_BEGIN_OBJ: {
					const char *pName = br_pem_decoder_name(&PEM);
					bCertificate = !strcmp(pName, "CERTIFICATE") || !strcmp(pName, "X509 CERTIFICATE");

					sDER.clear();
					br_pem_decoder_setdest(&PEM, bCertificate ? AppendToString : nullptr, &sDER);
					break;
				}
				case BR_PEM_END_OBJ:
					if (bCertificate) AddTrustAnchor(sDER);
					bCertificate = false;
					break;
				case BR_PEM_ERROR:
					fclose(pFile);
					return;
			}
		}
	}

	fclose(pFile);
}

static void LoadTrustAnchors() {
	if (g_bTrustAnchorsLoaded) return;
	g_bTrustAnchorsLoaded = true;

	g_TrustAnchors.assign(g_PCMDSTrustAnchors, g_PCMDSTrustAnchors + g_iPCMDSTrustAnchors);

	for (size_t i = 0; i < sizeof(g_pSystemCABundles) / sizeof(g_pSystemCABundles[0]); i++) {
		if (access(g_pSystemCABundles[i], R_OK)) continue;

		LoadSystemTrustAnchors(g_pSystemCABundles[i]);
		break;
	}
}

/*
 * Minimal TLS 1.2 client: ECDHE + AES-GCM only (what GitHub offers),
 * instead of br_ssl_client_init_full() which pulls every algorithm.
 */
static void InitTLSClient(br_ssl_client_context *pCC, br_x509_minimal_context *pXC) {
	static const uint16_t Suites[] = {
		BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
		BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
		BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
	};

	br_ssl_engine_context *pEngine = &pCC->eng;

	br_ssl_client_zero(pCC);
	br_ssl_engine_set_versions(pEngine, BR_TLS12, BR_TLS12);
	br_ssl_engine_set_suites(pEngine, Suites, sizeof(Suites) / sizeof(Suites[0]));

	br_x509_minimal_init(pXC, &br_sha256_vtable, g_TrustAnchors.data(), g_TrustAnchors.size());
	br_x509_minimal_set_hash(pXC, br_sha256_ID, &br_sha256_vtable);
	br_x509_minimal_set_hash(pXC, br_sha384_ID, &br_sha384_vtable);
	br_x509_minimal_set_hash(pXC, br_sha512_ID, &br_sha512_vtable);
	br_x509_minimal_set_rsa(pXC, br_rsa_i31_pkcs1_vrfy);
	br_x509_minimal_set_ecdsa(pXC, &br_ec_prime_i31, br_ecdsa_i31_vrfy_asn1);

	br_ssl_engine_set_hash(pEngine, br_sha256_ID, &br_sha256_vtable);
	br_ssl_engine_set_hash(pEngine, br_sha384_ID, &br_sha384_vtable);
	br_ssl_engine_set_prf_sha256(pEngine, &br_tls12_sha256_prf);
	br_ssl_engine_set_prf_sha384(pEngine, &br_tls12_sha384_prf);

	br_ssl_engine_set_rsavrfy(pEngine, br_rsa_i31_pkcs1_vrfy);
	br_ssl_engine_set_ec(pEngine, &br_ec_prime_i31);
	br_ssl_engine_set_ecdsa(pEngine, br_ecdsa_i31_vrfy_asn1);

	br_ssl_engine_set_gcm(pEngine, &br_sslrec_in_gcm_vtable, &br_sslrec_out_gcm_vtable);
	br_ssl_engine_set_aes_ctr(pEngine, &br_aes_ct_ctr_vtable);
	br_ssl_engine_set_ghash(pEngine, br_ghash_ctmul32);

	br_ssl_engine_set_x509(pEngine, &pXC->vtable);
}

static const char *GetTLSError(int iError) {
	switch (iError) {
		case BR_ERR_X509_NOT_TRUSTED:		return "certificate is not trusted";
		case BR_ERR_X509_EXPIRED:			return "certificate expired or not yet valid (check system time)";
		case BR_ERR_X509_BAD_SERVER_NAME:	return "certificate does not match host name";
		case BR_ERR_BAD_CIPHER_SUITE:
		case BR_ERR_BAD_VERSION:			return "server does not support TLS 1.2 ECDHE+AES-GCM";
		case BR_ERR_IO:						return "connection failed";
	}

	return nullptr;
}

class CTLS {
	public:
		br_ssl_client_context	CC;
		br_x509_minimal_context	XC;
		br_sslio_context		IOC;
		unsigned char			IOBuffer[BR_SSL_BUFSIZE_BIDI];
};

static int SockRead(void *pCtx, unsigned char *pBuffer, size_t iLength) {
	while (true) {
		ssize_t iRead = recv(*(int *)pCtx, pBuffer, iLength, 0);
		if (iRead > 0) return (int)iRead;
		if (iRead < 0 && errno == EINTR) continue;

		return -1;
	}
}

static int SockWrite(void *pCtx, const unsigned char *pBuffer, size_t iLength) {
	while (true) {
		ssize_t iWritten = send(*(int *)pCtx, pBuffer, iLength, MSG_NOSIGNAL);
		if (iWritten > 0) return (int)iWritten;
		if (iWritten < 0 && errno == EINTR) continue;

		return -1;
	}
}

bool CURL::Parse(const string &sUrl) {
	size_t iPos;

	if (!strncasecmp(sUrl.c_str(), "https://", 8)) {
		bHTTPS = true;
		iPort = 443;
		iPos = 8;
	}
	else if (!strncasecmp(sUrl.c_str(), "http://", 7)) {
		bHTTPS = false;
		iPort = 80;
		iPos = 7;
	}
	else return false;

	size_t iEnd = sUrl.find_first_of("/?#", iPos);
	string sAuthority = sUrl.substr(iPos, iEnd == string::npos ? string::npos : iEnd - iPos);

	size_t iColon = sAuthority.find(':');
	if (iColon != string::npos) {
		iPort = atoi(sAuthority.c_str() + iColon + 1);
		sAuthority.resize(iColon);
	}

	sHostName = sAuthority;

	Path = iEnd == string::npos ? "/" : sUrl.substr(iEnd);
	if (Path[0] != '/') Path.insert(0, "/");

	size_t iFragment = Path.find('#');
	if (iFragment != string::npos) Path.resize(iFragment);

	return sHostName.length() && iPort > 0 && iPort < 65536;
}

CHTTP::CHTTP() {
}

CHTTP::~CHTTP() {
	Clean();
}

void CHTTP::Clean() {
	if (m_iTCPSock != -1) {
		close(m_iTCPSock);
		m_iTCPSock = -1;
	}
}

void CHTTP::SetTimeout(int iTimeout) {
	m_iTimeout = iTimeout;
}

void CHTTP::SetUserAgent(const char *pUserAgent) {
	m_sUserAgent = pUserAgent;
}

bool CHTTP::OpenConnection(const CURL &URL) {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));

	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;

	char sPort[8];
	snprintf(sPort, sizeof(sPort), "%d", URL.iPort);

	struct addrinfo *pAddrInfo = nullptr;
	if (getaddrinfo(URL.sHostName.c_str(), sPort, &hints, &pAddrInfo) || !pAddrInfo) {
		m_sError = "can't resolve " + URL.sHostName;

		return false;
	}

	struct timeval timeout;
	timeout.tv_sec = m_iTimeout / 1000;
	timeout.tv_usec = (m_iTimeout % 1000) * 1000;

	for (struct addrinfo *pAddr = pAddrInfo; pAddr; pAddr = pAddr->ai_next) {
		m_iTCPSock = socket(pAddr->ai_family, pAddr->ai_socktype, pAddr->ai_protocol);
		if (m_iTCPSock == -1) continue;

		int iFlags = fcntl(m_iTCPSock, F_GETFL, 0);
		fcntl(m_iTCPSock, F_SETFL, iFlags | O_NONBLOCK);

		bool bConnected = !connect(m_iTCPSock, pAddr->ai_addr, pAddr->ai_addrlen);
		if (!bConnected && errno == EINPROGRESS) {
			fd_set fdset;
			FD_ZERO(&fdset);
			FD_SET(m_iTCPSock, &fdset);

			struct timeval wait = timeout;
			if (select(m_iTCPSock + 1, nullptr, &fdset, nullptr, &wait) == 1) {
				int iError = 0;
				socklen_t iLen = sizeof(iError);
				getsockopt(m_iTCPSock, SOL_SOCKET, SO_ERROR, &iError, &iLen);

				bConnected = !iError;
			}
		}

		if (bConnected) {
			fcntl(m_iTCPSock, F_SETFL, iFlags & ~O_NONBLOCK);
			setsockopt(m_iTCPSock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
			setsockopt(m_iTCPSock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

			freeaddrinfo(pAddrInfo);

			return true;
		}

		Clean();
	}

	freeaddrinfo(pAddrInfo);

	m_sError = "can't connect to " + URL.sHostName;

	return false;
}

static bool HeaderIs(const string &sLine, const char *pName, string &sValue) {
	size_t iLength = strlen(pName);
	if (sLine.length() <= iLength || sLine[iLength] != ':' || strncasecmp(sLine.c_str(), pName, iLength)) return false;

	size_t iStart = sLine.find_first_not_of(" \t", iLength + 1);
	sValue = iStart == string::npos ? "" : sLine.substr(iStart);

	return true;
}

bool CHTTP::ParseResponse(const string &sAnswer, CResponseHTTP &Response) {
	size_t iHeadersEnd = sAnswer.find("\r\n\r\n");
	if (iHeadersEnd == string::npos || strncmp(sAnswer.c_str(), "HTTP/1.", 7)) {
		m_sError = "bad HTTP response";

		return false;
	}

	size_t iSpace = sAnswer.find(' ');
	Response.Status = iSpace < iHeadersEnd ? atoi(sAnswer.c_str() + iSpace + 1) : 0;

	bool bChunked = false;
	long long iContentLength = -1;

	size_t iPos = sAnswer.find("\r\n") + 2;
	while (iPos < iHeadersEnd) {
		size_t iLineEnd = sAnswer.find("\r\n", iPos);
		string sLine = sAnswer.substr(iPos, iLineEnd - iPos), sValue;
		iPos = iLineEnd + 2;

		if (HeaderIs(sLine, "Location", sValue)) Response.Location = sValue;
		else if (HeaderIs(sLine, "Content-Length", sValue)) iContentLength = atoll(sValue.c_str());
		else if (HeaderIs(sLine, "Transfer-Encoding", sValue)) bChunked = !strncasecmp(sValue.c_str(), "chunked", 7);
	}

	size_t iBody = iHeadersEnd + 4;

	if (bChunked) {
		while (true) {
			size_t iLineEnd = sAnswer.find("\r\n", iBody);
			if (iLineEnd == string::npos) break;

			size_t iChunk = strtoul(sAnswer.c_str() + iBody, nullptr, 16);
			iBody = iLineEnd + 2;

			if (!iChunk) return true;

			if (iChunk > sAnswer.length() - iBody || sAnswer.length() - iBody - iChunk < 2) break;

			Response.Content.append(sAnswer, iBody, iChunk);
			iBody += iChunk + 2;
		}

		m_sError = "truncated response";

		return false;
	}

	Response.Content = sAnswer.substr(iBody);

	if (iContentLength >= 0) {
		if ((long long)Response.Content.length() < iContentLength) {
			m_sError = "truncated response";

			return false;
		}

		Response.Content.resize(iContentLength);
	}

	return true;
}

bool CHTTP::Request(const CURL &URL, CResponseHTTP &Response) {
	if (!OpenConnection(URL)) return false;

	string sHost = URL.sHostName;
	if (URL.iPort != (URL.bHTTPS ? 443 : 80)) sHost += ":" + to_string(URL.iPort);

	string sQuery = "GET " + URL.Path + " HTTP/1.1\r\n";
	sQuery += "Host: " + sHost + "\r\n";
	sQuery += "User-Agent: " + m_sUserAgent + "\r\n";
	sQuery += "Accept: */*\r\n";
	sQuery += "Connection: close\r\n\r\n";

	string sAnswer;
	unsigned char buffer[4096];

	if (URL.bHTTPS) {
		LoadTrustAnchors();

		CTLS *pTLS = new CTLS;
		InitTLSClient(&pTLS->CC, &pTLS->XC);
		br_ssl_engine_set_buffer(&pTLS->CC.eng, pTLS->IOBuffer, sizeof(pTLS->IOBuffer), 1);

		if (!br_ssl_client_reset(&pTLS->CC, URL.sHostName.c_str(), 0)) {
			delete pTLS;
			Clean();

			m_sError = "TLS init failed";

			return false;
		}

		br_sslio_init(&pTLS->IOC, &pTLS->CC.eng, SockRead, &m_iTCPSock, SockWrite, &m_iTCPSock);

		if (!br_sslio_write_all(&pTLS->IOC, sQuery.c_str(), sQuery.length()) && !br_sslio_flush(&pTLS->IOC)) {
			int iRead;
			while ((iRead = br_sslio_read(&pTLS->IOC, buffer, sizeof(buffer))) > 0) {
				sAnswer.append((char *)buffer, iRead);
				if (sAnswer.length() > MAX_RESPONSE_SIZE) break;
			}
		}

		int iError = br_ssl_engine_last_error(&pTLS->CC.eng);

		delete pTLS;
		Clean();

		//Servers may close without close_notify, body length is checked in ParseResponse
		if (!sAnswer.length()) {
			const char *pError = GetTLSError(iError);
			m_sError = pError ? pError : "TLS error " + to_string(iError);

			return false;
		}
	}
	else {
		if (SockWrite(&m_iTCPSock, (const unsigned char *)sQuery.c_str(), sQuery.length()) != (int)sQuery.length()) {
			Clean();

			m_sError = "send failed";

			return false;
		}

		int iRead;
		while ((iRead = SockRead(&m_iTCPSock, buffer, sizeof(buffer))) > 0) {
			sAnswer.append((char *)buffer, iRead);
			if (sAnswer.length() > MAX_RESPONSE_SIZE) break;
		}

		Clean();
	}

	if (sAnswer.length() > MAX_RESPONSE_SIZE) {
		m_sError = "response too large";

		return false;
	}

	return ParseResponse(sAnswer, Response);
}

bool CHTTP::GET(const string &sUrl, string &sContent) {
	CURL URL;
	if (!URL.Parse(sUrl)) {
		m_sError = "bad url: " + sUrl;

		return false;
	}

	for (int i = 0; i <= MAX_REDIRECTS; i++) {
		CResponseHTTP Response;
		if (!Request(URL, Response)) return false;

		if (Response.Status == 200) {
			sContent.swap(Response.Content);

			return true;
		}

		bool bRedirect = Response.Status == 301 || Response.Status == 302 || Response.Status == 303 || Response.Status == 307 || Response.Status == 308;
		if (!bRedirect || !Response.Location.length()) {
			m_sError = "HTTP " + to_string(Response.Status) + " from " + URL.sHostName;

			return false;
		}

		string sLocation = Response.Location;
		if (!sLocation.compare(0, 2, "//")) sLocation.insert(0, URL.bHTTPS ? "https:" : "http:");
		else if (sLocation[0] == '/') sLocation.insert(0, (URL.bHTTPS ? "https://" : "http://") + URL.sHostName + ":" + to_string(URL.iPort));

		bool bWasHTTPS = URL.bHTTPS;
		if (!URL.Parse(sLocation) || (bWasHTTPS && !URL.bHTTPS)) {
			m_sError = "bad redirect: " + sLocation;

			return false;
		}
	}

	m_sError = "too many redirects";

	return false;
}
