#include <CDetour/detours.h>

#include "osfix.h"

extern char g_cPrefix[];

CDetour *g_pMemcpyDetour = nullptr;

DETOUR_DECL_STATIC3(memcpy_hook, void *, void *, pDest, const void *, pSrc, size_t, iCount) {
	return memmove(pDest, pSrc, iCount);
}

void COSFix::Load() {
	if (!g_pMemcpyDetour) {
		g_pMemcpyDetour = DETOUR_CREATE_STATIC(memcpy_hook, (void*)memcpy);
		if (g_pMemcpyDetour) {
			g_pMemcpyDetour->EnableDetour();
			ConMsg(0, "%s OsFix Buffer Activated\n", g_cPrefix);
		}
		else g_pSM->LogError(myself, "Failed to setup memcpy static function detour");
	}
}

void COSFix::Unload() {
	if (g_pMemcpyDetour) {
		g_pMemcpyDetour->Destroy();
		g_pMemcpyDetour = nullptr;
	}
}