#ifdef ENGINE_CSS34
#define NETMSG_TYPE_BITS 5
#elif ENGINE_CSS
#define NETMSG_TYPE_BITS 6
#endif

#define net_File 2
#define net_NOP 0
#define net_Disconnect 1

#include <iostream>

#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>
#include <helpers/strtools.h>
#include <helpers/util.h>

#include <tier1/bitbuf.h>
#include <inetmessage.h>
#include <inetmsghandler.h>
#include <iclient.h>
#include <netadr.h>
#include "net_chan.hpp"

#include "gamepacketfix.h"

using namespace std;

extern char g_cPrefix[];
 
CDetour *g_pProcessMessagesDetour = nullptr;

#ifdef ENGINE_CSS34
#define WHITE_PATH_MAX 5
char WhitePath[WHITE_PATH_MAX][11] = {
	"downloads/",	"maps/", "sound/",
	"models/", "materials/"
};
#elif ENGINE_CSS
#define WHITE_PATH_MAX 6
char WhitePath[WHITE_PATH_MAX][13] = {
	"downloads/",	"maps/", "sound/",
	"models/", "materials/", "user_custom/"
};
#endif

#define WHITE_EXT_MAX 12

char WhiteExt[WHITE_EXT_MAX][4] = {
	"dat",	"bsp", "nav", "txt",
	"vmt", "vtx", "phy", "mdl",
	"vvd", "wav", "mp3", "vtf"
};

/*Start Function Pointer*/
INetMessage *(__cdecl* FindMessage)(void *CNetChanThis, int cmd);
void (__cdecl* UpdateMessageStats)(void *CNetChanThis, int msgroup, int bits);
bool (__cdecl* IsOverflowed)(void *CNetChanThis);
/*End Function Pointer*/

bool IsValidFileForTransfer(const char *pszFilename) {
	if ( !pszFilename || !pszFilename[0] ) return false;
	
	if (StrContains(pszFilename, "..") != -1) return false;
	
	int iLen = strlen(pszFilename);
	
	if (pszFilename[iLen - 4] != '.') return false;
	
	#ifdef ENGINE_CSS34
	bool bDownloadPath = false;
	#endif
	bool bValid = false;
	int iOffsetPath = -1;
	
	for (int i = 0; i < WHITE_PATH_MAX; i++)
		if (StrContains(pszFilename, WhitePath[i]) == 0) {
			bValid = true;
			iOffsetPath = strlen(WhitePath[i]);
			
			#ifdef ENGINE_CSS34
			//if (!strcmp(WhitePath[i], "downloads/") && iLen != 22) return false;
			if (!strcmp(WhitePath[i], "downloads/")) bDownloadPath = true;
			#endif
			
			break;
		}
		
	if (!bValid) return false;
	
	int iLenExt = iLen - 3;
	
	bValid = false;
	for (int i = 0; i < WHITE_EXT_MAX; i++)
		if (StrContains(pszFilename, WhiteExt[i]) == iLenExt) {
			#ifdef ENGINE_CSS34
			if (bDownloadPath && strcmp(WhiteExt[i], "dat")) break;
			#endif
			
			bValid = true;
			break;
		}
	
	if (!bValid) return false;
	
	int iEndPath = iLen - 4;
	
	for (int i = iOffsetPath; i < iEndPath; i++) {
		if(!(
			(pszFilename[i] >= 'a' && pszFilename[i] <= 'z') ||
			(pszFilename[i] >= '0' && pszFilename[i] <= '9') ||
			#ifdef ENGINE_CSS34
			(!bDownloadPath &&
			#elif ENGINE_CSS
			(
			#endif
				(
					(pszFilename[i] >= 'A' && pszFilename[i] <= 'Z') ||
					pszFilename[i] == '$' || pszFilename[i] == '/' ||
					pszFilename[i] == '_' || pszFilename[i] == '-' ||
					pszFilename[i] == '.'
				)
			)
			)
		)
		{
			return false;
		}
	}

	return true;
}

bool ProcessControlMessage( void *CNetChanThis, int cmd, bf_read &buf) {
	char string[1024];
	
	if (cmd == net_NOP) return true;
	
	if (cmd == net_Disconnect) {
		//buf.ReadString( string, sizeof(string) );
		
		INetChannelHandler *m_MessageHandler = (reinterpret_cast<CNetChan *>(CNetChanThis))->m_MessageHandler();
		m_MessageHandler->ConnectionCrashed( "Disconnect by user" );
		
		return false;
	}
	
	if (cmd == net_File) {
		unsigned int transferID = buf.ReadUBitLong( 32 );

		buf.ReadString( string, sizeof(string) );
	
		if ( buf.ReadOneBit() != 0 && IsValidFileForTransfer( string ) ) {
			INetChannelHandler *m_MessageHandler = (reinterpret_cast<CNetChan *>(CNetChanThis))->m_MessageHandler();
			m_MessageHandler->FileRequested( string, transferID );
		}
		else {
			INetChannelHandler *m_MessageHandler = (reinterpret_cast<CNetChan *>(CNetChanThis))->m_MessageHandler();
			m_MessageHandler->FileDenied( string, transferID );
		}
		
		return true;
	}
	
	//netadr_s remote_address = *(netadr_s *)( (uintptr_t)CNetChanThis + 116 );
	//ConMsg(0, "Netchannel: received bad control cmd %i from %s.\n", cmd, remote_address.ToString() );
	
	return false;
}

DETOUR_DECL_MEMBER1(ProcessMessages, bool, bf_read &, buf) {
	int startbit = buf.GetNumBitsRead();
	while ( true ) {
		if ( buf.IsOverflowed() ) {
			INetChannelHandler *m_MessageHandler = (reinterpret_cast<CNetChan *>(this))->m_MessageHandler();	
			//netadr_s remote_address = *(netadr_s *)( (uintptr_t)this + 116 );
			//char sCmd[256];
			//UTIL_Format(sCmd, sizeof(sCmd), "addip 5 %s\n", remote_address.ToString(true));
			
			if (m_MessageHandler) m_MessageHandler->ConnectionCrashed( "[ProtectCMDS] Buffer overflow in net message" );
			
			//gamehelpers->ServerCommand(sCmd);
			
			return false;
		}	

		if ( buf.GetNumBitsLeft() < NETMSG_TYPE_BITS ) break;
		
		unsigned char cmd = buf.ReadUBitLong( NETMSG_TYPE_BITS );
		if ( cmd <= net_File ) {
			if ( !ProcessControlMessage( this, cmd, buf ) ) return false; // disconnect or error

			continue;
		}

		INetMessage	* netmsg = FindMessage( this, cmd );
		if ( netmsg ) {
			const char *msgname = netmsg->GetName();
			
			int startbit = buf.GetNumBitsRead();

			if ( !netmsg->ReadFromBuffer( buf ) ) {
				netadr_s *remote_address = (reinterpret_cast<CNetChan *>(this))->remote_address();
				char sCmd[256];
				UTIL_Format(sCmd, sizeof(sCmd), "addip 5 %s\n", remote_address->ToString(true));
				//cout << "Netchannel: failed reading message " << msgname << " from " << remote_address.ToString() << endl;
				
				INetChannelHandler *m_MessageHandler = (reinterpret_cast<CNetChan *>(this))->m_MessageHandler();
				
				if (m_MessageHandler) m_MessageHandler->ConnectionCrashed( "[ProtectCMDS] Buffer overflow in net message" );
				
				gamehelpers->ServerCommand(sCmd);
			
				return false;
			}

			UpdateMessageStats( this, netmsg->GetGroup(), buf.GetNumBitsRead() - startbit );
			
			bool &m_bProcessingMessages = *(bool *)((uintptr_t)this + 4);
			m_bProcessingMessages = true;
			bool bRet = netmsg->Process();
			m_bProcessingMessages = false;


			if ( (reinterpret_cast<CNetChan *>(this))->m_bShouldDelete() ) {
				delete this;
				return false;
			}
			
			#ifdef ENGINE_CSS
			bool &m_bClearedDuringProcessing = *(bool *)((uintptr_t)this + 5);
			if ( m_bClearedDuringProcessing ) {
				// Clear() was called during processing, our buffer is no longer valid
				m_bClearedDuringProcessing = false;
				return false;
			}
			#endif

			if ( !bRet ) {
				#ifdef ENGINE_CSS
				if (netmsg->GetType() == 10) return false;

				INetChannelHandler *m_MessageHandler = (reinterpret_cast<CNetChan *>(this))->m_MessageHandler();
				if (m_MessageHandler) m_MessageHandler->ConnectionCrashed( "[ProtectCMDS] Buffer overflow in net message" );
				#endif
				
				//ConMsg(0, "Netchannel: failed processing message %s.\n", msgname);
				return false;
			}

			if ( IsOverflowed(this) ) return false;
		}
		else {
			netadr_s *remote_address = (reinterpret_cast<CNetChan *>(this))->remote_address();
			char sCmd[256];
			UTIL_Format(sCmd, sizeof(sCmd), "addip 5 %s\n", remote_address->ToString(true));
			//cout << "Netchannel: unknown net message (" << cmd << ") from " << remote_address.ToString() << endl;
			
			INetChannelHandler *m_MessageHandler = (reinterpret_cast<CNetChan *>(this))->m_MessageHandler();
			if (m_MessageHandler) m_MessageHandler->ConnectionCrashed( "[ProtectCMDS] Buffer overflow in net message" );
			
			gamehelpers->ServerCommand(sCmd);
			
			return false;
		}
	}
	
	return true;
}

void CGamePacketFix::Load() {
	/*
		"CNetChan::ProcessMessages"
		{
			"library"    "engine"
			"linux"        "@_ZN8CNetChan15ProcessMessagesER7bf_read"
		}
		"CNetChan::FindMessage"
		{
			"library"    "engine"
			"linux"        "@_ZN8CNetChan11FindMessageEi"
		}
		"CNetChan::UpdateMessageStats"
		{
			"library"    "engine"
			"linux"        "@_ZN8CNetChan18UpdateMessageStatsEii"
		}
		"CNetChan::IsOverflowed"
		{
			"library"    "engine"
			"linux"        "@_ZNK8CNetChan12IsOverflowedEv"
		}
	*/
	if (!g_pProcessMessagesDetour) {
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN8CNetChan15ProcessMessagesER7bf_read");
		g_pProcessMessagesDetour = DETOUR_CREATE_MEMBER(ProcessMessages, pAddrFunc);
		if (g_pProcessMessagesDetour) g_pProcessMessagesDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CNetChan::ProcessMessages detour");
		
		bool bErrorGetStructure = false;
		
		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN8CNetChan11FindMessageEi");
		FindMessage = reinterpret_cast<decltype(FindMessage)>(pAddrFunc);
		
		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN8CNetChan18UpdateMessageStatsEii");
		UpdateMessageStats = reinterpret_cast<decltype(UpdateMessageStats)>(pAddrFunc);
		
		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZNK8CNetChan12IsOverflowedEv");
		IsOverflowed = reinterpret_cast<decltype(IsOverflowed)>(pAddrFunc);
	}
}

void CGamePacketFix::Unload() {
	if (g_pProcessMessagesDetour) {
		g_pProcessMessagesDetour->Destroy();
		g_pProcessMessagesDetour = nullptr;
	}
}