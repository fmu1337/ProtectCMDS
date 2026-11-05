#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "fileswap.h"
#include <helpers/sha256.h>

using namespace std;

static bool SetError(string &sError, const string &sWhat, const string &sPath) {
	sError = sWhat + " " + sPath + ": " + strerror(errno);

	return false;
}

static bool WriteAll(int iFile, const char *pData, size_t iLength) {
	while (iLength > 0) {
		ssize_t iWritten = write(iFile, pData, iLength);
		if (iWritten < 0) {
			if (errno == EINTR) continue;

			return false;
		}

		pData += iWritten;
		iLength -= iWritten;
	}

	return true;
}

static bool ReadFile(const string &sPath, string &sContent) {
	FILE *pFile = fopen(sPath.c_str(), "rb");
	if (!pFile) return false;

	char szBuffer[65536];
	size_t iRead;
	sContent.clear();
	while ((iRead = fread(szBuffer, 1, sizeof(szBuffer), pFile)) > 0) sContent.append(szBuffer, iRead);

	bool bError = ferror(pFile);
	fclose(pFile);

	return !bError;
}

// Hard link keeps the old inode under a second name; copy only where links are not supported
static bool KeepPrevious(const string &sPath, const string &sPrev, string &sError) {
	if (access(sPath.c_str(), F_OK)) return true;

	if (unlink(sPrev.c_str()) && errno != ENOENT) return SetError(sError, "Failed to remove", sPrev);

	if (!link(sPath.c_str(), sPrev.c_str())) return true;

	string sOld;
	if (!ReadFile(sPath, sOld)) return SetError(sError, "Failed to read", sPath);

	int iFile = open(sPrev.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (iFile < 0) return SetError(sError, "Failed to create", sPrev);

	bool bWritten = WriteAll(iFile, sOld.data(), sOld.length());
	if (close(iFile)) bWritten = false;

	if (!bWritten) return SetError(sError, "Failed to write", sPrev);

	return true;
}

bool ReplaceFileAtomic(const char *pPath, const string &sContent, const string &sSHA256, string &sError) {
	string sPath = pPath;
	string sNew = sPath + ".new";
	string sPrev = sPath + ".prev";

	int iFile = open(sNew.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (iFile < 0) return SetError(sError, "Failed to create", sNew);

	bool bWritten = WriteAll(iFile, sContent.data(), sContent.length()) && !fsync(iFile);
	if (close(iFile)) bWritten = false;

	if (!bWritten) {
		SetError(sError, "Failed to write", sNew);
		unlink(sNew.c_str());

		return false;
	}

	// What is on disk, not what was downloaded, is what the next start loads
	string sWritten;
	if (!ReadFile(sNew, sWritten) || sha256(sWritten) != sSHA256) {
		sError = "Checksum mismatch after writing " + sNew;
		unlink(sNew.c_str());

		return false;
	}

	if (!KeepPrevious(sPath, sPrev, sError)) {
		unlink(sNew.c_str());

		return false;
	}

	if (rename(sNew.c_str(), sPath.c_str())) {
		SetError(sError, "Failed to rename to", sPath);
		unlink(sNew.c_str());

		return false;
	}

	// Make the rename itself durable
	string sDir = sPath.substr(0, sPath.find_last_of('/') + 1);
	int iDir = open(sDir.empty() ? "." : sDir.c_str(), O_RDONLY);
	if (iDir >= 0) {
		fsync(iDir);
		close(iDir);
	}

	return true;
}
