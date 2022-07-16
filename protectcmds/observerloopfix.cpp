#include <iostream>

#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>

#include "observerloopfix.h"

using namespace std;

extern CGlobalVars* g_pGlobals;
 
CDetour *g_pFindNextObserverTargetDetor = nullptr;

int (__cdecl* GetNextObserverSearchStartPoint)(void *CBasePlayerThis, bool bReverse);
bool (__cdecl* IsValidObserverTarget)(void *CBasePlayerThis, CBaseEntity *target);
CBaseEntity *(__cdecl* UTIL_PlayerByIndex)(int index);

DETOUR_DECL_MEMBER1(FindNextObserverTargetHook, CBaseEntity *, bool, bReverse) {
	int startIndex = GetNextObserverSearchStartPoint(this, bReverse);

    // [jason] Force this value into the valid client index range, in case we were previously
    //    spectating something that isn't a player.  This prevents the do..while loop below
    //    from getting in an infinite loop because of an invalid boundary condition
	if (startIndex > g_pGlobals->maxClients) startIndex = 1;
	else if (startIndex < 1) startIndex = g_pGlobals->maxClients;
	
	int    currentIndex = startIndex;
	int iDir = bReverse ? -1 : 1; 
	
	//cout << "зашел в цикл" << endl;
    
	do {
		CBaseEntity *nextTarget = UTIL_PlayerByIndex(currentIndex);

		if (IsValidObserverTarget(this, nextTarget)) {
			//cout << "вышел из цикла nextTarget" << endl;
			return nextTarget;    // found next valid player
		}

		currentIndex += iDir;

		// Loop through the clients
		if (currentIndex > g_pGlobals->maxClients) currentIndex = 1;
		else if (currentIndex < 1) currentIndex = g_pGlobals->maxClients;
	}
	while ( currentIndex != startIndex );
	
	//cout << "вышел из цикла NULL" << endl;

	return nullptr;
}

void CObServerLoopFix::Load() {
	/*
		"CBasePlayer::FindNextObserverTarget"
		{
			"library"		"server"
			"linux"		"@_ZN11CBasePlayer22FindNextObserverTargetEb"
		}
		"CBasePlayer::GetNextObserverSearchStartPoint"
		{
			"library"		"server"
			"linux"		"@_ZN11CBasePlayer31GetNextObserverSearchStartPointEb"
		}
		"CBasePlayer::IsValidObserverTarget"
		{
			"library"	"server"
			"linux"		"@_ZN11CBasePlayer21IsValidObserverTargetEP11CBaseEntity"
		}
		"UTIL_PlayerByIndex"
		{
			"library"	"server"
			"linux"		"@_Z18UTIL_PlayerByIndexi"
		}
	*/
	if (!g_pFindNextObserverTargetDetor) {
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::SERVER, "_ZN11CBasePlayer22FindNextObserverTargetEb");
		g_pFindNextObserverTargetDetor = DETOUR_CREATE_MEMBER(FindNextObserverTargetHook, pAddrFunc);
		if (g_pFindNextObserverTargetDetor) g_pFindNextObserverTargetDetor->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CBasePlayer::FindNextObserverTarget detour");
		
		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::SERVER, "_ZN11CBasePlayer31GetNextObserverSearchStartPointEb");
		GetNextObserverSearchStartPoint = reinterpret_cast<decltype(GetNextObserverSearchStartPoint)>(pAddrFunc);
		
		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::SERVER, "_ZN11CBasePlayer21IsValidObserverTargetEP11CBaseEntity");
		IsValidObserverTarget = reinterpret_cast<decltype(IsValidObserverTarget)>(pAddrFunc);
		
		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::SERVER, "_Z18UTIL_PlayerByIndexi");
		UTIL_PlayerByIndex = reinterpret_cast<decltype(UTIL_PlayerByIndex)>(pAddrFunc);
	}
}

void CObServerLoopFix::Unload() {
	if (g_pFindNextObserverTargetDetor) {
		g_pFindNextObserverTargetDetor->Destroy();
		g_pFindNextObserverTargetDetor = nullptr;
	}
}