#include "blockloadplugin.h"

#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>
#include <helpers/strtools.h>

extern char g_cPrefix[];
 
CDetour *g_pServerCommandDetour = nullptr;

DETOUR_DECL_MEMBER1(ServerCommand, void *, const char *, cBuffer) {
	if ( StrContains(cBuffer, "sm plugins load", false) == 0 
		|| StrContains(cBuffer, "sm plugins reload", false) == 0 
		|| StrContains(cBuffer, "sm exts load", false) == 0 
		|| StrContains(cBuffer, "sm exts reload", false) == 0) {
		if (StrContains(cBuffer, "..", false) != -1 ||
			StrContains(cBuffer, ".dat", false) != -1) {
			
			ConMsg(0, "%s prevented plugin/extension loading from outside the folder [RCON]\n", g_cPrefix);
		
			return nullptr;
		}
	}
	
	return DETOUR_MEMBER_CALL(ServerCommand)(cBuffer);
}

void CBlockLoadPluginFix::Load() {
	if (!g_pServerCommandDetour) {
		/*
			"CVEngineServer::ServerCommand"
			{
				"library"    "engine"
				"linux"        "@_ZN14CVEngineServer13ServerCommandEPKc"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN14CVEngineServer13ServerCommandEPKc");
		g_pServerCommandDetour = DETOUR_CREATE_MEMBER(ServerCommand, pAddrFunc);
		if (g_pServerCommandDetour) g_pServerCommandDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CVEngineServer::ServerCommand detour");
	}
}

void CBlockLoadPluginFix::Unload() {
	if (g_pServerCommandDetour) {
		g_pServerCommandDetour->Destroy();
		g_pServerCommandDetour = nullptr;
	}		
}