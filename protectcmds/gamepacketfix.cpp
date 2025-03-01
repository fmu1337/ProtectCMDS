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
#include "helpers/ratelimit.h"

#ifdef ENGINE_CSS34
#include "helpers/vtfvalidator.h"
#endif

#ifdef ENGINE_CSS
#include <edict.h>
//#include <game/server/iplayerinfo.h>
#endif

#include <tier1/bitbuf.h>
#include <inetmessage.h>
#include <inetmsghandler.h>
#include <iclient.h>
#include <netadr.h>
#include <tier0/dbg.h>
#include <tier1/convar.h>
#include <tier0/memalloc.h>
#include <filesystem.h>
#include <tier1/interface.h>
#include "net_chan.hpp"

#include "gamepacketfix.h"

using namespace std;

extern char g_cPrefix[];
extern CGlobalVars* g_pGlobals;
 
CDetour *g_pProcessMessagesDetour = nullptr;
CDetour *g_pCheckReceivingListDetour = nullptr;
CDetour *g_pSetupDetour = nullptr;

ConVar *net_showfragments = nullptr;
ConVar *sv_allowupload = nullptr;
IFileSystem* g_pFileSystem = nullptr;

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

CRateLimit g_RateLimiter[65];
CRateLimit g_UncompressLimiter[65];

/*Start Function Pointer*/
INetMessage *(__cdecl* FindMessage)(void *CNetChanThis, int cmd);
typedef int(__cdecl* BZ2_bzBuffToBuffDecompressFn)(char*         dest, 
      unsigned int* destLen,
      char*         source, 
      unsigned int  sourceLen,
      int           small, 
      int           verbosity 
);
BZ2_bzBuffToBuffDecompressFn BZ2_bzBuffToBuffDecompress;

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

bool ImplProcessMessages(CNetChan * NetChan, bf_read &buf) {
	int startbit = buf.GetNumBitsRead();
	while ( true ) {
		if ( buf.IsOverflowed() ) {
			INetChannelHandler *m_MessageHandler = (reinterpret_cast<CNetChan *>(NetChan))->m_MessageHandler();	
			//netadr_s remote_address = *(netadr_s *)( (uintptr_t)NetChan + 116 );
			
			if (m_MessageHandler) m_MessageHandler->ConnectionCrashed( "[ProtectCMDS] Buffer overflow in net message" );

			//ConMsg(0, "Buf overflow in buf.IsOverflowed()\n");
			
			return false;
		}	

		if ( buf.GetNumBitsLeft() < NETMSG_TYPE_BITS ) break;
		
		INetChannelHandler *m_MessageHandler = (reinterpret_cast<CNetChan *>(NetChan))->m_MessageHandler();
		
		unsigned char cmd = buf.ReadUBitLong( NETMSG_TYPE_BITS );
		
		if (cmd != net_NOP)
		{
			int m_nClientSlot = (int)(*((int*)m_MessageHandler + 3));
			if (!g_RateLimiter[m_nClientSlot].Check()) return false;
		}
		
		if ( cmd <= net_File ) {
			if ( !ProcessControlMessage( NetChan, cmd, buf ) ) return false; // disconnect or error

			continue;
		}

		INetMessage	* netmsg = FindMessage( NetChan, cmd );
		if ( netmsg ) {
			const char *msgname = netmsg->GetName();
			
			int startbit = buf.GetNumBitsRead();

			if ( !netmsg->ReadFromBuffer( buf ) ) {
				netadr_s *remote_address = (reinterpret_cast<CNetChan *>(NetChan))->remote_address();
				char sCmd[256];
				UTIL_Format(sCmd, sizeof(sCmd), "addip 5 %s\n", remote_address->ToString(true));
				//cout << "Netchannel: failed reading message " << msgname << " from " << remote_address.ToString() << endl;
				
				if (m_MessageHandler) m_MessageHandler->ConnectionCrashed( "[ProtectCMDS] Buffer overflow in net message" );

				//ConMsg(0, "buf overflow in failed reading message\n");
				
				gamehelpers->ServerCommand(sCmd);
			
				return false;
			}

			NetChan->UpdateMessageStats( netmsg->GetGroup(), buf.GetNumBitsRead() - startbit );
			
			bool &m_bProcessingMessages = *(bool *)((uintptr_t)NetChan + 4);
			m_bProcessingMessages = true;
			bool bRet = netmsg->Process();
			m_bProcessingMessages = false;


			if ( (reinterpret_cast<CNetChan *>(NetChan))->m_bShouldDelete() ) {
				g_pMemAlloc->Free(NetChan);
				return false;
			}
			
			#ifdef ENGINE_CSS
			bool &m_bClearedDuringProcessing = *(bool *)((uintptr_t)NetChan + 5);
			if ( m_bClearedDuringProcessing ) {
				// Clear() was called during processing, our buffer is no longer valid
				m_bClearedDuringProcessing = false;
				return false;
			}
			#endif

			if ( !bRet ) {
				#ifdef ENGINE_CSS
				if (netmsg->GetType() == 10) return false;

				if (m_MessageHandler) m_MessageHandler->ConnectionCrashed( "[ProtectCMDS] Buffer overflow in net message" );
				#endif
				
				//ConMsg(0, "Netchannel: failed processing message %s.\n", msgname);
				return false;
			}

			if ( NetChan->IsOverflowed() ) return false;
		}
		else {
			//netadr_s *remote_address = (reinterpret_cast<CNetChan *>(NetChan))->remote_address();
			//char sCmd[256];
			//UTIL_Format(sCmd, sizeof(sCmd), "addip 5 %s\n", remote_address->ToString(true));
			//cout << "Netchannel: unknown net message (" << cmd << ") from " << remote_address.ToString() << endl;
			
			if (m_MessageHandler) m_MessageHandler->ConnectionCrashed( "[ProtectCMDS] Buffer overflow in net message" );
			//ConMsg(0, "buf overflow in unknown net message\n");
			
			//gamehelpers->ServerCommand(sCmd);
			
			return false;
		}
	}
	
	return true;
}

DETOUR_DECL_MEMBER1(ProcessMessages, bool, bf_read &, buf) {
	return ImplProcessMessages((CNetChan*)this, buf);
}

DETOUR_DECL_MEMBER4(Setup, void, int, sock, void *, adr, const char *, name, INetChannelHandler *, handler) {
	int m_nClientSlot = (int)(*((int*)handler + 3));
	
	static float fTickRate = 1.0 / g_pGlobals->interval_per_tick;
	static int iLimit = int(fTickRate * 5.0);
	
	g_RateLimiter[m_nClientSlot].SetLimit(iLimit, 1.0, "NetChan");
	
	g_UncompressLimiter[m_nClientSlot].SetLimit(256000, 1.0, "Compressed fragments");
	
	return DETOUR_MEMBER_CALL(Setup)(sock, adr, name, handler);
}

void UncompressFragments( dataFragments_t& data )
{
	char *newbuffer = (char*)g_pMemAlloc->Alloc((data.nUncompressedSize + 3) & 0xFFFFFFFC);
	unsigned int uncompressedSize = data.nUncompressedSize;

	BZ2_bzBuffToBuffDecompress( newbuffer, &uncompressedSize, data.buffer, data.bytes, 1, 0 );

	if(data.buffer)
	{
		g_pMemAlloc->Free(data.buffer);
	}
	data.buffer = newbuffer;
	data.bytes = uncompressedSize;
	data.isCompressed = false;
}

DETOUR_DECL_MEMBER1(CheckReceivingList, bool, int, nList) {
	CNetChan* pNetChan = reinterpret_cast<CNetChan *>(this);
	
	dataFragments_t& data = pNetChan->GetReceiveList(nList);
	if (data.buffer == NULL)
	{
		return true;
	}
	
	if (data.ackedFragments < data.numFragments)
	{
		return true;
	}
	
	if (data.ackedFragments > data.numFragments)
	{
		Msg("Receiving failed: too many fragments %i/%i\n", data.ackedFragments, data.numFragments);
		return false;
	}

	if (net_showfragments->GetBool())
	{
		Msg("Receiving complete: %i fragments, %i bytes\n", data.numFragments, data.bytes);
	}

	if (data.isCompressed)
	{
		INetChannelHandler *m_MessageHandler = pNetChan->m_MessageHandler();	
		
		int m_nClientSlot = (int)(*((int*)m_MessageHandler + 3));
		
		if (!g_UncompressLimiter[m_nClientSlot].Check(data.nUncompressedSize)) return false;
		
		UncompressFragments(data);
	}

	if (!data.filename[0])
	{
		bf_read buffer(data.buffer, data.bytes);
		if (!ImplProcessMessages(pNetChan, buffer))
		{
			return false;
		}
	}
	else
	{
		if (sv_allowupload->GetBool())
		{
			if (IsValidFileForTransfer(data.filename))
			{
				#ifdef ENGINE_CSS34
				CVTFValidator VTFValidator;
				if ( !VTFValidator.IsValid(data.buffer, data.bytes) )
				{
					if ( VTFValidator.IsShouldLog() )
					{
						Msg("VTFValidator: invalid spray from %s (%s)\n", pNetChan->remote_address()->ToString(), VTFValidator.GetLastError());
					}

					delete [] data.buffer; 
					data.buffer = NULL;

					return true;
				}
				#endif
				
				if (!g_pFileSystem->FileExists(data.filename))
				{
					char szParentDir[ MAX_PATH ];
					if ( !V_ExtractFilePath( data.filename, szParentDir, sizeof(szParentDir) ) )
					{
						szParentDir[0] = '\0';
					}
					g_pFileSystem->CreateDirHierarchy( szParentDir );

					data.file = g_pFileSystem->Open(data.filename, "wb");
					if (FILESYSTEM_INVALID_HANDLE != data.file)
					{
						g_pFileSystem->Write(data.buffer, data.bytes, data.file);
						g_pFileSystem->Close(data.file);
						if (net_showfragments->GetInt() == 2)
						{
							DevMsg("FileReceived: %s, %i bytes (ID %i)\n", data.filename, data.bytes, data.transferID);
						}
						INetChannelHandler *m_MessageHandler = pNetChan->m_MessageHandler();
						
						#ifdef ENGINE_CSS34
						m_MessageHandler->FileReceived( data.filename, data.transferID );
						#else
						m_MessageHandler->FileReceived( data.filename, data.transferID );
						#endif
					}
					else
					{
						Msg("Failed to write received file '%s'!\n", data.filename);
					}
				}
			}
			else
			{
				Msg("Download file '%s' has invalid path or extension!\n", data.filename);
			}
		}
		else
		{
			Msg("Download file '%s' ignored. File uploads are disabled!\n", data.filename);
		}
	}

	if (data.buffer)
	{
		g_pMemAlloc->Free(data.buffer);
		data.buffer = NULL;
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
	*/
	if (!g_pProcessMessagesDetour) {
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN8CNetChan15ProcessMessagesER7bf_read");
		g_pProcessMessagesDetour = DETOUR_CREATE_MEMBER(ProcessMessages, pAddrFunc);
		if (g_pProcessMessagesDetour) g_pProcessMessagesDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CNetChan::ProcessMessages detour");
		
		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN8CNetChan11FindMessageEi");
		FindMessage = reinterpret_cast<decltype(FindMessage)>(pAddrFunc);
	}
	
	if (!g_pCheckReceivingListDetour) {
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN8CNetChan18CheckReceivingListEi");
		g_pCheckReceivingListDetour = DETOUR_CREATE_MEMBER(CheckReceivingList, pAddrFunc);
		if (g_pCheckReceivingListDetour) g_pCheckReceivingListDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CNetChan::CheckReceivingList detour");

		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "BZ2_bzBuffToBuffDecompress");
		BZ2_bzBuffToBuffDecompress = reinterpret_cast<decltype(BZ2_bzBuffToBuffDecompress)>(pAddrFunc);

		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "g_pFullFileSystem");
		g_pFileSystem = *reinterpret_cast<decltype(g_pFileSystem)*>(pAddrFunc);

		net_showfragments = cvar->FindVar("net_showfragments");
		sv_allowupload = cvar->FindVar("sv_allowupload");
	}
	
	if (!g_pSetupDetour) {
#ifdef ENGINE_CSS34
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN8CNetChan5SetupEiP8netadr_sPKcP18INetChannelHandler");
#else
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN8CNetChan5SetupEiP8netadr_sPKcP18INetChannelHandleri");
#endif
		g_pSetupDetour = DETOUR_CREATE_MEMBER(Setup, pAddrFunc);
		if (g_pSetupDetour) g_pSetupDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CNetChan::Setup detour");
	}
}

void CGamePacketFix::Unload() {
	if (g_pProcessMessagesDetour) {
		g_pProcessMessagesDetour->Destroy();
		g_pProcessMessagesDetour = nullptr;
	}
	
	if (g_pCheckReceivingListDetour) {
		g_pCheckReceivingListDetour->Destroy();
		g_pCheckReceivingListDetour = nullptr;
	}
	
	if (g_pSetupDetour) {
		g_pSetupDetour->Destroy();
		g_pSetupDetour = nullptr;
	}
}
