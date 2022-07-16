#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>

#include "gameuifix.h"

extern char g_cPrefix[];
 
CDetour *g_pGameeUIHookDetour = nullptr;

DETOUR_DECL_MEMBER1(GameUI, void *, CBaseEntity*, pActivator) {
	CBaseHandle *m_player_hndl = (CBaseHandle*)( (uintptr_t)this + 1316);

	if(!m_player_hndl->IsValid()) {
		ConMsg(0, "%s prevented server crash with CGameUI::Deactivate\n", g_cPrefix);
		return nullptr;
	}
	
	return DETOUR_MEMBER_CALL(GameUI)(pActivator);
}

void CGameUIFix::Load() {
	if (!g_pGameeUIHookDetour) {
		/*
			"CGameUI::Deactivate"
			{
				"library"		"server"
				"linux"		"@_ZN7CGameUI10DeactivateEP11CBaseEntity"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::SERVER, "_ZN7CGameUI10DeactivateEP11CBaseEntity");
		g_pGameeUIHookDetour = DETOUR_CREATE_MEMBER(GameUI, pAddrFunc);
		if (g_pGameeUIHookDetour) g_pGameeUIHookDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup GameUI::Deactivate detour");
	}
}

void CGameUIFix::Unload() {
	if (g_pGameeUIHookDetour) {
		g_pGameeUIHookDetour->Destroy();
		g_pGameeUIHookDetour = nullptr;
	}
}