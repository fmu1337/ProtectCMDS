#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>

#include "gamestatsfix.h"

CDetour *g_pUpdateConnectionDetour = nullptr;

DETOUR_DECL_MEMBER0(UpdateConnection_hook, void) {
	return;
}

void CGameStatsFix::Load() {
	if (!g_pUpdateConnectionDetour) {
		/*
			"CUploadGameStats::UpdateConnection"
			{
				"library"	"engine"
				"linux"		"@_ZN16CUploadGameStats16UpdateConnectionEv"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN16CUploadGameStats16UpdateConnectionEv");
		g_pUpdateConnectionDetour = DETOUR_CREATE_MEMBER(UpdateConnection_hook, pAddrFunc);
		if (g_pUpdateConnectionDetour) g_pUpdateConnectionDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CUploadGameStats::UpdateConnection detour");
	}
}

void CGameStatsFix::Unload() {
	if (g_pUpdateConnectionDetour) {
		g_pUpdateConnectionDetour->Destroy();
		g_pUpdateConnectionDetour = nullptr;
	}
}