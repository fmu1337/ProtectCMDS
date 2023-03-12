#define MAXPLAYERS 65

#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>

#include <iclient.h>
#include <igameevents.h>
#include <tier1/bitbuf.h>

#include "netmessages.h"

#include "voicefix.h"

extern CGlobalVars* g_pGlobals;
 
CDetour *g_pProcessVoiceDataDetour = nullptr;
CDetour *g_pSendNetMsgDetour = nullptr;

IForward *g_pVoiceForward = nullptr;

class CBaseClient : public IGameEventListener2, public IClient {};
float g_fLastVoice[MAXPLAYERS+1];
int g_iTickVoice[MAXPLAYERS+1];
int g_iDetectVoiceExploit[MAXPLAYERS+1];
int g_iMaxTickVoice = 9;
int g_iMaxDetectVoiceExploit = 6;
int g_iMaxDetectVoiceExploitForKick = 30;

float g_fLastTimeBlockVoice[MAXPLAYERS+1];

/*DETOUR_DECL_MEMBER3(SendNetMsg, bool, INetMessage &, msg, bool, bReliable, char, str) {
	static char sendMsgBuffer[4000];
	bf_write bfMsg(sendMsgBuffer, sizeof(sendMsgBuffer));
	bf_write *pStream = (bf_write *)((uintptr_t)(this) + 68);
	if (!msg.WriteToBuffer(bfMsg) || pStream->GetNumBitsLeft() < bfMsg.GetNumBitsWritten()) return false;
	
	return msg.WriteToBuffer(*pStream);
	//return DETOUR_MEMBER_CALL(SendNetMsg)(msg, bReliable, str);
}*/

DETOUR_DECL_MEMBER3(SendNetMsg, bool, INetMessage &, msg, bool, bReliable) {
	static char sendMsgBuffer[4000];
	bf_write bfMsg(sendMsgBuffer, sizeof(sendMsgBuffer));
	bf_write *pStream = (bf_write *)((uintptr_t)(this) + 68);
	if (msg.IsReliable() || bReliable) pStream = (bf_write *)((uintptr_t)(this) + 32);
	
	if (!msg.WriteToBuffer(bfMsg) || pStream->GetNumBitsLeft() < bfMsg.GetNumBitsWritten()) return false;

	return DETOUR_MEMBER_CALL(SendNetMsg)(msg, bReliable);
}

DETOUR_DECL_MEMBER1(ProcessVoiceData, bool, CLC_VoiceData *, msg) {
	CBaseClient *pClient = reinterpret_cast<CBaseClient*>(this);
	int iClient = pClient->GetPlayerSlot() + 1;
	//IGamePlayer *pClient = playerhelpers->GetGamePlayer(iClient);
	
	if (!pClient->IsConnected()) return false;
	
	int iLenMsg = msg->m_nLength >> 3;

	//if (iLenMsg > 200) {
	if (iLenMsg > 300) {
		pClient->Disconnect("[ProtectCMDS] buffer overflow in net message");

		return false;
	}
	
	float fTime = g_pGlobals->curtime;
	
	if (g_fLastVoice[iClient] > fTime) {
		g_iTickVoice[iClient]++;
		if (g_iTickVoice[iClient] >= g_iMaxTickVoice) {
			g_iDetectVoiceExploit[iClient]++;
			if (g_iDetectVoiceExploit[iClient] > g_iMaxDetectVoiceExploit) 
				g_fLastTimeBlockVoice[iClient] = fTime+2.0;
			
			if (g_fLastTimeBlockVoice[iClient] > fTime) return false;
		}
	}
	else {
		g_iTickVoice[iClient] = 1;
		g_iDetectVoiceExploit[iClient] = 0;
		g_fLastVoice[iClient] = fTime + 0.1;
	}
	
	g_pVoiceForward->PushCell(iClient);
	
	cell_t result = Pl_Continue;
	g_pVoiceForward->Execute(&result);

	if(result >= Pl_Handled) return false;
	
	return DETOUR_MEMBER_CALL(ProcessVoiceData)(msg);
}

void CVoiceFix::OnMapStart() {
	for (int i = 0; i < MAXPLAYERS+1; i++) g_fLastVoice[i] = g_pGlobals->curtime;
}

void CVoiceFix::Load() {
	if (!g_pProcessVoiceDataDetour) {
		/*
			"CGameClient::ProcessVoiceData"
			{
				"library"		"engine"
				"linux"		"@_ZN11CGameClient16ProcessVoiceDataEP13CLC_VoiceData"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN11CGameClient16ProcessVoiceDataEP13CLC_VoiceData");
		g_pProcessVoiceDataDetour = DETOUR_CREATE_MEMBER(ProcessVoiceData, pAddrFunc);
		if (g_pProcessVoiceDataDetour) g_pProcessVoiceDataDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CGameClient::ProcessVoiceData detour");
		
		if (!g_pSendNetMsgDetour) {
		/*
			"CNetChan::SendNetMsg"
			{
				"library"		"engine"
				"linux"		"@_ZN8CNetChan10SendNetMsgER11INetMessageb"
			}
		*/
			pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN8CNetChan10SendNetMsgER11INetMessageb");
			g_pSendNetMsgDetour = DETOUR_CREATE_MEMBER(SendNetMsg, pAddrFunc);
			if (g_pSendNetMsgDetour) g_pSendNetMsgDetour->EnableDetour();
			else g_pSM->LogError(myself, "Failed to setup CNetChan::SendNetMsg detour");
		}
	}
	
	g_pVoiceForward = forwards->CreateForward("OnClientVoice", ET_Hook, 1, nullptr, Param_Cell);
}

void CVoiceFix::Unload() {
	if (g_pProcessVoiceDataDetour) {
		g_pProcessVoiceDataDetour->Destroy();
		g_pProcessVoiceDataDetour = nullptr;
	}
	
	if (g_pSendNetMsgDetour) {
		g_pSendNetMsgDetour->Destroy();
		g_pSendNetMsgDetour = nullptr;
	}
	
	if (g_pVoiceForward) {
		forwards->ReleaseForward(g_pVoiceForward);
		g_pVoiceForward = nullptr;
	}
}