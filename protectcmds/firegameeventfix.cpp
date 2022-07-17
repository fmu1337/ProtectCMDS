#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>

#include "firegameeventfix.h"

#include <igameevents.h>
#include <inetchannel.h>

extern char g_cPrefix[];

CDetour *g_pFireGameEventDetour = nullptr;

DETOUR_DECL_MEMBER1(FireGameEvent, void, IGameEvent *, pEvent) {
	INetChannel *pNetChannel = (INetChannel *)*((uintptr_t *)this + 51);
	if (!pNetChannel) {
		ConMsg(0, "%s prevented server crash with CBaseClient::FireGameEvent nullptr m_NetChannel\n", g_cPrefix);
		return;
	}
	
	return DETOUR_MEMBER_CALL(FireGameEvent)(pEvent);
}

void CFireGameEventFix::Load() {
	if (!g_pFireGameEventDetour) {
		/*
			"CBaseClient::FireGameEvent"
			{
				"library"		"engine"
				"linux"		"@_ZN11CBaseClient13FireGameEventEP10IGameEvent"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN11CBaseClient13FireGameEventEP10IGameEvent");
		g_pFireGameEventDetour = DETOUR_CREATE_MEMBER(FireGameEvent, pAddrFunc);
		if (g_pFireGameEventDetour) g_pFireGameEventDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CBaseClient::FireGameEvent detour");
	}
}

void CFireGameEventFix::Unload() {
	if (g_pFireGameEventDetour) {
		g_pFireGameEventDetour->Destroy();
		g_pFireGameEventDetour = nullptr;
	}
}