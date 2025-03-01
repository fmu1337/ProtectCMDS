#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>

#include "namefix.h"

#include "netmessages.h"
#include <igameevents.h>
#include <iclient.h>
#include <tier1/KeyValues.h>
#include <iplayerinfo.h>

#include <iostream>

#define TIME_OUT 120
#define CHECK_TIME 90

using namespace std;

extern char g_cPrefix[];
extern CGlobalVars* g_pGlobals;
extern IPlayerInfoManager* g_pPlayerInfoManager;

class CBaseClient : public IGameEventListener2, public IClient {};

CDetour *g_pProcessSetConVarDetour = nullptr;

struct NameFlood {
	int iUserID;
	float fLastTime = 0.0;
};

list<NameFlood *> g_UserIDList;

DETOUR_DECL_MEMBER1(ProcessSetConVarHook, bool, NET_SetConVar *, msg) {
	KeyValues *pConVars = (KeyValues *)*((uintptr_t *)this + 37);

	char szOldName[64];
	Q_strncpy(szOldName, (char *)pConVars->GetString("name"), sizeof(szOldName));
	
	for ( int i=0; i<msg->m_ConVars.Count(); i++ ) {
		const char *name = msg->m_ConVars[i].name;
		const char *value = msg->m_ConVars[i].value;
		pConVars->SetString(name, value);
	}
	
	bool *pBConVarsChanged = (bool *)((uintptr_t)this + 152);
	*pBConVarsChanged = 1;

	if(strcmp(szOldName, pConVars->GetString("name")) != 0) {
		CBaseClient *pBaseClient = reinterpret_cast<CBaseClient *>(this);
		
		int iUserID = pBaseClient->GetUserID();
		
		if (IsFloodNames(iUserID)) {
			pBaseClient->ClientPrintf("%s name change not allowed (block by time)\n", g_cPrefix);
			pConVars->SetString("name", szOldName);
			
			return true;
		}
		
		IPlayerInfo *pInfoPlayer = g_pPlayerInfoManager->GetPlayerInfo(gamehelpers->EdictOfIndex(pBaseClient->GetPlayerSlot()+1));
		
		if (!pInfoPlayer) return true;
		
		if (pInfoPlayer->IsDead()) {
			pBaseClient->ClientPrintf("%s name change not allowed (dead)\n", g_cPrefix);
			pConVars->SetString("name", szOldName);
		}
	}
	
	return true;
}

bool IsFloodNames(int iUserID) {
	float fTime = g_pGlobals->curtime;
	NameFlood *pNameFlood = nullptr;
	
	for (auto iter = g_UserIDList.begin(); iter != g_UserIDList.end(); ++iter){
		pNameFlood = *iter;
		//Check timed out
		if (pNameFlood->fLastTime + TIME_OUT < fTime) {
			delete pNameFlood;
			iter = g_UserIDList.erase(iter);
			 
			continue;
		}
		
		if (pNameFlood->iUserID == iUserID) {
			if (pNameFlood->fLastTime + CHECK_TIME > fTime) {
				pNameFlood->fLastTime = fTime;
				
				return true;
			}
			
			pNameFlood->fLastTime = fTime;
			
			return false;
		}
	}
	
	pNameFlood = new NameFlood;
	pNameFlood->iUserID = iUserID;
	pNameFlood->fLastTime = fTime;
	g_UserIDList.push_back(pNameFlood);
	
	return false;
}

void CNameFix::Load() {
	if (!g_pProcessSetConVarDetour) {
		/*
			"CBaseClient::ProcessSetConVar"
			{
				"library"		"engine"
				"linux"		"@_ZN11CBaseClient16ProcessSetConVarEP13NET_SetConVar"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN11CBaseClient16ProcessSetConVarEP13NET_SetConVar");
		g_pProcessSetConVarDetour = DETOUR_CREATE_MEMBER(ProcessSetConVarHook, pAddrFunc);
		if (g_pProcessSetConVarDetour) g_pProcessSetConVarDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CBaseClient::ProcessSetConVar detour");
	}
}

void CNameFix::Unload() {
	if (g_pProcessSetConVarDetour) {
		g_pProcessSetConVarDetour->Destroy();
		g_pProcessSetConVarDetour = nullptr;
	}
}