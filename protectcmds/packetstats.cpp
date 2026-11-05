#include <arpa/inet.h>
#include <string.h>

#include <algorithm>

#include "smsdk_ext.h"
#include "packetstats.h"

CPacketStats g_PacketStats;

static const char *g_sStatName[PacketStat_Max] = { "Empty UDP", "Split", "Connectionless flood" };

const char *IPToString(uint32_t iIP, char *pBuffer, int iMaxLength) {
	struct in_addr Addr;
	Addr.s_addr = iIP;

	if (!inet_ntop(AF_INET, &Addr, pBuffer, iMaxLength)) pBuffer[0] = '\0';

	return pBuffer;
}

void CPacketStats::Configure(bool bEnabled, int iLogThreshold, int iLogInterval) {
	Reset();

	m_bEnabled = bEnabled;
	m_iLogThreshold = iLogThreshold > 0 ? iLogThreshold : 0;
	m_iLogInterval = iLogInterval > 0 ? iLogInterval : 10;
	m_tNextLog = time(nullptr) + m_iLogInterval;
}

void CPacketStats::Reset() {
	memset(m_Entries, 0, sizeof(m_Entries));
	memset(m_iTotal, 0, sizeof(m_iTotal));
	memset(m_iTotalLogged, 0, sizeof(m_iTotalLogged));
	m_tSince = time(nullptr);
}

void CPacketStats::Add(EPacketStat eType, uint32_t iIP) {
	if (!m_bEnabled) return;

	m_iTotal[eType]++;

	// 0.0.0.0 marks a free slot; such packets only go to the totals
	if (!iIP) return;

	time_t tNow = time(nullptr);
	uint32_t iHash = iIP * 2654435761u;
	int iStart = (iHash >> 24) % MAX_ENTRIES;
	Entry *pOldest = nullptr;

	for (int i = 0; i < MAX_PROBE; i++) {
		Entry *pEntry = &m_Entries[(iStart + i) % MAX_ENTRIES];

		if (pEntry->iIP == iIP) {
			pEntry->iCount[eType]++;
			pEntry->tLast = tNow;

			return;
		}

		if (!pEntry->iIP) {
			pOldest = pEntry;

			break;
		}

		if (!pOldest || pEntry->tLast < pOldest->tLast) pOldest = pEntry;
	}

	// New address: a free slot, or the one seen least recently
	memset(pOldest, 0, sizeof(*pOldest));
	pOldest->iIP = iIP;
	pOldest->iCount[eType] = 1;
	pOldest->tFirst = pOldest->tLast = tNow;
}

void CPacketStats::Think() {
	if (!m_bEnabled || !m_iLogThreshold) return;

	time_t tNow = time(nullptr);
	if (tNow < m_tNextLog) return;

	m_tNextLog = tNow + m_iLogInterval;

	char szIP[INET_ADDRSTRLEN];

	for (int iType = 0; iType < PacketStat_Max; iType++) {
		uint32_t iTotalNew = m_iTotal[iType] - m_iTotalLogged[iType];
		m_iTotalLogged[iType] = m_iTotal[iType];

		if (iTotalNew < (uint32_t)m_iLogThreshold) {
			// Keep per-address counters in step so a slow trickle does not add up to one big line later
			for (int i = 0; i < MAX_ENTRIES; i++) m_Entries[i].iLogged[iType] = m_Entries[i].iCount[iType];

			continue;
		}

		g_pSM->LogMessage(myself, "%s packets: %u in the last %d s", g_sStatName[iType], iTotalNew, m_iLogInterval);

		int iLines = 0, iMore = 0;
		for (int i = 0; i < MAX_ENTRIES; i++) {
			Entry *pEntry = &m_Entries[i];
			uint32_t iNew = pEntry->iCount[iType] - pEntry->iLogged[iType];
			pEntry->iLogged[iType] = pEntry->iCount[iType];

			if (!pEntry->iIP || iNew < (uint32_t)m_iLogThreshold) continue;

			if (iLines++ < MAX_LOG_LINES)
				g_pSM->LogMessage(myself, "%s packets from %s: %u in the last %d s, %u since %ld s ago", g_sStatName[iType],
					IPToString(pEntry->iIP, szIP, sizeof(szIP)), iNew, m_iLogInterval, pEntry->iCount[iType], (long)(tNow - pEntry->tFirst));
			else iMore++;
		}

		if (iMore) g_pSM->LogMessage(myself, "%s packets: %d more addresses over the threshold", g_sStatName[iType], iMore);
	}
}

void CPacketStats::Print(PCMDSRootConsole::IRootConsole *pConsole) {
	time_t tNow = time(nullptr);

	pConsole->ConsolePrint("[ProtectCMDS] Packet statistics: %s, since %ld s ago, log: %s", m_bEnabled ? "on" : "off (filterpacket_stats)",
		(long)(tNow - m_tSince), m_iLogThreshold ? "on" : "off");
	if (m_iLogThreshold) pConsole->ConsolePrint("  Logged when an address sends %d or more in %d s", m_iLogThreshold, m_iLogInterval);
	pConsole->ConsolePrint("  Total: empty %u, split %u, connectionless flood %u", m_iTotal[PacketStat_Empty], m_iTotal[PacketStat_Split], m_iTotal[PacketStat_Flood]);

	int iIndex[MAX_ENTRIES], iCount = 0;
	for (int i = 0; i < MAX_ENTRIES; i++) if (m_Entries[i].iIP) iIndex[iCount++] = i;

	if (!iCount) return;

	std::sort(iIndex, iIndex + iCount, [this](int a, int b) {
		const Entry &A = m_Entries[a], &B = m_Entries[b];

		return A.iCount[0] + A.iCount[1] + A.iCount[2] > B.iCount[0] + B.iCount[1] + B.iCount[2];
	});

	char szIP[INET_ADDRSTRLEN];
	pConsole->ConsolePrint("  %-15s  %10s  %10s  %10s  %12s  %12s", "IP", "empty", "split", "flood", "first seen", "last seen");
	for (int i = 0; i < iCount; i++) {
		const Entry &E = m_Entries[iIndex[i]];
		pConsole->ConsolePrint("  %-15s  %10u  %10u  %10u  %10lds ago  %10lds ago", IPToString(E.iIP, szIP, sizeof(szIP)),
			E.iCount[PacketStat_Empty], E.iCount[PacketStat_Split], E.iCount[PacketStat_Flood], (long)(tNow - E.tFirst), (long)(tNow - E.tLast));
	}

	if (iCount == MAX_ENTRIES) pConsole->ConsolePrint("  The table is full: the addresses seen least recently are replaced by new ones");
}
