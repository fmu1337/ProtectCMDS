// FindConflictingModule must see ProcessCmds' libraries (SourceMod extension and Metamod plugin) while they are
// loaded, and nothing else - in particular not ProtectCMDS itself.
//
//   gcc -m32 -shared -fPIC -o <dir>/processcmds.ext.1.ep1.so tests/conflicts_dummy.c   (same for the names below)
//   g++ -m32 -I. -o conflicts_test tests/conflicts_test.cpp helpers/conflicts.cpp -ldl
//   ./conflicts_test <dir>
//
// <dir> holds copies of the dummy as processcmds.ext.1.ep1.so, PROCESSCMDS_MM_I386.so and protectcmds.ext.so.

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include <helpers/conflicts.h>

static int g_iFailed = 0;

static void Check(bool bOk, const char *pWhat) {
	printf("%s: %s\n", bOk ? "ok" : "FAIL", pWhat);
	if (!bOk) g_iFailed++;
}

static void *Open(const std::string &sPath) {
	void *pHandle = dlopen(sPath.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (!pHandle) printf("dlopen %s: %s\n", sPath.c_str(), dlerror());

	return pHandle;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("usage: %s <dir with the dummy libraries>\n", argv[0]);
		return 2;
	}

	std::string sDir = argv[1];
	char szName[256];

	Check(!FindConflictingModule(szName, sizeof(szName)), "nothing found before anything is loaded");

	void *pOwn = Open(sDir + "/protectcmds.ext.so");
	Check(pOwn && !FindConflictingModule(szName, sizeof(szName)), "protectcmds.ext.so is not a conflict");

	void *pExt = Open(sDir + "/processcmds.ext.1.ep1.so");
	Check(pExt && FindConflictingModule(szName, sizeof(szName)), "processcmds.ext.1.ep1.so is found");
	Check(!strcmp(szName, "processcmds.ext.1.ep1.so"), "the file name is reported without its directory");

	char szShort[5];
	Check(FindConflictingModule(szShort, sizeof(szShort)) && !strcmp(szShort, "proc"), "a short buffer is cut and terminated");
	Check(FindConflictingModule(NULL, 0), "a NULL buffer only reports the result");

	if (pExt) dlclose(pExt);
	Check(!FindConflictingModule(szName, sizeof(szName)), "nothing found after ProcessCmds is unloaded");

	void *pMM = Open(sDir + "/PROCESSCMDS_MM_I386.so");
	Check(pMM && FindConflictingModule(szName, sizeof(szName)) && !strcmp(szName, "PROCESSCMDS_MM_I386.so"),
		"the Metamod plugin is found, letter case ignored");

	if (pMM) dlclose(pMM);
	if (pOwn) dlclose(pOwn);

	printf(g_iFailed ? "%d check(s) failed\n" : "all checks passed\n", g_iFailed);

	return g_iFailed ? 1 : 0;
}
