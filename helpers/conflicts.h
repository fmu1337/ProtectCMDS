#ifndef _INCLUDE_PROTECTCMDS_CONFLICTS_H_
#define _INCLUDE_PROTECTCMDS_CONFLICTS_H_

#include <stddef.h>

// Looks through the shared objects loaded into this process for one that conflicts with ProtectCMDS:
// ProcessCmds hooks the same engine functions, and the two crash when both are loaded.
// Returns true and copies the object's file name into pName when one is found.
bool FindConflictingModule(char *pName, size_t iMaxLength);

#endif // _INCLUDE_PROTECTCMDS_CONFLICTS_H_
