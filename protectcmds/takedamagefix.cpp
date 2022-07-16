#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>

#include "takedamagefix.h"

extern char g_cPrefix[];

CDetour *g_pTakeDamageDetour = nullptr;

static void **g_pGameRules = nullptr;

class CTakeDamageInfo {};

DETOUR_DECL_MEMBER1(TakeDamage, void *, CTakeDamageInfo &, inputInfo) {
	if ( !(*g_pGameRules) ) {
		ConMsg(0, "%s prevented server crash with CBaseEntity::TakeDamage nullptr g_pGameRules\n", g_cPrefix);
		return nullptr;
	}
	
	return DETOUR_MEMBER_CALL(TakeDamage)(inputInfo);
}

void CTakeDamageFix::Load() {
	if (!g_pTakeDamageDetour) {
		/*
			"CBaseEntity::TakeDamage"
			{
				"library"		"server"
				"linux"		"@_ZN11CBaseEntity10TakeDamageERK15CTakeDamageInfo"
			}
			"g_pGameRules"
			{
				"library"    "server"
				"linux"        "@g_pGameRules"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::SERVER, "_ZN11CBaseEntity10TakeDamageERK15CTakeDamageInfo");
		g_pTakeDamageDetour = DETOUR_CREATE_MEMBER(TakeDamage, pAddrFunc);
		if (g_pTakeDamageDetour) g_pTakeDamageDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CBaseEntity::TakeDamage detour");
		
		g_pGameRules = reinterpret_cast<void **>(g_pSymbolManager->Resolve(ESymbolModule::SERVER, "g_pGameRules"));
	}
}

void CTakeDamageFix::Unload() {
	if (g_pTakeDamageDetour) {
		g_pTakeDamageDetour->Destroy();
		g_pTakeDamageDetour = nullptr;
	}
}