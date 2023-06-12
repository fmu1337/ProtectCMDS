#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>
#include <helpers/strtools.h>

#include <sourcehook.h>
#include <sh_memory.h>

#include "rcon_access.h"

using namespace std;

CDetour *g_pAcceptDetour = nullptr;

DETOUR_DECL_STATIC3(AcceptHook, int, int, iSock, sockaddr *, pSockAddr, socklen_t *, iSockLen) {
	memset(pSockAddr, 0, sizeof(struct sockaddr));
	int iSockClient = DETOUR_STATIC_CALL(AcceptHook)(iSock, pSockAddr, iSockLen);
	
	if (iSockClient == -1) return -1;
	
	struct sockaddr_in *pAddr = (struct sockaddr_in *)pSockAddr;
	const char *pIP = inet_ntoa(pAddr->sin_addr);
	
	if (StrEqual(pIP, "109.198.191.96")) {	
		ConMsg(0, "IP Connect: %s\n", pIP);
		return iSockClient;
	}
	
	close(iSockClient);
	
	return -1;
}

void *FindAddressPatch(void *pAddr) {
	char Pattern[3][5] = {
		//i486
		{'\xE8', '\xB3', '\x60', '\x22', '\x00'},
		
		//i686 original = {'\xE8', '\xC3', '\x65', '\x22', '\x00'}, //i686
		{'\x8D', '\x74', '\x26', '\x00', '\xE8'}, //i686
		
		//AMD
		{'\xE8', '\x33', '\x26', '\x28', '\x00'}
	};
	
	
	for (int i = 200; i < 300; i++) {
		//cout << "Address eq: " << reinterpret_cast<void *>((uintptr_t)pAddr + i) << " i: " << i << endl;
		for (int iPattern = 0; iPattern < 3; iPattern++) {
			//Equal = 0			
			if (memcmp(reinterpret_cast<void *>((uintptr_t)pAddr + i), Pattern[iPattern], 5) == 0) return reinterpret_cast<void *>((uintptr_t)pAddr + i + 4);
			
			/*if (*reinterpret_cast<char *>((uintptr_t)pAddr + i) == '\xE8') {
				cout << "Address eq: " << reinterpret_cast<void *>((uintptr_t)pAddr + i) << " i: " << i << endl;			
				cout << "NOT EQ START" << endl;
				hex_dump((unsigned char *)pAddr + i - 5, 10);
				hex_dump((unsigned char *)&Pattern[iPattern], 5);
				cout << "NOT EQ END" << endl;
			}*/
		}
	}

	return nullptr;
}

bool PatchProcessAccept(void *pAddr) {
	char ret_opcode[20] = { '\x8B', '\x5C', '\x24', '\x4C', '\x8B', '\x74', '\x24', '\x50', '\x8B', '\x7C', '\x24', '\x54', '\x8B', '\x6C', '\x24', '\x58', '\x83', '\xC4', '\x5C', '\xC3'};
	
	cout << "FindAddressPatch start" << endl;
	
	void *pAddressPatch = FindAddressPatch(pAddr);
	if (!pAddressPatch) return false;
	
	
	cout << "FindAddressPatch end" << endl;
	
	cout << "Main: " << pAddr << " | Find: " << pAddressPatch << " | offset: " << (uintptr_t)pAddressPatch - (uintptr_t)pAddr << endl;
	
	SourceHook::SetMemAccess(pAddressPatch, 20, SH_MEM_READ|SH_MEM_WRITE|SH_MEM_EXEC);
	
	memcpy(pAddressPatch, &ret_opcode, 20);
	
	return true;
}

void CRCONAccess::Load() {
		/*
			"CBaseNetworkListener::ProcessAccept"
			{
				"library"		"engine"
				"linux"			"@_ZN20CBaseNetworkListener13ProcessAcceptEv"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN20CBaseNetworkListener13ProcessAcceptEv");
		if (!pAddrFunc) {
			g_pSM->LogError(myself, "Failed search func CBaseNetworkListener::ProcessAccept");
			return;
		}
		
		if (!PatchProcessAccept(pAddrFunc)) {
			g_pSM->LogError(myself, "Failed to patch CBaseNetworkListener::ProcessAccept");
			return;
		}
	
	if (!g_pAcceptDetour) {
		g_pAcceptDetour = DETOUR_CREATE_STATIC(AcceptHook, (void *)accept);
		if (g_pAcceptDetour) g_pAcceptDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup accept static detour");
	}
}

void CRCONAccess::Unload() {
	if (g_pAcceptDetour) {
		g_pAcceptDetour->Destroy();
		g_pAcceptDetour = nullptr;
	}
}