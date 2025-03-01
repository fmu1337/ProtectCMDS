#ifndef _NET_CHAN_H
#define _NET_CHAN_H

#include <inetchannel.h>

typedef void* FileHandle_t;

typedef struct dataFragments_s
{
	FileHandle_t file;				// open file handle
	char filename[260];				// filename
	char *buffer;					// if NULL it's a file
	unsigned int bytes;				// size in bytes
	unsigned int bits;				// size in bits
	unsigned int transferID;		// only for files
	bool isCompressed;				// true if data is bzip compressed
	unsigned int nUncompressedSize; // full size in bytes
#ifdef ENGINE_CSS34
	bool asTCP;						// send as TCP stream
#endif
	int numFragments;				// number of total fragments
	int ackedFragments;				// number of fragments send & acknowledged
	int pendingFragments;			// number of fragments send, but not acknowledged yet
} dataFragments_t;

class CNetChan : public INetChannel {
	public:
		INetChannelHandler *m_MessageHandler() {
			#ifdef ENGINE_CSS34
				return *reinterpret_cast<INetChannelHandler **>((uintptr_t)this + 7648);
			#elif ENGINE_CSS
				return *reinterpret_cast<INetChannelHandler **>((uintptr_t)this + 8988);
			#endif
		};
		
		netadr_s *remote_address() {
			#ifdef ENGINE_CSS34
				return reinterpret_cast<netadr_s *>((uintptr_t)this + 116);
			#elif ENGINE_CSS
				return reinterpret_cast<netadr_s *>((uintptr_t)this + 148);
			#endif
		};
		
		bool m_bShouldDelete() {
			#ifdef ENGINE_CSS34
				return *reinterpret_cast<bool *>((uintptr_t)this + 5);
			#elif ENGINE_CSS
				return *reinterpret_cast<bool *>((uintptr_t)this + 6);
			#endif
		};
		
		bf_write *m_StreamUnreliable() {
			return reinterpret_cast<bf_write *>((uintptr_t)this + 68);
		};
		
		bf_write *m_StreamReliable() {
			return reinterpret_cast<bf_write *>((uintptr_t)this + 32);
		};
		
		bf_write *m_StreamVoice() {
			
			return reinterpret_cast<bf_write *>((uintptr_t)this + 104);
		};
		
		dataFragments_s* m_pReceiveList() {
			#ifdef ENGINE_CSS34
				return reinterpret_cast<dataFragments_s *>((uintptr_t)this + 192);
			#elif ENGINE_CSS
				return reinterpret_cast<dataFragments_s *>((uintptr_t)this + 288);
			#endif
		};

		dataFragments_s& GetReceiveList(int nList) {
			return m_pReceiveList()[nList];
		};
};
#endif