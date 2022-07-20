#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>

#include "netmessages.h"
#include "iclient.h"
#include "igameevents.h"


#include "sourcetvfix.h"

extern char g_cPrefix[];
 
CDetour *g_pProcessClientInfoDetour = nullptr;

class CBaseClient : public IGameEventListener2, public IClient {};

DETOUR_DECL_MEMBER1(ProcessClientInfo, bool, CLC_ClientInfo *, msg) {
	if (msg->m_bIsHLTV) {
		CBaseClient *pClient = reinterpret_cast<CBaseClient*>(this);
		//int iClient = pClient->GetPlayerSlot() + 1;
		const char *pName = pClient->GetClientName();
		
		ConMsg(0, "%s %s attempted to kick SourceTV (prevented)\n", g_cPrefix, pName);
		
		msg->m_bIsHLTV = false;
	}
	
	return DETOUR_MEMBER_CALL(ProcessClientInfo)(msg);
}

void CSourceTVFix::Load() {
	if (!g_pProcessClientInfoDetour) {
		/*
			"CGameClient::ProcessClientInfo"
			{
				"library"		"engine"
				"linux"		"@_ZN11CGameClient17ProcessClientInfoEP14CLC_ClientInfo"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN11CGameClient17ProcessClientInfoEP14CLC_ClientInfo");
		g_pProcessClientInfoDetour = DETOUR_CREATE_MEMBER(ProcessClientInfo, pAddrFunc);
		if (g_pProcessClientInfoDetour) g_pProcessClientInfoDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CGameClient::ProcessClientInfo detour");
	}
}

void CSourceTVFix::Unload() {
	if (g_pProcessClientInfoDetour) {
		g_pProcessClientInfoDetour->Destroy();
		g_pProcessClientInfoDetour = nullptr;
	}
}