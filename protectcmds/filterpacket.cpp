#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>
#include <helpers/strtools.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <list>

#include "filterpacket.h"
#include "packetstats.h"
#include <helpers/inireader.h>

#include <icvar.h>
#include <tier0/vcrmode.h>

#ifdef ENGINE_CSS
#include <edict.h>
#endif

using namespace std;

#ifdef ENGINE_CSS34
#define NORMAL_SEQ_PACKET "\x00"
#define NET_HEADER_FLAG_SPLITPACKET "\xFE\xFF\xFF\xFF"
#endif

extern CGlobalVars* g_pGlobals;

#ifdef ENGINE_CSS34
CDetour *g_pFilter_ShouldDiscardDetour = nullptr;
CDetour *g_pProcessConnectionlessPacketDetour = nullptr;
#endif

CDetour *g_pProcessPacketDetour = nullptr;

#ifdef ENGINE_CSS34
CUtlVector < ipfilter_t > *g_pIPFilters = nullptr;
ConVar *pSV_FilterBan = nullptr;
void (*Filter_SendBan)(const netadr_t & addr) = nullptr;
#endif

// filterpacket_connectionless_limit: connectionless packets per second per address, 0 = no limit
int g_iConnectionlessLimit = 0;

bool g_bRecvFromHooked = false;
int (*g_pRealRecvFrom) (int , char *, int , int , struct sockaddr *, int *);

#ifdef ENGINE_CSS34
DETOUR_DECL_STATIC1(Filter_ShouldDiscardHook, bool, const netadr_t &, adr) {
	if (g_pIPFilters->Count() > 65) return false;
	
	return DETOUR_STATIC_CALL(Filter_ShouldDiscardHook)(adr);
}

// One-second window per address; fixed table, the address seen least recently gives way to a new one
struct ConnectionlessCount {
	uint32_t iIP;
	time_t tSecond;
	uint32_t iCount;
};

#define MAX_CONNECTIONLESS_LIST 256
#define MAX_CONNECTIONLESS_PROBE 16
ConnectionlessCount g_ConnectionlessList[MAX_CONNECTIONLESS_LIST];

bool IsConnectionlessFlood(uint32_t iIP) {
	time_t tNow = time(nullptr);
	int iStart = ((iIP * 2654435761u) >> 24) % MAX_CONNECTIONLESS_LIST;
	ConnectionlessCount *pSlot = nullptr;

	for (int i = 0; i < MAX_CONNECTIONLESS_PROBE; i++) {
		ConnectionlessCount *pEntry = &g_ConnectionlessList[(iStart + i) % MAX_CONNECTIONLESS_LIST];

		if (pEntry->iIP == iIP) {
			pSlot = pEntry;

			break;
		}

		if (!pSlot || pEntry->tSecond < pSlot->tSecond) pSlot = pEntry;
	}

	if (pSlot->iIP != iIP || pSlot->tSecond != tNow) {
		pSlot->iIP = iIP;
		pSlot->tSecond = tNow;
		pSlot->iCount = 0;
	}

	return ++pSlot->iCount > (uint32_t)g_iConnectionlessLimit;
}

DETOUR_DECL_MEMBER1(ProcessConnectionlessPacketHook, bool, netpacket_t *, packet) {
	if (g_iConnectionlessLimit > 0) {
		uint32_t iIP;
		memcpy(&iIP, packet->from.ip, sizeof(iIP));

		if (IsConnectionlessFlood(iIP)) {
			g_PacketStats.Add(PacketStat_Flood, iIP);

			return false;
		}
	}

	if (g_pIPFilters->Count() > 65 && DETOUR_STATIC_CALL(Filter_ShouldDiscardHook)(packet->from)) {
		Filter_SendBan(packet->from);
		
		return false;
	}
	
	return DETOUR_MEMBER_CALL(ProcessConnectionlessPacketHook)(packet);
}
#endif

static uint32_t GetSenderIP(const struct sockaddr *from) {
	if (!from || from->sa_family != AF_INET) return 0;

	return ((const struct sockaddr_in *)from)->sin_addr.s_addr;
}

int RecvFromHook(int s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen) {
	int iRet = g_pRealRecvFrom(s, buf, len, flags, from, fromlen);

	if (!iRet) {
		g_PacketStats.Add(PacketStat_Empty, GetSenderIP(from));

		iRet = 1;
		buf[0] = '\0';
	}
	#ifdef ENGINE_CSS34
	// Disable split packet (only a real packet: on an error the buffer still holds an old one)
	else if (iRet >= 4 && !memcmp(buf, NET_HEADER_FLAG_SPLITPACKET, 4)) {
		g_PacketStats.Add(PacketStat_Split, GetSenderIP(from));

		iRet = 1;
		buf[0] = '\0';
	}
	#endif

	return iRet;
}

DETOUR_DECL_MEMBER2(ProcessPacketHook, void, netpacket_t *, packet, bool, bHasHeader) {
	#ifdef ENGINE_CSS34
	if (memcmp(packet->data+3, NORMAL_SEQ_PACKET, 1)) return;
	#endif

	return DETOUR_MEMBER_CALL(ProcessPacketHook)(packet, bHasHeader);
}

// sm protectcmds packets [reset]
class CPacketStatsCommand : public PCMDSRootConsole::ICommand {
	public:
		void OnRootConsoleCommand(const char *pCmdName, const PCMDSRootConsole::ICommandArgs *pArgs) override {
			if (pArgs->ArgC() >= 3 && !strcmp(pArgs->Arg(2), "packets")) {
				if (pArgs->ArgC() >= 4 && !strcmp(pArgs->Arg(3), "reset")) {
					g_PacketStats.Reset();
					m_pConsole->ConsolePrint("[ProtectCMDS] Packet statistics cleared");

					return;
				}

				g_PacketStats.Print(m_pConsole);

				return;
			}

			m_pConsole->ConsolePrint("[ProtectCMDS] Usage: sm protectcmds packets [reset]");
			m_pConsole->ConsolePrint("    packets        - empty/split/flood packets dropped by FilterPacket, per IP");
			m_pConsole->ConsolePrint("    packets reset  - clear the statistics");
		}

	public:
		PCMDSRootConsole::IRootConsole *m_pConsole = nullptr;
};

CPacketStatsCommand g_PacketStatsCommand;

static void PacketStatsFrame(bool simulating) {
	g_PacketStats.Think();
}

void CFilterPacket::Load() {
	g_PacketStats.Configure(g_pSettings->GetInteger("FilterPacket", "filterpacket_stats", 1) != 0,
		g_pSettings->GetInteger("FilterPacket", "filterpacket_stats_log_threshold", 100),
		g_pSettings->GetInteger("FilterPacket", "filterpacket_stats_log_interval", 10));

	#ifdef ENGINE_CSS34
	g_iConnectionlessLimit = g_pSettings->GetInteger("FilterPacket", "filterpacket_connectionless_limit", 0);
	#endif

	if (!g_PacketStatsCommand.m_pConsole) {
		g_PacketStatsCommand.m_pConsole = PCMDSRootConsole::Add("protectcmds", "ProtectCMDS packet statistics", &g_PacketStatsCommand);
		if (!g_PacketStatsCommand.m_pConsole) g_pSM->LogError(myself, "Failed to add the \"sm protectcmds\" command");

		smutils->AddGameFrameHook(&PacketStatsFrame);
	}

	#ifdef ENGINE_CSS34
	if (!g_pFilter_ShouldDiscardDetour) {
		/*
			"Filter_ShouldDiscard"
			{
				"library"	"engine"
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
				"library"	"engine"
				"linux"		"@_ZN11CBaseServer27ProcessConnectionlessPacketEP11netpacket_s"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN11CBaseServer27ProcessConnectionlessPacketEP11netpacket_s");
		g_pProcessConnectionlessPacketDetour = DETOUR_CREATE_MEMBER(ProcessConnectionlessPacketHook, pAddrFunc);
		if (g_pProcessConnectionlessPacketDetour) g_pProcessConnectionlessPacketDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CBaseServer::ProcessConnectionlessPacket detour");
	}
	#endif
	
	if (!g_bRecvFromHooked) {
		g_pRealRecvFrom = g_pVCR->Hook_recvfrom;
		g_pVCR->Hook_recvfrom = &RecvFromHook;
		g_bRecvFromHooked = true;		
	}
	
	if (!g_pProcessPacketDetour) {
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN8CNetChan13ProcessPacketEP11netpacket_sb");
		g_pProcessPacketDetour = DETOUR_CREATE_MEMBER(ProcessPacketHook, pAddrFunc);
		if (g_pProcessPacketDetour) g_pProcessPacketDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CNetChan::ProcessPacket detour");
	}
}

void CFilterPacket::Unload() {
	if (g_PacketStatsCommand.m_pConsole) {
		PCMDSRootConsole::Remove("protectcmds", &g_PacketStatsCommand);
		g_PacketStatsCommand.m_pConsole = nullptr;

		smutils->RemoveGameFrameHook(&PacketStatsFrame);
	}

	#ifdef ENGINE_CSS34
	if (g_pFilter_ShouldDiscardDetour) {
		g_pFilter_ShouldDiscardDetour->Destroy();
		g_pFilter_ShouldDiscardDetour = nullptr;
	}
	
	if (g_pProcessConnectionlessPacketDetour) {
		g_pProcessConnectionlessPacketDetour->Destroy();
		g_pProcessConnectionlessPacketDetour = nullptr;
	}
	#endif

	if (g_bRecvFromHooked) {
		g_pVCR->Hook_recvfrom = g_pRealRecvFrom;
		g_bRecvFromHooked = false;		
	}	
	
	if (g_pProcessPacketDetour) {
		g_pProcessPacketDetour->Destroy();
		g_pProcessPacketDetour = nullptr;
	}
}