#ifndef _FILTERPACKET_H_
#define _FILTERPACKET_H_

#include <list>

#include "ifix.h"
#include <tier1/bitbuf.h>
#include <tier1/netadr.h>

typedef struct
{
	unsigned	mask;
	unsigned	compare;
	float       banEndTime; // 0 for permanent ban
	float       banTime;
} ipfilter_t;

typedef struct netpacket_s
{
	netadr_t		from;		// sender IP
	int				source;		// received source 
	double			received;	// received time
	unsigned char	*data;		// pointer to raw packet data
	bf_read			message;	// easy bitbuf data access
	int				size;		// size in bytes
	int				wiresize;   // size in bytes before decompression
	bool			stream;		// was send as stream
	struct netpacket_s *pNext;	// for internal use, should be NULL in public
} netpacket_t;

struct PacketCount {
	unsigned long ip;
	int iCountPacket;
	int fLastTimePacket;
};

class CFilterPacket : public IFix {
	public:
		void Load();
		void Unload();
};

bool IsPacketsFlood(unsigned int ip, char *pIP);

#endif