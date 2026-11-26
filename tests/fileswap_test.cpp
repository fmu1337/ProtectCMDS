// Replaces a protectcmds .so that this process has loaded, the way the updater does, and checks that the
// loaded copy keeps working and the files on disk are what they should be.
//
//   g++ -m32 -I. -o fileswap_test tests/fileswap_test.cpp updater/fileswap.cpp helpers/sha256.cpp -ldl
//   LD_LIBRARY_PATH=<hl2sdk linux_sdk> ./fileswap_test old.so new.so <empty dir>

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <vector>

#include <updater/fileswap.h>
#include <helpers/sha256.h>

typedef uint32_t (*HashSteam_t)(uint32_t AccountID);

static bool ReadAll(const std::string &sPath, std::string &sContent) {
	FILE *pFile = fopen(sPath.c_str(), "rb");
	if (!pFile) return false;

	char szBuffer[65536];
	size_t iRead;
	sContent.clear();
	while ((iRead = fread(szBuffer, 1, sizeof(szBuffer), pFile)) > 0) sContent.append(szBuffer, iRead);
	fclose(pFile);

	return true;
}

static bool WriteAll(const std::string &sPath, const std::string &sContent) {
	FILE *pFile = fopen(sPath.c_str(), "wb");
	if (!pFile) return false;

	bool bWritten = fwrite(sContent.data(), 1, sContent.size(), pFile) == sContent.size();

	return !fclose(pFile) && bWritten;
}

static std::vector<uint32_t> Hashes(HashSteam_t HashSteam) {
	std::vector<uint32_t> Result;
	for (uint32_t i = 1; i <= 20000; i++) Result.push_back(HashSteam(i * 7919));

	return Result;
}

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s (line %d)\n", #x, __LINE__); return 1; } } while (0)

int main(int argc, char **argv) {
	if (argc < 4) {
		fprintf(stderr, "usage: %s <old.so> <new.so> <empty dir>\n", argv[0]);
		return 2;
	}

	std::string sOld, sNew;
	CHECK(ReadAll(argv[1], sOld));
	CHECK(ReadAll(argv[2], sNew));
	CHECK(sOld != sNew);

	std::string sPath = std::string(argv[3]) + "/protectcmds.ext.so";
	CHECK(WriteAll(sPath, sOld));

	void *pLib = dlopen(sPath.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (!pLib) fprintf(stderr, "dlopen: %s\n", dlerror());
	CHECK(pLib);

	std::string *pSalt = (std::string *)dlsym(pLib, "_Z7g_sSaltB5cxx11");
	HashSteam_t HashSteam = (HashSteam_t)dlsym(pLib, "_ZN15CHashingSteamID9HashSteamEj");
	CHECK(pSalt && HashSteam);
	*pSalt = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef";
	std::vector<uint32_t> Before = Hashes(HashSteam);

	// A bad checksum leaves everything as it was
	std::string sError;
	CHECK(!ReplaceFileAtomic(sPath.c_str(), sNew, std::string(64, '0'), sError));
	printf("bad checksum refused: %s\n", sError.c_str());
	std::string sOnDisk;
	CHECK(ReadAll(sPath, sOnDisk) && sOnDisk == sOld);
	CHECK(access((sPath + ".new").c_str(), F_OK) != 0);
	CHECK(access((sPath + ".prev").c_str(), F_OK) != 0);

	// The real update
	CHECK(ReplaceFileAtomic(sPath.c_str(), sNew, sha256(sNew), sError));
	CHECK(ReadAll(sPath, sOnDisk) && sOnDisk == sNew);
	CHECK(ReadAll(sPath + ".prev", sOnDisk) && sOnDisk == sOld);
	CHECK(access((sPath + ".new").c_str(), F_OK) != 0);

	// The loaded copy still runs the old code
	CHECK(Hashes(HashSteam) == Before);

	// A second update replaces .prev with the version that was current
	CHECK(ReplaceFileAtomic(sPath.c_str(), sOld, sha256(sOld), sError));
	CHECK(ReadAll(sPath, sOnDisk) && sOnDisk == sOld);
	CHECK(ReadAll(sPath + ".prev", sOnDisk) && sOnDisk == sNew);
	CHECK(Hashes(HashSteam) == Before);

	printf("OK: loaded library survived two updates, %zu hashes unchanged, .prev holds the previous file\n", Before.size());

	return 0;
}
