#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>
#include <helpers/strtools.h>

#include <sys/socket.h>
#include <list>

#include "filterpacket.h"

#include <icvar.h>

using namespace std;

#define NORMAL_SEQ_PACKET "\x00"
#define GAME_PACKET_TIMEOUT 2
#define GAME_PACKET_CHECK_TIME 1
#define MAX_GAME_PACKETS_PER_CHECK_TIME 200

extern CGlobalVars* g_pGlobals;

CDetour *g_pFilter_ShouldDiscardDetour = nullptr;
CDetour *g_pProcessConnectionlessPacketDetour = nullptr;
CDetour *g_pRecvFromDetour = nullptr;
CDetour *g_pProcessPacketDetour = nullptr;

CUtlVector < ipfilter_t > *g_pIPFilters = nullptr;
ConVar *pSV_FilterBan = nullptr;

list<PacketCount *> g_GamePacketList;

#define MAX_PACKET_LIST 128
PacketCount g_PacketList[MAX_PACKET_LIST];

void (*Filter_SendBan)(const netadr_t & addr) = nullptr;

DETOUR_DECL_STATIC1(Filter_ShouldDiscardHook, bool, const netadr_t &, adr) {
	if (g_pIPFilters->Count() > 65) return false;
	
	return DETOUR_STATIC_CALL(Filter_ShouldDiscardHook)(adr);
}

DETOUR_DECL_MEMBER1(ProcessConnectionlessPacketHook, bool, netpacket_t *, packet) {
	if (g_pIPFilters->Count() > 65 && DETOUR_STATIC_CALL(Filter_ShouldDiscardHook)(packet->from)) {
		Filter_SendBan(packet->from);
		
		return false;
	}
	
	return DETOUR_MEMBER_CALL(ProcessConnectionlessPacketHook)(packet);
}

DETOUR_DECL_STATIC6(RecvFromHook, int, int, iSock, char *, pBuf, int, iLen, int, iFlags, void *, from, void *, fromlen) {
	int iRet = DETOUR_STATIC_CALL(RecvFromHook)(iSock, pBuf, iLen, iFlags, from, fromlen);
	
	if (!iRet) {
		iRet = 1;
		pBuf[0] = '0';
	}
	
	return iRet;
}

DETOUR_DECL_MEMBER2(ProcessPacketHook, void, netpacket_t *, packet, bool, bHasHeader) {
	if (memcmp(packet->data+3, NORMAL_SEQ_PACKET, 1)) return;

	if (IsPacketsFlood(packet->from.GetIP())) return;
	
	return DETOUR_MEMBER_CALL(ProcessPacketHook)(packet, bHasHeader);
}

bool IsPacketsFlood(unsigned int ip) {
	float fTime = g_pGlobals->curtime;
	float fTimedOut = fTime - (GAME_PACKET_TIMEOUT - GAME_PACKET_CHECK_TIME);
	
	int i = 0;
	for (i = 0; i < MAX_PACKET_LIST; i++) {
		if (g_PacketList[i].ip == 0) continue;
		
		if (g_PacketList[i].fLastTimePacket < fTimedOut) {
			g_PacketList[i].ip = 0;
			 
			continue;
		}
	
		if (g_PacketList[i].ip == ip) {
			if (g_PacketList[i].fLastTimePacket > fTime) {
				if (g_PacketList[i].iCountPacket > MAX_GAME_PACKETS_PER_CHECK_TIME) return true;
				
				g_PacketList[i].fLastTimePacket = fTime;
				g_PacketList[i].iCountPacket++;
			}
			else {
				g_PacketList[i].fLastTimePacket = fTime;
				g_PacketList[i].iCountPacket = 1;
			}
			
			return false;
		}
	}

	if (++i >= MAX_PACKET_LIST) return false;
	
	g_PacketList[i].ip = ip;
	g_PacketList[i].fLastTimePacket = fTime + GAME_PACKET_CHECK_TIME;
	g_PacketList[i].iCountPacket = 1;
	
	return false;
	
	/*PacketCount *pPacketCount = nullptr;
	float fTime = g_pGlobals->curtime;
	
	for (auto iter = pPacketList.begin(); iter != pPacketList.end(); iter++) {
		pPacketCount = *iter;
		
		//Check timed out
		if (pPacketCount->fLastTimePacket + GAME_PACKET_TIMEOUT < fTime) {
			delete pPacketCount;
			iter = pPacketList.erase(iter);
			 
			continue;
		}
		
		if (pPacketCount->ip == ip) {
			if (pPacketCount->fLastTimePacket + GAME_PACKET_CHECK_TIME > fTime) {
				if (pPacketCount->iCountPacket >= MAX_GAME_PACKETS_PER_CHECK_TIME) return true;
				
				pPacketCount->fLastTimePacket = fTime;
				pPacketCount->iCountPacket++;
			}
			else {
				pPacketCount->fLastTimePacket = fTime;
				pPacketCount->iCountPacket = 1;
			}
			
			return false;
		}
	}
	
	pPacketCount = new PacketCount;
	pPacketCount->ip = ip;
	pPacketCount->fLastTimePacket = fTime;
	pPacketCount->iCountPacket = 1;
	pPacketList.push_back(pPacketCount);
	
	return false;*/
}

void CFilterPacket::Load() {
	if (!g_pFilter_ShouldDiscardDetour) {
		/*
			"Filter_ShouldDiscard"
			{
				"library"		"engine"
				"linux"		"@_Z20Filter_ShouldDiscardRK8netadr_s"
			}
		*/
		
		g_pIPFilters = (CUtlVector < ipfilter_t > *)g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "g_IPFilters");
		if (!g_pIPFilters) {
			g_pSM->LogError(myself, "Failed to get IPFilters offset");
			
			return;
		}
		
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_Z14Filter_SendBanRK8netadr_s");
		Filter_SendBan = reinterpret_cast<decltype(Filter_SendBan)>(pAddrFunc);
		
		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_Z20Filter_ShouldDiscardRK8netadr_s");
		g_pFilter_ShouldDiscardDetour = DETOUR_CREATE_STATIC(Filter_ShouldDiscardHook, pAddrFunc);
		if (g_pFilter_ShouldDiscardDetour) g_pFilter_ShouldDiscardDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup Filter_ShouldDiscard detour");
	}
	
	if (!g_pProcessConnectionlessPacketDetour) {
		/*
			"CBaseServer::ProcessConnectionlessPacket"
			{
				"library"		"engine"
				"linux"		"@_ZN11CBaseServer27ProcessConnectionlessPacketEP11netpacket_s"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN11CBaseServer27ProcessConnectionlessPacketEP11netpacket_s");
		g_pProcessConnectionlessPacketDetour = DETOUR_CREATE_MEMBER(ProcessConnectionlessPacketHook, pAddrFunc);
		if (g_pProcessConnectionlessPacketDetour) g_pProcessConnectionlessPacketDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CBaseServer::ProcessConnectionlessPacket detour");
	}
	
	if (!g_pRecvFromDetour) {
		g_pRecvFromDetour = DETOUR_CREATE_STATIC(RecvFromHook, (void *)recvfrom);
		if (g_pRecvFromDetour) g_pRecvFromDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup recvfrom detour");
	}
	
	if (!g_pProcessPacketDetour) {
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN8CNetChan13ProcessPacketEP11netpacket_sb");
		g_pProcessPacketDetour = DETOUR_CREATE_MEMBER(ProcessPacketHook, pAddrFunc);
		if (g_pProcessPacketDetour) g_pProcessPacketDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CNetChan::ProcessPacket detour");
	}
}

void CFilterPacket::Unload() {
	if (g_pFilter_ShouldDiscardDetour) {
		g_pFilter_ShouldDiscardDetour->Destroy();
		g_pFilter_ShouldDiscardDetour = nullptr;
	}
	
	if (g_pProcessConnectionlessPacketDetour) {
		g_pProcessConnectionlessPacketDetour->Destroy();
		g_pProcessConnectionlessPacketDetour = nullptr;
	}
	
	if (g_pRecvFromDetour) {
		g_pRecvFromDetour->Destroy();
		g_pRecvFromDetour = nullptr;
	}
	
	if (g_pProcessPacketDetour) {
		g_pProcessPacketDetour->Destroy();
		g_pProcessPacketDetour = nullptr;
	}
}