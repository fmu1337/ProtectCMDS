#ifndef _NETMESSAGES_H
#define _NETMESSAGES_H

#define NETMSG_TYPE_BITS 			5
#define net_SetConVar				5
#define clc_ClientInfo				8
#define clc_VoiceData				10      // Voicestream data from a client
#define svc_VoiceData 				15
#define MAX_PLAYER_NAME_LENGTH		32
#define MAX_CUSTOM_FILES			4		// max 4 files
#define	MAX_OSPATH					260

#include <inetchannelinfo.h>
#include <inetmsghandler.h>
#include <inetmessage.h>
#include <tier1/bitbuf.h>
#include <tier1/checksum_crc.h>
#include <tier1/utlvector.h>

#define DECLARE_BASE_MESSAGE( msgtype )						\
	public:													\
		bool			ReadFromBuffer( bf_read &buffer );	\
		bool			WriteToBuffer( bf_write &buffer );	\
		const char		*ToString() const;					\
		int				GetType() const { return msgtype; } \
		const char		*GetName() const { return #msgtype;}\

#define DECLARE_SVC_MESSAGE( name )		\
	DECLARE_BASE_MESSAGE( svc_##name );	\
	IServerMessageHandler *m_pMessageHandler;\
	bool Process() { return m_pMessageHandler->Process##name( this ); }\
	
#define DECLARE_CLC_MESSAGE( name )		\
	DECLARE_BASE_MESSAGE( clc_##name );	\
	IClientMessageHandler *m_pMessageHandler;\
	bool Process() { return m_pMessageHandler->Process##name( this ); }\
	
#define DECLARE_NET_MESSAGE( name )			\
	DECLARE_BASE_MESSAGE( net_##name );		\
	INetMessageHandler *m_pMessageHandler;	\
	bool Process() { return m_pMessageHandler->Process##name( this ); }\
	
#define Bits2Bytes(b) ((b+7)>>3)

class CNetMessage : public INetMessage {
	public:
		CNetMessage() {	m_bReliable = true;
						m_NetChannel = NULL; }

		virtual ~CNetMessage() {};

		virtual int		GetGroup() const { return INetChannelInfo::GENERIC; }
		INetChannel		*GetNetChannel() const { return m_NetChannel; }
			
		virtual void	SetReliable( bool state) {m_bReliable = state;};
		virtual bool	IsReliable() const { return m_bReliable; };
		virtual void    SetNetChannel(INetChannel * netchan) { m_NetChannel = netchan; }	
		virtual bool	Process() { Assert( 0 ); return false; };	// no handler set

	protected:
		bool				m_bReliable;	// true if message should be send reliable
		INetChannel			*m_NetChannel;	// netchannel this message is from/for
};

class NET_SetConVar : public CNetMessage {
	DECLARE_NET_MESSAGE( SetConVar );

	int	GetGroup() const { return INetChannelInfo::STRINGCMD; }

	NET_SetConVar() {}
	NET_SetConVar( const char * name, const char * value)
	{
		cvar_t cvar;
		Q_strncpy( cvar.name, name, MAX_OSPATH );
		Q_strncpy( cvar.value, value, MAX_OSPATH );
		m_ConVars.AddToTail( cvar );	
	}

public:	
	
	typedef struct cvar_s {
		char	name[MAX_OSPATH];
		char	value[MAX_OSPATH];
	} cvar_t;

	CUtlVector<cvar_t> m_ConVars;
};

class CLC_ClientInfo : public CNetMessage {
	DECLARE_CLC_MESSAGE( ClientInfo );

public:
	CRC32_t			m_nSendTableCRC;
	int				m_nServerCount;
	bool			m_bIsHLTV;
	uint32			m_nFriendsID;
	char			m_FriendsName[MAX_PLAYER_NAME_LENGTH];
	CRC32_t			m_nCustomFiles[MAX_CUSTOM_FILES];
};

class CLC_VoiceData : public CNetMessage {
	DECLARE_CLC_MESSAGE( VoiceData );

	int	GetGroup() const { return INetChannelInfo::VOICE; }

	CLC_VoiceData() { m_bReliable = false; };

public:
	int				m_nLength;
	bf_read			m_DataIn;
	bf_write		m_DataOut;
	uint64			m_xuid;
};

class SVC_VoiceData : public CNetMessage {
	DECLARE_SVC_MESSAGE ( VoiceData );

	int	GetGroup() const { return INetChannelInfo::VOICE; }

	SVC_VoiceData() { m_bReliable = false; }

public:	
	int				m_nFromClient;	// client who has spoken
	//bool			m_bProximity;
	int				m_nLength;		// data length in bits
	//uint64			m_xuid;			// X360 player ID ?????????

	bf_read			m_DataIn;
	void			*m_DataOut;
};

#endif