#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

#include <sh_list.h>

#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>
#include <helpers/sha256.h>
#include <helpers/util.h>

#include <steamclientpublic.h>
#include <iclient.h>
#include <igameevents.h>
#ifdef ENGINE_CSS
#include <iserver.h>
#endif

using namespace std;
using std::string;

#include "hashingsteamid.h"

class CBaseClient : public IGameEventListener2, public IClient {
public:
	virtual ~CBaseClient();
};

extern char g_cPrefix[];

CDetour *g_pSetSteamIDDetour = nullptr;
string g_sSalt;

#ifdef ENGINE_CSS
CDetour *g_pGetClientSteamIDByPlayerIndexDetour = nullptr;
CDetour *g_pGetNetworkIDStringDetour = nullptr;
CDetour *g_pFilter_IsUserBannedDetour = nullptr;

const USERID_t (__cdecl* GetNetworkID)(void *This);

#define MAXPLAYERS 65
#define IDTYPE_STEAM	1

struct USERID_t {
	int			idtype;
	CSteamID	steamid;
};

IServer *g_pServer = nullptr;

CSteamID g_SteamIDList[MAXPLAYERS+1];
#endif

SourceHook::List<SteamID_Hash *> m_pSteamIDHash;

static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

#define SALT_BIT 256

#ifdef ENGINE_CSS
CBaseClient* FindClientByUserID(const USERID_t& userid) {
	uint32_t iAccountID = userid.steamid.GetAccountID();
	for (int i = 0; i < g_pServer->GetClientCount(); i++) {
		CBaseClient *pClient = static_cast<CBaseClient *>(g_pServer->GetClient(i));
		
		if (iAccountID == pClient->GetNetworkID().steamid.GetAccountID()) return pClient;
	}
	
	return nullptr;
}

DETOUR_DECL_STATIC1(Filter_IsUserBannedHook, bool, const USERID_t&, userid) {
	CBaseClient *pClient = FindClientByUserID(userid);
	if (!pClient) return false;
	
	USERID_t userID;
	
	userID.steamid = g_SteamIDList[pClient->GetPlayerSlot()+1];
	userID.idtype = IDTYPE_STEAM;
	
	bool bResult = DETOUR_STATIC_CALL(Filter_IsUserBannedHook)(userID);
	
	if (bResult) ConMsg(0, "%s [STEAM_0:%u:%u] is banned\n", g_cPrefix, userID.steamid.GetAccountID() & 1, userID.steamid.GetAccountID() >> 1);
	
	return bResult;
}

DETOUR_DECL_MEMBER0(GetNetworkIDStringHook, const char *) {
	CBaseClient *pClient = reinterpret_cast<CBaseClient *>(this);
	
	if (pClient->IsFakeClient()) return "BOT";
	
	static char szSteamID[64];
	g_pSM->Format(szSteamID, sizeof(szSteamID), "[U:1:%u]", g_SteamIDList[pClient->GetPlayerSlot()+1].GetAccountID());
	
	return szSteamID;
}

DETOUR_DECL_MEMBER1(GetClientSteamIDByPlayerIndexHook, CSteamID *, int, entnum) {
	if (entnum < 1 || entnum > g_pServer->GetClientCount()) return nullptr;
	
	CBaseClient *pClient = static_cast<CBaseClient *>(g_pServer->GetClient(entnum - 1));
	if (!pClient || !pClient->IsConnected()) return nullptr;
	
	CSteamID *pSteamID = (CSteamID*)(reinterpret_cast<uintptr_t>(pClient) + 89);
	if (!pSteamID->IsValid()) return nullptr;
	
	return &g_SteamIDList[pClient->GetPlayerSlot()+1];
}
#endif

DETOUR_DECL_MEMBER1(SetSteamID, void *, CSteamID &, inputSteamID) {
	CBaseClient* pBaseClient = reinterpret_cast<CBaseClient *>(this);
	
	if (pBaseClient->IsFakeClient() || pBaseClient->IsHLTV()) return DETOUR_MEMBER_CALL(SetSteamID)(inputSteamID);
	
	uint32_t inputAccountID = inputSteamID.GetAccountID();
	if (!inputAccountID) return DETOUR_MEMBER_CALL(SetSteamID)(inputSteamID);
	
	CSteamID SteamID(inputSteamID);
	uint32_t hashedAccountID = 0;
	
	hashedAccountID = CHashingSteamID::CheckHashConfigSteamID(inputAccountID);
	if (!hashedAccountID) {
		hashedAccountID = CHashingSteamID::HashSteam(inputAccountID);
		if (!(inputAccountID & 1)) hashedAccountID--;
	}

	SteamID.SetAccountID(hashedAccountID);
	#ifdef ENGINE_CSS
	g_SteamIDList[pBaseClient->GetPlayerSlot()+1] = SteamID;
	#endif
	
	ConMsg(0, "%s [STEAM_0:%u:%u] changed to [STEAM_0:%u:%u]\n", g_cPrefix, inputAccountID & 1, inputAccountID >> 1, hashedAccountID & 1, hashedAccountID >> 1);
	
	#ifdef ENGINE_CSS34
	inputSteamID.SetAccountID(hashedAccountID);
	#endif
	
	return DETOUR_MEMBER_CALL(SetSteamID)(inputSteamID);
}

uint32_t CHashingSteamID::CheckHashConfigSteamID(uint32_t AccountID) {
	SourceHook::List<SteamID_Hash *>::iterator iter;
	
	SteamID_Hash *pSteamID_Hash;
	
	for (iter=m_pSteamIDHash.begin(); iter != m_pSteamIDHash.end(); iter++) {
		pSteamID_Hash = (*iter);
		
		if (pSteamID_Hash->AccountID_Original == AccountID) return pSteamID_Hash->AccountID_Replaced;
	}
	
	return 0;
}

uint32_t CHashingSteamID::HashSteam(uint32_t AccountID) {
	string sSteamID = std::to_string((int)(AccountID/2));
	sSteamID.append(g_sSalt.c_str());
	string sHash = sha256(sSteamID);
	int iHashSteam = HexToDec(sHash, strlen(sSteamID.c_str())-strlen(g_sSalt.c_str()));
	
	return (uint32_t)(iHashSteam*2);
}

int CHashingSteamID::HexToDec(string sHash, int iLength) {
	int iSteamID = 0;
	
	for (int i = 0; i < iLength; i++) {
		sHash.resize(iLength-i);
		std::istringstream(sHash) >> std::hex >> iSteamID;
		
		if (iSteamID < 2147483647) break;
	}
	
	return iSteamID;
}

bool CHashingSteamID::GetSalt() {
	char szPath[256];
	char szOldPath[256];
	g_pSM->BuildPath(Path_SM, szPath, sizeof szPath, "data/protectcmds/protectcmds_steamid_salt.dat");
	g_pSM->BuildPath(Path_SM, szOldPath, sizeof szOldPath, "data/protectcmds_steamid_salt.dat");
	
	if (!libsys->PathExists(szPath)) {
		if (libsys->PathExists(szOldPath)) {
			if (!LoadSalt(szPath)) return false;
			remove(szOldPath);
			if (!CreateSalt(szPath, true)) return false;
			
			return true;
		}
		if (!CreateSalt(szPath, false)) return false;
	
		return true;
	}
	
	if (!LoadSalt(szPath)) return false;

	return true;
}

bool CHashingSteamID::LoadSalt(char *szPath) {
	FILE* pFile = fopen(szPath, "rb");
	if (pFile == nullptr) {
		g_pSM->LogError(myself, "Failed to load salt in %s", szPath);
		return false;
	}
	
	fseek(pFile, 0L, SEEK_END);
	int nFileSize = ftell(pFile);
	fseek(pFile, 0L, SEEK_SET);
	
	if (fread(&g_sSalt[0], sizeof(char), nFileSize, pFile)) 
		cout << g_cPrefix << " Salt is loaded!\n" << endl;
	
	fclose(pFile);
	
	return true;
}

bool CHashingSteamID::CreateSalt(char *szPath, bool bSaltIsExist) {
	FILE *pFile = fopen(szPath, "wb");
	
	if (pFile == nullptr) {
		g_pSM->LogError(myself, "Failed to create salt in %s", szPath);
		return false;
	}
	
	if (!bSaltIsExist) GenerateSalt();
	
	fwrite(g_sSalt.c_str(), 1, strlen(g_sSalt.c_str()), pFile);
	fclose(pFile);
	
	return true;
}

void CHashingSteamID::LoadHashConfigSteamID() {
	m_pSteamIDHash.clear();
	
	char szPath[256];
	g_pSM->BuildPath(Path_SM, szPath, sizeof szPath, "data/protectcmds/steamid_hash.txt");
	
	if (libsys->PathExists(szPath)) {
		ifstream pFile(szPath);
		string sData;
		while (getline(pFile, sData)) {
			if (sData.size() < 22) continue;
			
			sData.erase(remove(sData.begin(),sData.end(),' '),sData.end());

			if (sData.find("//") != string::npos || sData.find("STEAM_0:") == string::npos) continue;
			
			int iEq = sData.find("=");
			
			if (iEq < 0) continue;
			
			string sSteam[2];
			
			sSteam[0].append(sData.substr(0, iEq).c_str());
			
			char sBuffer[1500];
			UTIL_SubStr(sData.c_str(), iEq+1, sData.size()-sSteam[0].size(), sBuffer, sizeof(sBuffer));
			sSteam[1].append(sBuffer);

			SteamID_Hash *shSteamID = new SteamID_Hash;
			
			UTIL_SubStr(sSteam[0].c_str(), 8, 1, sBuffer, sizeof(sBuffer));
			int bits = stoi(sBuffer);
			
			UTIL_SubStr(sSteam[0].c_str(), 10, sSteam[0].size()-10, sBuffer, sizeof(sBuffer));
			shSteamID->AccountID_Original = stoi(sBuffer)*2;
			if (bits) shSteamID->AccountID_Original++;
			
			UTIL_SubStr(sSteam[1].c_str(), 8, 1, sBuffer, sizeof(sBuffer));
			bits = stoi(sBuffer);
			
			UTIL_SubStr(sSteam[1].c_str(), 10, sSteam[1].size()-10, sBuffer, sizeof(sBuffer));
			shSteamID->AccountID_Replaced = stoi(sBuffer)*2;
			if (bits) shSteamID->AccountID_Replaced++;
			
			m_pSteamIDHash.push_back(shSteamID);
		}
		
		pFile.close();
	}
	else {
		FILE *pFile = fopen(szPath, "wb");
		if (pFile == nullptr) g_pSM->LogError(myself, "Failed to create steamid_hash.txt in %s", szPath);
		fclose(pFile);
	}
}

void CHashingSteamID::GenerateSalt() {
	srand( (unsigned) time(NULL) );
	for (int i = 0; i < SALT_BIT/8; ++i) g_sSalt += alphanum[rand() % (sizeof(alphanum) - 1)];
}

void CHashingSteamID::Load() {
	if (!GetSalt()) return;
	
	if (!g_pSetSteamIDDetour) {
		/*
			"CBaseServer::SetSteamID"
			{
				"library"	"engine"
				"linux"		"@_ZN11CBaseClient10SetSteamIDERK8CSteamID"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN11CBaseClient10SetSteamIDERK8CSteamID");
		g_pSetSteamIDDetour = DETOUR_CREATE_MEMBER(SetSteamID, pAddrFunc);
		if (g_pSetSteamIDDetour) g_pSetSteamIDDetour->EnableDetour();
		else {
			g_pSM->LogError(myself, "Failed to setup CBaseServer::SetSteamID detour");
			return;
		}
	}
	
	#ifdef ENGINE_CSS
	g_pServer = reinterpret_cast<IServer *>(g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "sv"));
	if (!g_pServer) {
		g_pSM->LogError(myself, "Failed to setup IServer detour");
		
		return;
	}
	
	void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZNK11CBaseClient12GetNetworkIDEv");
	if (!pAddrFunc) {
		g_pSM->LogError(myself, "Failed to setup CBaseClient::GetNetworkID detour");
		
		return;
	}
	GetNetworkID = reinterpret_cast<decltype(GetNetworkID)>(pAddrFunc);
	
	if (!g_pGetClientSteamIDByPlayerIndexDetour) {
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN14CVEngineServer29GetClientSteamIDByPlayerIndexEi");
		g_pGetClientSteamIDByPlayerIndexDetour = DETOUR_CREATE_MEMBER(GetClientSteamIDByPlayerIndexHook, pAddrFunc);
		if (g_pGetClientSteamIDByPlayerIndexDetour) g_pGetClientSteamIDByPlayerIndexDetour->EnableDetour();
		else {
			g_pSM->LogError(myself, "Failed to setup CVEngineServer::GetClientSteamIDByPlayerIndex detour");
			return;
		}
	}
	
	if (!g_pGetNetworkIDStringDetour) {
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZNK11CBaseClient18GetNetworkIDStringEv");
		g_pGetNetworkIDStringDetour = DETOUR_CREATE_MEMBER(GetNetworkIDStringHook, pAddrFunc);
		if (g_pGetNetworkIDStringDetour) g_pGetNetworkIDStringDetour->EnableDetour();
		else {
			g_pSM->LogError(myself, "Failed to setup CBaseClient::GetNetworkIDString detour");
			return;
		}
	}
	
	if (!g_pFilter_IsUserBannedDetour) {
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_Z19Filter_IsUserBannedRK8USERID_s");
		g_pFilter_IsUserBannedDetour = DETOUR_CREATE_STATIC(Filter_IsUserBannedHook, pAddrFunc);
		if (g_pFilter_IsUserBannedDetour) g_pFilter_IsUserBannedDetour->EnableDetour();
		else {
			g_pSM->LogError(myself, "Failed to setup Filter_IsUserBanned detour");
			return;
		}
	}
	#endif
	
	LoadHashConfigSteamID();
}
void CHashingSteamID::Unload() {
	if (g_pSetSteamIDDetour) {
		g_pSetSteamIDDetour->Destroy();
		g_pSetSteamIDDetour = nullptr;
	}
	
	#ifdef ENGINE_CSS
	if (g_pGetClientSteamIDByPlayerIndexDetour) {
		g_pGetClientSteamIDByPlayerIndexDetour->Destroy();
		g_pGetClientSteamIDByPlayerIndexDetour = nullptr;
	}
	
	if (g_pGetNetworkIDStringDetour) {
		g_pGetNetworkIDStringDetour->Destroy();
		g_pGetNetworkIDStringDetour = nullptr;
	}
	
	if (g_pFilter_IsUserBannedDetour) {
		g_pFilter_IsUserBannedDetour->Destroy();
		g_pFilter_IsUserBannedDetour = nullptr;
	}
	#endif
}