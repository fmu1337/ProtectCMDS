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

using namespace std;
using std::string;

#include "hashingsteamid.h"

class CBaseClient : public IGameEventListener2, public IClient {};

extern char g_cPrefix[];

CDetour *g_pSetSteamIDDetour = nullptr;
string g_sSalt;

SourceHook::List<SteamID_Hash *> m_pSteamIDHash;

static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

#define SALT_BIT 256

DETOUR_DECL_MEMBER1(SetSteamID, void *, CSteamID &, inputSteamID) {
	CBaseClient* pBaseClient = reinterpret_cast<CBaseClient*>(this);
	
	if (pBaseClient->IsFakeClient() || pBaseClient->IsHLTV()) return DETOUR_MEMBER_CALL(SetSteamID)(inputSteamID);
	
	CSteamID SteamID(inputSteamID);
	
	char szSteamID_old[64], szSteamID_new[64];
	bool bBits = (SteamID.GetAccountID() % 2) ? true : false;
	
	int iOriginalSteamID = (int32)(SteamID.GetAccountID()/2);
	
	g_pSM->Format(szSteamID_old, sizeof(szSteamID_old), "STEAM_0:%d:%u", bBits, iOriginalSteamID);
	
	int iHashSteam = 0;
	
	iHashSteam = CHashingSteamID::CheckHashConfigSteamID(bBits, iOriginalSteamID);

	if (iHashSteam == -1) {
		iHashSteam = CHashingSteamID::HashSteam(iOriginalSteamID);
		if (!bBits) iHashSteam--;
	}
	//else iHashSteam++;
	
	SteamID.SetAccountID(iHashSteam);
	
	bBits = (SteamID.GetAccountID() % 2) ? true : false;
	
	g_pSM->Format(szSteamID_new, sizeof(szSteamID_new), "STEAM_0:%d:%u", bBits, (int32)(SteamID.GetAccountID()/2));
	ConMsg(0, "%s [%s] changed to [%s]\n", g_cPrefix, szSteamID_old, szSteamID_new);
	
	inputSteamID = SteamID;
	
	return DETOUR_MEMBER_CALL(SetSteamID)(inputSteamID);
}

int CHashingSteamID::CheckHashConfigSteamID(bool bBits, int iOriginalSteamID) {
	SourceHook::List<SteamID_Hash *>::iterator iter;
	
	SteamID_Hash *pSteamID_Hash;
	
	for (iter=m_pSteamIDHash.begin(); iter != m_pSteamIDHash.end(); iter++) {
		pSteamID_Hash = (*iter);
		
		if (pSteamID_Hash->OriginalBits == bBits && pSteamID_Hash->iOriginalSteamID == iOriginalSteamID) {
			int iSteam = pSteamID_Hash->iSteamID*2;
			if (pSteamID_Hash->Bits) iSteam++;
			
			return iSteam;
		}
	}
	
	return -1;
}

int CHashingSteamID::HashSteam(int iSteamID) {
	string sSteamID = std::to_string(iSteamID);
	sSteamID.append(g_sSalt.c_str());
	string sHash = sha256(sSteamID);
	int iHashSteam = HexToDec(sHash, strlen(sSteamID.c_str())-strlen(g_sSalt.c_str()));
	
	return iHashSteam*2;
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

			if (sData.find("//") != string::npos) continue;
			
			if (sData.find("STEAM_0:") == string::npos) continue;
			
			int iEq = sData.find("=");
			
			if (iEq < 0) continue;
			
			string sSteam[2];
			
			sSteam[0].append(sData.substr(0, iEq).c_str());
			
			char sBuffer[1500];
			UTIL_SubStr(sData.c_str(), iEq+1, sData.size()-sSteam[0].size(), sBuffer, sizeof(sBuffer));
			sSteam[1].append(sBuffer);

			SteamID_Hash *shSteamID = new SteamID_Hash;
			
			UTIL_SubStr(sSteam[0].c_str(), 8, 1, sBuffer, sizeof(sBuffer));
			shSteamID->OriginalBits = stoi(sBuffer);
			
			UTIL_SubStr(sSteam[0].c_str(), 10, sSteam[0].size()-10, sBuffer, sizeof(sBuffer));
			shSteamID->iOriginalSteamID = stoi(sBuffer);
			
			UTIL_SubStr(sSteam[1].c_str(), 8, 1, sBuffer, sizeof(sBuffer));
			shSteamID->Bits = stoi(sBuffer);
			
			UTIL_SubStr(sSteam[1].c_str(), 10, sSteam[1].size()-10, sBuffer, sizeof(sBuffer));
			shSteamID->iSteamID = stoi(sBuffer);
			
			m_pSteamIDHash.push_back(shSteamID);
			
			
			/*Original
			
			sSteam[0].append(sData.substr(0, iEq).c_str());
			sSteam[1].append(sData.substr(iEq+1, sData.size()-sSteam[0].size()).c_str());

			SteamID_Hash *shSteamID = new SteamID_Hash;
			
			shSteamID->OriginalBits = stoi(sSteam[0].substr(8, 1));
			shSteamID->iOriginalSteamID = stoi(sSteam[0].substr(10, sSteam[0].size()-10));
			shSteamID->Bits = stoi(sSteam[1].substr(8, 1));
			shSteamID->iSteamID = stoi(sSteam[1].substr(10, sSteam[1].size()-10));
			
			m_pSteamIDHash.push_back(shSteamID);
			*/
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
	if (!g_pSetSteamIDDetour) {
		if (!GetSalt()) return;
		/*
			"CBaseServer::SetSteamID"
			{
				"library"		"engine"
				"linux"		"@_ZN11CBaseClient10SetSteamIDERK8CSteamID"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN11CBaseClient10SetSteamIDERK8CSteamID");
		g_pSetSteamIDDetour = DETOUR_CREATE_MEMBER(SetSteamID, pAddrFunc);
		if (g_pSetSteamIDDetour) {
			g_pSetSteamIDDetour->EnableDetour();
			LoadHashConfigSteamID();
		}
		else g_pSM->LogError(myself, "Failed to setup CBaseServer::SetSteamID detour");
	}
}
void CHashingSteamID::Unload() {
	if (g_pSetSteamIDDetour) {
		g_pSetSteamIDDetour->Destroy();
		g_pSetSteamIDDetour = nullptr;
	}
}