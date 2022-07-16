#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>
#include <helpers/strtools.h>

#include <sys/socket.h>
#include <iostream>
#include <list>

#include "filterpacket.h"

#include <icvar.h>

using namespace std;

extern CGlobalVars* g_pGlobals;
#define NORMAL_SEQ_PACKET "\x00"
#define MAX_GAME_PACKETS_SEC_TIMEOUT 5

CDetour *g_pFilter_ShouldDiscardDetour = nullptr;
CDetour *g_pProcessConnectionlessPacketDetour = nullptr;
CDetour *g_pRecvFromDetour = nullptr;
CDetour *g_pProcessPacketDetour = nullptr;

CUtlVector < ipfilter_t > *g_pIPFilters = nullptr;
ConVar *pSV_FilterBan = nullptr;

list<PacketCount *> g_GamePacketList;

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

bool PacketIsBlock(list<PacketCount *> &pPacketList, netadr_t &adr, int iTimeoutSec, int iCheckTime, int iMaxPackets) {
	PacketCount *pPacketCount = nullptr;
	float fTime = g_pGlobals->realtime;
	
	for (auto iter = pPacketList.begin(); iter != pPacketList.end(); iter++) {
		pPacketCount = *iter;
		
		//Check timed out
		if (pPacketCount->fLastTimePacket + iTimeoutSec < fTime) {
			delete pPacketCount;
			iter = pPacketList.erase(iter);
			
			continue;
		}
		
		if (pPacketCount->ip == adr.GetIP()) {
			if (pPacketCount->fLastTimePacket + iCheckTime > fTime) {				
				if (pPacketCount->iCountPacket >= iMaxPackets) {
					
					return true;
				}
				
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
	pPacketCount->ip = adr.GetIP();
	pPacketCount->fLastTimePacket = fTime;
	pPacketCount->iCountPacket = 1;
	pPacketList.push_back(pPacketCount);
	
	return false;
}

DETOUR_DECL_MEMBER2(ProcessPacketHook, void, netpacket_t *, packet, bool, bHasHeader) {
	if (memcmp(packet->data+3, NORMAL_SEQ_PACKET, 1)) return;
	
	if (PacketIsBlock(g_GamePacketList, packet->from, 2, 1, 200)) return;
	
	return DETOUR_MEMBER_CALL(ProcessPacketHook)(packet, bHasHeader);
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