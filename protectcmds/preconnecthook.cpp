#include <iostream>

#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>

#include <netadr.h>
#include "inetmessage.h"
#include <iclient.h>

#include "preconnecthook.h"

class CBaseServer;

using namespace std;
 
CDetour *g_pConnectClientDetour = nullptr;
IForward *g_pPreConnectForward = nullptr;
IForward *g_pPreConnectPostForward = nullptr;

void (*RejectConnection)(CBaseServer *, const netadr_t &addr, const char *format, ...);
int (*GetNextUserID)(CBaseServer *);

DETOUR_DECL_MEMBER8(ConnectClient, IClient *, netadr_t &, addr, int, protocol, int, challenge, int, nAuthProtocol, char const *, name, char const *, password, char *, hashedCDkey, int, cdKeyLen) {
	if (nAuthProtocol == 3) { // k_EAuthProtocolSteam = 3 
		char rejectReason[255] = "";
		g_pPreConnectForward->PushCell(GetNextUserID((CBaseServer *)this));
		g_pPreConnectForward->PushString(addr.ToString(false));
		g_pPreConnectForward->PushString(name);
		g_pPreConnectForward->PushString(password);
		g_pPreConnectForward->PushStringEx(rejectReason, sizeof(rejectReason), SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);

		cell_t result = Pl_Continue;
		g_pPreConnectForward->Execute(&result);

		if(result >= Pl_Handled) {
			RejectConnection((CBaseServer *)this, addr, rejectReason);
			return nullptr;
		}
	}
	
	IClient *pClient = DETOUR_MEMBER_CALL(ConnectClient)(addr, protocol, challenge, nAuthProtocol, name, password, hashedCDkey, cdKeyLen);;

	if (!pClient) return pClient;
	
	g_pPreConnectPostForward->PushCell(pClient->GetUserID());
	g_pPreConnectPostForward->PushString(addr.ToString(false));
	g_pPreConnectPostForward->PushString(pClient->GetClientName());
	g_pPreConnectPostForward->PushString(password);
	g_pPreConnectPostForward->Execute(nullptr);
	
	return pClient;
}

void CPreConnectHook::Load() {
	if (!g_pConnectClientDetour) {
		/*
			"CBaseServer::RejectConnection"
			{
				"library"		"engine"
				"linux"		"@_ZN11CBaseServer16RejectConnectionERK8netadr_sPcz"
			}
			"CBaseServer::ConnectClient"
			{
				"library"		"engine"
				"linux"		"@_ZN11CBaseServer13ConnectClientER8netadr_siiiPKcS3_S3_iS3_i"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN11CBaseServer13ConnectClientER8netadr_siiiPKcS3_S3_iS3_i");
		g_pConnectClientDetour = DETOUR_CREATE_MEMBER(ConnectClient, pAddrFunc);
		if (g_pConnectClientDetour) g_pConnectClientDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CBaseServer::ConnectClient detour");
		
		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN11CBaseServer16RejectConnectionERK8netadr_sPcz");
		RejectConnection = reinterpret_cast<decltype(RejectConnection)>(pAddrFunc);
		
		pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN11CBaseServer13GetNextUserIDEv");
		GetNextUserID = reinterpret_cast<decltype(GetNextUserID)>(pAddrFunc);
	}

	g_pPreConnectForward = forwards->CreateForward("ProtectCMDS_OnClientPreConnect", ET_Hook, 5, nullptr, Param_Cell, Param_String, Param_String, Param_String, Param_String);
	g_pPreConnectPostForward = forwards->CreateForward("ProtectCMDS_OnClientPreConnectPost", ET_Ignore, 4, nullptr, Param_Cell, Param_String, Param_String, Param_String);

}

void CPreConnectHook::Unload() {
	if (g_pConnectClientDetour) {
		g_pConnectClientDetour->Destroy();
		g_pConnectClientDetour = nullptr;
	}
	
	if (g_pPreConnectForward) {
		forwards->ReleaseForward(g_pPreConnectForward);
		g_pPreConnectForward = nullptr;
	}
	
	if (g_pPreConnectPostForward) {
		forwards->ReleaseForward(g_pPreConnectPostForward);
		g_pPreConnectPostForward = nullptr;
	}
}