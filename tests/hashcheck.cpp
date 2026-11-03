// Prints the hashed SteamID of a protectcmds .so for fixed test salts and account IDs,
// so CI can check that two builds hash byte-identically. With --api it prints the
// extension API version the .so reports to SourceMod instead.
//
//   g++ -m32 -o hashcheck tests/hashcheck.cpp -ldl
//   LD_LIBRARY_PATH=<hl2sdk linux_sdk> ./hashcheck protectcmds.ext.so > out.txt
//   LD_LIBRARY_PATH=<hl2sdk linux_sdk> ./hashcheck protectcmds.ext.so --api

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string>

typedef uint32_t (*HashSteam_t)(uint32_t AccountID);
typedef void *(*GetSMExtAPI_t)();
// IExtensionInterface::GetExtensionVersion(), the first virtual of the interface (i386 GCC: this on the stack)
typedef unsigned int (*GetExtensionVersion_t)(void *pThis);

static const char alphanum[] =
	"0123456789"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz";

static uint32_t g_iSeed;

static uint32_t Next() {
	g_iSeed = g_iSeed * 1103515245u + 12345u;
	return g_iSeed;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <protectcmds.ext.so> [--api]\n", argv[0]);
		return 2;
	}

	void *pLib = dlopen(argv[1], RTLD_LAZY | RTLD_LOCAL);
	if (!pLib) {
		fprintf(stderr, "dlopen: %s\n", dlerror());
		return 1;
	}

	if (argc > 2 && std::string(argv[2]) == "--api") {
		GetSMExtAPI_t GetSMExtAPI = (GetSMExtAPI_t)dlsym(pLib, "GetSMExtAPI");
		void *pAPI = GetSMExtAPI ? GetSMExtAPI() : nullptr;
		if (!pAPI) {
			fprintf(stderr, "GetSMExtAPI not found\n");
			return 1;
		}

		GetExtensionVersion_t GetExtensionVersion = (*(GetExtensionVersion_t **)pAPI)[0];
		printf("%u\n", GetExtensionVersion(pAPI));
		return 0;
	}

	std::string *pSalt =(std::string *)dlsym(pLib, "g_sSalt");
	HashSteam_t HashSteam = (HashSteam_t)dlsym(pLib, "_ZN15CHashingSteamID9HashSteamEj");
	if (!pSalt || !HashSteam) {
		fprintf(stderr, "dlsym: g_sSalt=%p HashSteam=%p\n", (void *)pSalt, (void *)HashSteam);
		return 1;
	}

	// Test salts only: the same shape as a generated salt file (32 alphanumeric chars), plus odd ones
	std::string sSalts[8] = { "", "0", "salt", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef" };
	g_iSeed = 1337;
	for (int i = 4; i < 8; i++) {
		for (int j = 0; j < 32; j++) sSalts[i] += alphanum[Next() % (sizeof(alphanum) - 1)];
	}

	for (int i = 0; i < 8; i++) {
		*pSalt = sSalts[i];
		printf("salt %d [%s]\n", i, sSalts[i].c_str());

		g_iSeed = 42 + i;
		for (uint32_t n = 0; n < 20000; n++) {
			uint32_t iAccountID = n < 4096 ? n + 1 : Next();
			if (!iAccountID) continue;

			// Same as SetSteamID in protectcmds/hashingsteamid.cpp
			uint32_t iHashed = HashSteam(iAccountID);
			if (!(iAccountID & 1)) iHashed--;

			printf("%u %u\n", iAccountID, iHashed);
		}
	}

	return 0;
}
