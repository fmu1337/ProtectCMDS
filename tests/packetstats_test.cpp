// Feeds fake packets through a built protectcmds .so's recvfrom hook and checks what it returns and counts:
// empty datagrams, split packets, errors with an old split header still in the buffer, normal packets,
// more addresses than the table holds, and the connectionless limit.
//
//   g++ -m32 -o packetstats_test tests/packetstats_test.cpp -ldl
//   LD_LIBRARY_PATH=<hl2sdk libs> ./packetstats_test protectcmds.ext.so css34|css

#include <arpa/inet.h>
#include <dlfcn.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <string>

typedef int (*RecvFrom_t)(int, char *, int, int, struct sockaddr *, int *);
typedef void (*Configure_t)(void *pThis, bool bEnabled, int iLogThreshold, int iLogInterval);
typedef void (*Reset_t)(void *pThis);
typedef void (*Print_t)(void *pThis, void *pConsole);
typedef bool (*IsConnectionlessFlood_t)(uint32_t iIP);

// Same layout as PCMDSRootConsole::IRootConsole (helpers/rootconsole.h); only ConsolePrint is used
class CFakeConsole {
	public:
		virtual unsigned int GetInterfaceVersion() { return 3; }
		virtual const char *GetInterfaceName() { return "IRootConsole"; }
		virtual bool IsVersionCompatible(unsigned int) { return true; }
		virtual bool AddRootConsoleCommand(const char *, const char *, void *) { return true; }
		virtual bool RemoveRootConsoleCommand(const char *, void *) { return true; }
		virtual void ConsolePrint(const char *pFormat, ...) {
			char szBuffer[1024];
			va_list ap;
			va_start(ap, pFormat);
			vsnprintf(szBuffer, sizeof(szBuffer), pFormat, ap);
			va_end(ap);
			m_sOutput += szBuffer;
			m_sOutput += "\n";
		}

	public:
		std::string m_sOutput;
};

static uint32_t g_iFakeIP;
static const char *g_pFakeData;
static int g_iFakeLength;
static int g_iFakeReturn;

static int FakeRecvFrom(int s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen) {
	if (g_iFakeReturn > 0) memcpy(buf, g_pFakeData, g_iFakeLength);

	struct sockaddr_in *pAddr = (struct sockaddr_in *)from;
	memset(pAddr, 0, sizeof(*pAddr));
	pAddr->sin_family = AF_INET;
	pAddr->sin_addr.s_addr = g_iFakeIP;

	return g_iFakeReturn;
}

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s (line %d)\n", #x, __LINE__); return 1; } } while (0)

static void *Symbol(void *pLib, const char *pName) {
	void *pSymbol = dlsym(pLib, pName);
	if (!pSymbol) fprintf(stderr, "missing symbol %s\n", pName);

	return pSymbol;
}

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "usage: %s <protectcmds.ext.so> css34|css\n", argv[0]);
		return 2;
	}

	bool bCSS34 = !strcmp(argv[2], "css34");

	void *pLib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
	if (!pLib) fprintf(stderr, "dlopen: %s\n", dlerror());
	CHECK(pLib);

	void *pStats = Symbol(pLib, "g_PacketStats");
	RecvFrom_t *pRealRecvFrom = (RecvFrom_t *)Symbol(pLib, "g_pRealRecvFrom");
	RecvFrom_t RecvFromHook = (RecvFrom_t)Symbol(pLib, "_Z12RecvFromHookiPciiP8sockaddrPi");
	Configure_t Configure = (Configure_t)Symbol(pLib, "_ZN12CPacketStats9ConfigureEbii");
	Reset_t Reset = (Reset_t)Symbol(pLib, "_ZN12CPacketStats5ResetEv");
	Print_t Print = (Print_t)Symbol(pLib, "_ZN12CPacketStats5PrintEPN16PCMDSRootConsole12IRootConsoleE");
	CHECK(pStats && pRealRecvFrom && RecvFromHook && Configure && Reset && Print);

	// Logging off: it needs SourceMod
	Configure(pStats, true, 0, 10);
	*pRealRecvFrom = &FakeRecvFrom;

	char szBuffer[2048];
	struct sockaddr_in From;
	int iFromLength = sizeof(From);

	// Empty datagram: one zero byte, counted
	g_iFakeIP = inet_addr("1.2.3.4");
	g_iFakeReturn = 0;
	for (int i = 0; i < 5; i++) {
		szBuffer[0] = 'x';
		CHECK(RecvFromHook(0, szBuffer, sizeof(szBuffer), 0, (struct sockaddr *)&From, &iFromLength) == 1);
		CHECK(szBuffer[0] == '\0');
	}

	// Normal connectionless packet: untouched
	static const char sQuery[] = "\xFF\xFF\xFF\xFFTSource Engine Query";
	g_iFakeIP = inet_addr("9.9.9.9");
	g_pFakeData = sQuery;
	g_iFakeLength = sizeof(sQuery);
	g_iFakeReturn = sizeof(sQuery);
	CHECK(RecvFromHook(0, szBuffer, sizeof(szBuffer), 0, (struct sockaddr *)&From, &iFromLength) == (int)sizeof(sQuery));
	CHECK(!memcmp(szBuffer, sQuery, sizeof(sQuery)));

	if (bCSS34) {
		// Split packet: dropped to one zero byte, counted
		static const char sSplit[] = "\xFE\xFF\xFF\xFF\x01\x02\x03\x04\x05\x06";
		g_iFakeIP = inet_addr("5.6.7.8");
		g_pFakeData = sSplit;
		g_iFakeLength = sizeof(sSplit);
		g_iFakeReturn = sizeof(sSplit);
		for (int i = 0; i < 3; i++) CHECK(RecvFromHook(0, szBuffer, sizeof(szBuffer), 0, (struct sockaddr *)&From, &iFromLength) == 1);

		// recvfrom error with an old split header left in the buffer: the error must come through
		memcpy(szBuffer, sSplit, sizeof(sSplit));
		g_iFakeReturn = -1;
		CHECK(RecvFromHook(0, szBuffer, sizeof(szBuffer), 0, (struct sockaddr *)&From, &iFromLength) == -1);
	}

	CFakeConsole Console;
	Print(pStats, &Console);
	printf("%s", Console.m_sOutput.c_str());
	CHECK(Console.m_sOutput.find(bCSS34 ? "Total: empty 5, split 3, connectionless flood 0" : "Total: empty 5, split 0, connectionless flood 0") != std::string::npos);
	CHECK(Console.m_sOutput.find("1.2.3.4") != std::string::npos);
	CHECK(Console.m_sOutput.find("9.9.9.9") == std::string::npos);
	CHECK(!bCSS34 || Console.m_sOutput.find("5.6.7.8") != std::string::npos);

	// More addresses than the table holds: oldest give way, nothing breaks
	g_iFakeReturn = 0;
	for (uint32_t i = 1; i <= 1000; i++) {
		g_iFakeIP = htonl(0x0A000000 + i);
		CHECK(RecvFromHook(0, szBuffer, sizeof(szBuffer), 0, (struct sockaddr *)&From, &iFromLength) == 1);
	}
	Console.m_sOutput.clear();
	Print(pStats, &Console);
	CHECK(Console.m_sOutput.find("Total: empty 1005") != std::string::npos);
	CHECK(Console.m_sOutput.find("The table is full") != std::string::npos);

	Reset(pStats);
	Console.m_sOutput.clear();
	Print(pStats, &Console);
	CHECK(Console.m_sOutput.find("Total: empty 0, split 0, connectionless flood 0") != std::string::npos);

	if (bCSS34) {
		int *pLimit = (int *)Symbol(pLib, "g_iConnectionlessLimit");
		IsConnectionlessFlood_t IsConnectionlessFlood = (IsConnectionlessFlood_t)Symbol(pLib, "_Z21IsConnectionlessFloodj");
		CHECK(pLimit && IsConnectionlessFlood);

		*pLimit = 3;
		uint32_t iIP = inet_addr("7.7.7.7");
		for (int i = 0; i < 3; i++) CHECK(!IsConnectionlessFlood(iIP));
		CHECK(IsConnectionlessFlood(iIP));
		CHECK(!IsConnectionlessFlood(inet_addr("8.8.8.8")));
	}

	printf("OK: %s\n", argv[1]);

	return 0;
}
