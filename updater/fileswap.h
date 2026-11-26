#ifndef _FILESWAP_H_
#define _FILESWAP_H_

#include <string>

// Replaces a file that a running process may have loaded (protectcmds.ext.so) without touching the loaded copy:
// writes <path>.new and syncs it, checks its sha256, keeps the current file as <path>.prev and rename()s
// <path>.new over <path>. The running process keeps the old inode mapped until it restarts.
// No SourceMod dependencies, so tests/fileswap_test.cpp can use it too.
bool ReplaceFileAtomic(const char *pPath, const std::string &sContent, const std::string &sSHA256, std::string &sError);

#endif
