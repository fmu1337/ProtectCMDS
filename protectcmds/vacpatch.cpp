#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>
#include <tier0/icommandline.h>

#include "vacpatch.h"

CDetour *g_pSteamGameServerDetour = nullptr;
CDetour *g_pBSecureDetour = nullptr;

DETOUR_DECL_MEMBER0(BSecureHook, int) {
	return 1;
}

DETOUR_DECL_MEMBER0(SteamGameServerHook, void *) {
	void *pSteamGameServer = DETOUR_MEMBER_CALL(SteamGameServerHook)();
	if (!pSteamGameServer) return pSteamGameServer;
	
	static bool bInSecureInit = false;
	if (!bInSecureInit) {
		bInSecureInit = true;
		
		if (!CommandLine()->FindParm("-insecure") && !g_pBSecureDetour) {
			void *fn = reinterpret_cast<void ***>(pSteamGameServer)[0][12];
			g_pBSecureDetour = DETOUR_CREATE_MEMBER(BSecureHook, fn);
			if (g_pBSecureDetour) g_pBSecureDetour->EnableDetour();
			else g_pSM->LogError(myself, "failed detour to VAC Patch secure");
		}
	}
	
	return pSteamGameServer;
}

void CVACPatch::Load() {
	if (!g_pSteamGameServerDetour) {		
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::VALVE_API, "SteamGameServer");
		g_pSteamGameServerDetour = DETOUR_CREATE_MEMBER(SteamGameServerHook, pAddrFunc);
		if (g_pSteamGameServerDetour) g_pSteamGameServerDetour->EnableDetour();
		else g_pSM->LogError(myself, "failed detour to VAC Patch SGS");
	}
}

void CVACPatch::Unload() {
	if (g_pSteamGameServerDetour) {
		g_pSteamGameServerDetour->Destroy();
		g_pSteamGameServerDetour = nullptr;
	}
	
	if (g_pBSecureDetour) {
		g_pBSecureDetour->Destroy();
		g_pBSecureDetour = nullptr;
	}
}