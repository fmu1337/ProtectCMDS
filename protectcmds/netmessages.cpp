#include "netmessages.h"
#include <tier1/strtools.h>

static char s_text[1024];

bool NET_SetConVar::WriteToBuffer( bf_write &buffer ) {
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	int numvars = m_ConVars.Count();
	
	// Note how many we're sending
	buffer.WriteByte( numvars );

	for (int i=0; i< numvars; i++ ) {
		cvar_t * cvar = &m_ConVars[i];
		buffer.WriteString( cvar->name  );
		buffer.WriteString( cvar->value );
	}

	return !buffer.IsOverflowed();
}

bool NET_SetConVar::ReadFromBuffer( bf_read &buffer ) {
	int numvars = buffer.ReadByte();
	
	m_ConVars.RemoveAll();
		
	for (int i=0; i< numvars; i++ ) {
		cvar_t cvar;
		buffer.ReadString( cvar.name, sizeof(cvar.name) );
		buffer.ReadString( cvar.value, sizeof(cvar.value) );
		m_ConVars.AddToTail( cvar );

	}
	return !buffer.IsOverflowed();
}

const char *NET_SetConVar::ToString(void) const {
	Q_snprintf(s_text, sizeof(s_text), "%s: %i cvars, \"%s\"=\"%s\"", 
		GetName(), m_ConVars.Count(), 
		m_ConVars[0].name, m_ConVars[0].value );
	return s_text;
}

const char *CLC_ClientInfo::ToString(void) const {
	Q_snprintf(s_text, sizeof(s_text), "%s: SendTableCRC %i", GetName(), 
		m_nSendTableCRC );
	return s_text;
}

bool CLC_ClientInfo::WriteToBuffer( bf_write &buffer ) {
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	buffer.WriteLong( m_nServerCount );
	buffer.WriteLong( m_nSendTableCRC );
	buffer.WriteOneBit( m_bIsHLTV?1:0 );
	buffer.WriteLong( m_nFriendsID );
	buffer.WriteString( m_FriendsName );
	
	for ( int i=0; i<MAX_CUSTOM_FILES; i++ ) {
		if ( m_nCustomFiles[i] != 0 ) {
			buffer.WriteOneBit( 1 );
			buffer.WriteUBitLong( m_nCustomFiles[i], 32 );
		}
		else buffer.WriteOneBit( 0 );
	}
		
	return !buffer.IsOverflowed();
}

bool CLC_ClientInfo::ReadFromBuffer( bf_read &buffer ) {
	m_nServerCount = buffer.ReadLong();
	m_nSendTableCRC = buffer.ReadLong();
	m_bIsHLTV = buffer.ReadOneBit()!=0;
	m_nFriendsID = buffer.ReadLong();
	buffer.ReadString( m_FriendsName, sizeof(m_FriendsName) );
	
	for ( int i=0; i<MAX_CUSTOM_FILES; i++ ) {
		if ( buffer.ReadOneBit() != 0 ) m_nCustomFiles[i] = buffer.ReadUBitLong( 32 );
		else m_nCustomFiles[i] = 0;
	}

	return !buffer.IsOverflowed();
}

const char *CLC_VoiceData::ToString(void) const {
	Q_snprintf(s_text, sizeof(s_text), "%s: %i bytes", GetName(), Bits2Bytes(m_nLength) );
	return s_text;
}

bool CLC_VoiceData::WriteToBuffer( bf_write &buffer ) {
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	
	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteWord( m_nLength );	// length in bits

	return buffer.WriteBits( m_DataOut.GetBasePointer(), m_nLength );
}

bool CLC_VoiceData::ReadFromBuffer( bf_read &buffer ) {
	m_nLength = buffer.ReadWord();	// length in bits

	m_DataIn = buffer;

	return buffer.SeekRelative( m_nLength );
}

bool SVC_VoiceData::WriteToBuffer( bf_write &buffer ) {
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteByte( m_nFromClient );
	buffer.WriteWord( m_nLength );
	
	return buffer.WriteBits( m_DataOut, m_nLength );
}

bool SVC_VoiceData::ReadFromBuffer( bf_read &buffer ) {
	m_nFromClient = buffer.ReadByte();
	m_nLength = buffer.ReadWord();

	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_VoiceData::ToString(void) const {
	Q_snprintf(s_text, sizeof(s_text), "%s: client %i, bytes %i", GetName(), m_nFromClient, Bits2Bytes(m_nLength) );
	return s_text;
}