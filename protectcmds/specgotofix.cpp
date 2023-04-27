#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>

#include "specgotofix.h"
#include <helpers/strtools.h>

#include <eiface.h>
#include <vector.h>

//#define clamp(val, min, max) (((val) > (max)) ? (max) : (((val) < (min)) ? (min) : (val)))
#define MAX_COORD_FLOAT				(16384.0f)
#define MIN_COORD_FLOAT				(-MAX_COORD_FLOAT)

enum {
	OBS_MODE_NONE = 0,	// not in spectator mode
	OBS_MODE_DEATHCAM,	// special mode for detah cam animation
	OBS_MODE_FIXED,		// view from a fixed camera position
	OBS_MODE_IN_EYE,	// follow a player in first perosn view
	OBS_MODE_CHASE,		// follow a player in third person view
	OBS_MODE_ROAMING,	// free roaming
};
 
CDetour *g_pClientCommandDetour = nullptr;

IVEngineServer *g_pEngine = nullptr;
int (__cdecl* GetObserverMode)(void *CBasePlayerThis);
void (__cdecl* JumptoPosition)(void *CBasePlayerThis, const Vector &origin, const QAngle &angles);

DETOUR_DECL_MEMBER1(ClientCommandHook, bool, char *, pCommand) {
	if (StrContains(pCommand, "spec_goto", true) == 0) {
		if ( (GetObserverMode( this ) == OBS_MODE_FIXED || GetObserverMode( this ) == OBS_MODE_ROAMING) && g_pEngine->Cmd_Argc() == 6 ) {
			Vector origin;
			origin.x = clamp( atof( g_pEngine->Cmd_Argv(1) ), MIN_COORD_FLOAT, MAX_COORD_FLOAT );
 			origin.y = clamp( atof( g_pEngine->Cmd_Argv(2) ), MIN_COORD_FLOAT, MAX_COORD_FLOAT );
 			origin.z = clamp( atof( g_pEngine->Cmd_Argv(3) ), MIN_COORD_FLOAT, MAX_COORD_FLOAT );

			QAngle angle;
			angle.x = atof( g_pEngine->Cmd_Argv(4) );
			angle.y = atof( g_pEngine->Cmd_Argv(5) );
			angle.z = 0.0f;

			JumptoPosition( this, origin, angle );
		}
		
		return true;
	}
	
	return DETOUR_MEMBER_CALL(ClientCommandHook)(pCommand);
}

void CSpecGotoFix::Load() {
	if (!g_pClientCommandDetour) {
		/*
			"CBasePlayer::ClientCommand"
			{
				"library"		"server"
				"linux"			"@_ZN11CBasePlayer13ClientCommandEPKc"
			}
		*/
		
		void * pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::SERVER, "_ZN11CBasePlayer15GetObserverModeEv");
		if (!pAddrFunc) {
			g_pSM->LogError(myself, "Failed to setup CBasePlayer::GetObServerMode detour");
			return;
		}
		GetObserverMode = reinterpret_cast<decltype(GetObserverMode)>(pAddrFunc);
		
		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::SERVER, "_ZN11CBasePlayer14JumptoPositionERK6VectorRK6QAngle");
		if (!pAddrFunc) {
			g_pSM->LogError(myself, "Failed to setup CBasePlayer::JumptoPosition detour");
			return;
		}
		JumptoPosition = reinterpret_cast<decltype(JumptoPosition)>(pAddrFunc);
		
		g_pEngine = *(reinterpret_cast<IVEngineServer **>(g_pSymbolManager->Resolve(ESymbolModule::SERVER, "engine")));
		if (!g_pEngine) {
			g_pSM->LogError(myself, "Failed to setup IVEngineServer detour");
			return;
		}
		
		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::SERVER, "_ZN11CBasePlayer13ClientCommandEPKc");
		g_pClientCommandDetour = DETOUR_CREATE_MEMBER(ClientCommandHook, pAddrFunc);
		if (g_pClientCommandDetour) g_pClientCommandDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CBasePlayer::ClientCommand detour");
	}
}

void CSpecGotoFix::Unload() {
	if (g_pClientCommandDetour) {
		g_pClientCommandDetour->Destroy();
		g_pClientCommandDetour = nullptr;
	}
}