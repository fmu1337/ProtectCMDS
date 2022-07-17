#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>

#include "namefix.h"

#include "netmessages.h"
#include <igameevents.h>
#include <iclient.h>
#include <tier1/KeyValues.h>
#include <iostream>

#define TIME_OUT 60
#define CHECK_TIME 30

using namespace std;

extern char g_cPrefix[];
extern CGlobalVars* g_pGlobals;

class CBaseClient : public IGameEventListener2, public IClient {};

CDetour *g_pProcessSetConVarDetour = nullptr;

struct NameFlood {
	int iUserID;
	float fLastTime = 0.0;
};

list<NameFlood *> g_UserIDList;

bool IsFloodNames(int iUserID) {
	float fTime = g_pGlobals->realtime;
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
				//if (pPacketCount->iCountPacket >= iMaxPackets) return true;
				return true;
			}
			else {
				pNameFlood->fLastTime = fTime;
			}
			
			return false;
		}
	}
	
	pNameFlood = new NameFlood;
	pNameFlood->iUserID = iUserID;
	pNameFlood->fLastTime = fTime;
	g_UserIDList.push_back(pNameFlood);
	
	return false;
}

DETOUR_DECL_MEMBER1(ProcessSetConVarHook, bool, NET_SetConVar *, msg) {
	/*for ( int i=0; i<msg->m_ConVars.Count(); i++ ) {
		bool bAdd = true;
		
		const char *name = msg->m_ConVars[i].name;
		const char *value = msg->m_ConVars[i].value;
		
		if (!strcmp(name, "name")) {
			CBaseClient *pBaseClient = reinterpret_cast<CBaseClient *>(this);
			int iUserID = pBaseClient->GetUserID();
			if (IsFloodNames(iUserID)) bAdd = false;
		}
		
		if (!bAdd) continue;
		
		KeyValues *pConVars = (KeyValues *)*((uintptr_t *)this + 37);
		pConVars->SetString(name, value);
	}
	
	bool *pBConVarsChanged = (bool *)((uintptr_t)this + 152);
		
	*pBConVarsChanged = 1;
	
	return true;	
	//return DETOUR_MEMBER_CALL(ProcessSetConVarHook)(msg);*/
	
	KeyValues *pConVars = (KeyValues *)*((uintptr_t *)this + 37);
	
	char* pName;
	char szOldName[64];
	pName = (char *)pConVars->GetString("name");
	strncpy(szOldName, pName, sizeof(szOldName));
	
	bool bRet = DETOUR_MEMBER_CALL(ProcessSetConVarHook)(msg);
	
	pName = (char *)pConVars->GetString("name");
	
	if(strcmp(szOldName, pName) != 0)
	{
		CBaseClient *pBaseClient = reinterpret_cast<CBaseClient *>(this);
		int iUserID = pBaseClient->GetUserID();
		if (IsFloodNames(iUserID))
		{
			pConVars->SetString("name", szOldName);
		}
	}
	
	return bRet;
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