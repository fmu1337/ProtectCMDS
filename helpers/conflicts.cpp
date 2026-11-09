#include "conflicts.h"

#include <link.h>
#include <string.h>
#include <strings.h>

// File name prefixes of conflicting libraries. ProcessCmds ships as a SourceMod extension
// (processcmds.ext.1.ep1.so, processcmds.ext.so, ...) and as a Metamod plugin (processcmds_mm_i386.so).
static const char *g_sConflictPrefixes[] = {
	"processcmds"
};

struct ConflictSearch {
	char *pName;
	size_t iMaxLength;
};

static int FindConflict(struct dl_phdr_info *pInfo, size_t iSize, void *pData) {
	const char *pPath = pInfo->dlpi_name;
	if (!pPath || !pPath[0]) return 0;

	const char *pFile = strrchr(pPath, '/');
	pFile = pFile ? pFile + 1 : pPath;

	for (size_t i = 0; i < sizeof(g_sConflictPrefixes) / sizeof(g_sConflictPrefixes[0]); i++) {
		if (strncasecmp(pFile, g_sConflictPrefixes[i], strlen(g_sConflictPrefixes[i]))) continue;

		ConflictSearch *pSearch = (ConflictSearch *)pData;
		if (pSearch->pName && pSearch->iMaxLength) {
			strncpy(pSearch->pName, pFile, pSearch->iMaxLength - 1);
			pSearch->pName[pSearch->iMaxLength - 1] = '\0';
		}

		return 1;
	}

	return 0;
}

bool FindConflictingModule(char *pName, size_t iMaxLength) {
	ConflictSearch search = { pName, iMaxLength };

	return dl_iterate_phdr(FindConflict, &search) != 0;
}
