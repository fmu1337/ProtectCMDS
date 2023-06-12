#ifndef _NET_CHAN_H
#define _NET_CHAN_H
class CNetChan {
	public:
		INetChannelHandler *m_MessageHandler() {
			#ifdef ENGINE_CSS34
				return *reinterpret_cast<INetChannelHandler **>((uintptr_t)this + 7648);
			#elif ENGINE_CSS
				return *reinterpret_cast<INetChannelHandler **>((uintptr_t)this + 9220);
			#endif
		};
		
		netadr_s *remote_address() {
			#ifdef ENGINE_CSS34
				return reinterpret_cast<netadr_s *>((uintptr_t)this + 116);
			#elif ENGINE_CSS
				return reinterpret_cast<netadr_s *>((uintptr_t)this + 152);
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
};
#endif