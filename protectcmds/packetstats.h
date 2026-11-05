#ifndef _PACKETSTATS_H_
#define _PACKETSTATS_H_

#include <stdint.h>
#include <time.h>

#include <helpers/rootconsole.h>

enum EPacketStat {
	PacketStat_Empty = 0,		// empty UDP datagrams, rewritten to one zero byte (what dosprotect counted)
	PacketStat_Split,			// split packets, dropped (CS:S v34)
	PacketStat_Flood,			// connectionless packets over filterpacket_connectionless_limit, dropped
	PacketStat_Max
};

// Per-IP counters of packets FilterPacket rewrites or drops. Fixed-size table and no allocations:
// Add() runs inside the recvfrom hook, though only for those packets, not for normal traffic.
class CPacketStats {
	public:
		void Configure(bool bEnabled, int iLogThreshold, int iLogInterval);
		bool IsEnabled() const { return m_bEnabled; }

		// iIP in network byte order
		void Add(EPacketStat eType, uint32_t iIP);

		// Called every frame: every iLogInterval seconds logs addresses whose counters grew by iLogThreshold or more
		void Think();

		void Print(PCMDSRootConsole::IRootConsole *pConsole);
		void Reset();

	private:
		struct Entry {
			uint32_t iIP;
			uint32_t iCount[PacketStat_Max];
			uint32_t iLogged[PacketStat_Max];
			time_t tFirst;
			time_t tLast;
		};

		static const int MAX_ENTRIES = 256;
		static const int MAX_PROBE = 16;
		static const int MAX_LOG_LINES = 10;

		Entry m_Entries[MAX_ENTRIES];
		uint32_t m_iTotal[PacketStat_Max];
		uint32_t m_iTotalLogged[PacketStat_Max];

		bool m_bEnabled = false;
		int m_iLogThreshold = 0;
		int m_iLogInterval = 10;
		time_t m_tNextLog = 0;
		time_t m_tSince = 0;
};

extern CPacketStats g_PacketStats;

const char *IPToString(uint32_t iIP, char *pBuffer, int iMaxLength);

#endif
