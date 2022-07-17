#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <string>
#include <cstring>
#include <iostream>
#include <regex>

#include "http.h"

using namespace std;

CHTTP::CHTTP() {
	//Addr
	m_TCPAddr.sin_family = AF_INET;
}

CHTTP::~CHTTP() {
	Clean();
}

void CHTTP::Clean() {
	if (m_iTCPSock != -1) {
		close(m_iTCPSock);
		m_iTCPSock = -1;
	}

	if (m_pAddrInfo) {
		freeaddrinfo(m_pAddrInfo);
		m_pAddrInfo = nullptr;
	}
}

void CHTTP::SetTimeout(int iTimeout) {
	m_iTimeout = iTimeout;
}

CHOST CHTTP::GetHost(char *pUrl) {
	regex RegExpr("(http[s]?)?(:\\/\\/)?(?:[^@\\n]+@)?(?:www\\.)?([^:\\/\\n?]+)\\:?(\\d+)?");
	smatch sMatch;

	CHOST HOST;
	string sUrl = pUrl;

	if (regex_search(sUrl, sMatch, RegExpr)) {
		if (!strcmp(sMatch[1].str().c_str(), "https")) HOST.iPort = 443;

		HOST.sHostName = sMatch[3].str();

		int iPort = atoi(sMatch[4].str().c_str());
		if (iPort) HOST.iPort = iPort;

		HOST.Path = sMatch.suffix().str().length() ? sMatch.suffix().str() : "/";
	}

	return HOST;
}

void CHTTP::AddHeader(char *pHeader) {
	m_sHeaders.append(pHeader);
	m_sHeaders.append("\r\n");
}

bool CHTTP::ResolveHost(CHOST HOST) {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));

	hints.ai_family		= AF_INET;
	hints.ai_socktype 	= SOCK_STREAM;
	hints.ai_flags		= AI_PASSIVE;
	
	char sNum[5];
	sprintf(sNum, "%d", HOST.iPort);
	
	if (getaddrinfo(HOST.sHostName.c_str(), sNum, &hints, &m_pAddrInfo) != 0) return false;

	return true;
}

bool CHTTP::OpenConnection(CHOST HOST) {
	m_iTCPSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (fcntl(m_iTCPSock, F_SETFL, O_NONBLOCK) == -1) {
		cout << "error fcntl" << endl;

		return false;
	}

	if (m_iTimeout != -1) {
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = m_iTimeout * 1000;
		if (setsockopt(m_iTCPSock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
			cout << "error setsockopt recv" << endl;

			return false;
		}
	}

	m_TCPAddr.sin_port = htons(HOST.iPort);
	m_TCPAddr.sin_addr.s_addr = ((struct sockaddr_in *)(m_pAddrInfo->ai_addr))->sin_addr.s_addr;

	if (connect(m_iTCPSock, (struct sockaddr *)&m_TCPAddr, sizeof(m_TCPAddr)) < 0) {
		fd_set fdset;
		FD_ZERO(&fdset);
		FD_SET(m_iTCPSock, &fdset);
		struct timeval timeout;
		timeout.tv_sec = 3;
		timeout.tv_usec = 0;

		if (select(m_iTCPSock+1, nullptr, &fdset, nullptr, &timeout) == -1) {
			cout << "error select connect" << endl;

			return false;
		}

		int iError = 0;
		socklen_t iLen = sizeof(iError);
		getsockopt(m_iTCPSock, SOL_SOCKET, SO_ERROR, &iError, &iLen);

		if (iError) {
			cout << "Error connect: timeout " << iError << endl;
			return false;
		}
	}

	return true;
}

CResponseHTTP CHTTP::GET(char *pUrl) {
	CResponseHTTP Response;

	CHOST HOST = GetHost(pUrl);

	if (!ResolveHost(HOST)) {
		cout << "can't resolve: " << HOST.sHostName << endl;

		Clean();
		return Response;
	}

	if (!OpenConnection(HOST)) {
		cout << "error connect" << endl;
		
		Clean();
		return Response;
	}

	freeaddrinfo(m_pAddrInfo);
	m_pAddrInfo = nullptr;

	string sQuery = "GET ";
	sQuery.append(HOST.Path);
	sQuery += m_sHeaders + "\r\n";

	int iFlags = fcntl(m_iTCPSock, F_GETFL, 0);
	iFlags &= ~O_NONBLOCK;

	if (fcntl(m_iTCPSock, F_SETFL, iFlags) == -1) {
		cout << "error fcntl to blocking" << endl;

		Clean();
		return Response;
	}
	
	int iLen = send(m_iTCPSock, sQuery.c_str(), sQuery.length(), 0);
	if (iLen < 0) {
		cout << "Error write" << endl;
		
		Clean();
		return Response;
	}

	string sAnswer;
	char buffer[1500];
	int iLength = -1;
	do {
		iLength = recv(m_iTCPSock, buffer, sizeof(buffer), 0);
		if (!iLength) break;
		
		if (iLength < 0) {
			cout << "err read" << endl;

			Clean();
			return Response;
		}
		
		sAnswer.append(buffer, iLength);
	}
	while (iLength >= 0);
	Clean();

	//Get content (skip headers)
	char *pAddress = (char *)sAnswer.c_str();
	char *pEndAddress = pAddress + sAnswer.length();
	
	while (true) {
		if (pAddress >= pEndAddress) break;
		
		//0x0D = \r | 0x0A = \n
		if (*pAddress++ != 0x0D) continue;
		
		if (*pAddress++ != 0x0A) continue;
		
		if (*pAddress++ != 0x0D) continue;
		
		if (*pAddress++ != 0x0A) continue;
		
		break;
	}

	iLength = pEndAddress - pAddress;
	if (iLength > 0) Response.Content.append(pAddress, iLength);
	
	return Response;
}

