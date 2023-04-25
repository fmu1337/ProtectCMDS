#define MAXPLAYERS 65

#include <fstream>
#include <algorithm>

#include <helpers/util.h>
#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>
#include <helpers/strtools.h>

#include <sh_list.h>

#include <iclient.h>
#include <igameevents.h>

#include "blockcmds.h"

using namespace std;

class CBaseClient : public IGameEventListener2, public IClient {};

extern char g_cPrefix[];
extern CGlobalVars* g_pGlobals;

CDetour *g_pExecuteStringCommandDetour = nullptr;
IForward *g_pExecuteStringCommand = nullptr;

int g_iMaxUserCmds = 30;
float g_fLastUserCmds[MAXPLAYERS+1];
int g_iUserCmds[MAXPLAYERS+1];

SourceHook::List<BlockCommands *> m_pBlockListCmds;

#define NUM_BAD_CMDS 49

char g_sBlockBadCmds[NUM_BAD_CMDS][100] = {
	"ai_test_los", "changelevel", "cl_fullupdate", "dbghist_addline", "dbghist_dump", "drawcross", "drawline",
	"dump_entity_sizes", "dump_globals", "dump_panels", "dump_terrain", "dumpcountedstrings", "dumpentityfactories",
	"dumpeventqueue", "dumpgamestringtable", "editdemo", "endround", "groundlist", "listmodels", "map_showspawnpoints",
	"mem_dump", "mp_dump_timers", "npc_ammo_deplete", "npc_heal", "npc_speakall", "npc_thinknow", "physics_budget",
	"physics_debug_entity", "physics_highlight_active", "physics_report_active", "physics_select", "q_sndrcn",
	"report_entities", "report_touchlinks", "report_simthinklist", "respawn_entities", "rr_reloadresponsesystems", 
	"scene_flush", "send_me_rcon", "snd_digital_surround", "snd_restart", "soundlist", "soundscape_flush",
	"sv_benchmark_force_start", "sv_findsoundname", "sv_soundemitter_filecheck", "sv_soundemitter_flush",
	"sv_soundscape_printdebuginfo", "wc_update_entity" 
};

DETOUR_DECL_MEMBER1(ExecuteStringCommand, bool, const char *, pCommand) {
	CBaseClient *pClient = reinterpret_cast<CBaseClient*>(this);
	int iClient = pClient->GetPlayerSlot() + 1;
	
	if (!pClient->IsConnected()) return false;
	
	if (strlen(pCommand) > 1023) return false;
	
	float fTime = g_pGlobals->curtime;
	
	if (g_fLastUserCmds[iClient] > fTime) {
		g_iUserCmds[iClient]++;
		if (g_iUserCmds[iClient] > g_iMaxUserCmds) {
			ConMsg(0, "%s %s has been kicked by spamming (max: %d)\n", g_cPrefix, pClient->GetClientName(), g_iMaxUserCmds);
			pClient->Disconnect("[ProtectCMDS] you have been kicked by spam");
			
			return false;
		}
	}
	else {
		g_iUserCmds[iClient] = 1;
		g_fLastUserCmds[iClient] = fTime + 1.0;
	}
	
	char pCmd[1024];
	strcpy(pCmd, pCommand);
	pCmd[1023] = '\0';
	
	for (int i = 0; i < NUM_BAD_CMDS; i++)
		if (StrContains(pCmd, g_sBlockBadCmds[i], true) == 0) return false;
	
	if (CBlockCmds::IsCommandBlock(pCmd)) {
		char Msg[1024];
		UTIL_Format(Msg, sizeof(Msg), "%s the command \"%s\" is blocked\n", g_cPrefix, pCmd);
		pClient->ClientPrintf(Msg);
		return false;
	}
	
	cell_t result = Pl_Continue;
	g_pExecuteStringCommand->PushCell(iClient);
	g_pExecuteStringCommand->PushString(pCmd);
	g_pExecuteStringCommand->Execute(&result);
	
	if (result >= Pl_Handled) return false;
	
	return DETOUR_MEMBER_CALL(ExecuteStringCommand)(pCommand);
}

void CBlockCmds::OnMapStart() {
	for (int i = 0; i < MAXPLAYERS+1; i++) g_fLastUserCmds[i] = g_pGlobals->curtime;
}

bool CBlockCmds::IsCommandBlock(char *pCmd) {
	TrimString(pCmd);

	SourceHook::List<BlockCommands *>::iterator iter;
	
	BlockCommands *pBlockCmds;
	
	for (iter=m_pBlockListCmds.begin(); iter != m_pBlockListCmds.end(); iter++) {
		pBlockCmds = (*iter);
		
		if (StrEqual(pCmd, pBlockCmds->pCmds, pBlockCmds->bSensitive)) return true;

		if (pBlockCmds->bArgs) {
			char sCmdBlock[1024];
			
			int iLen = strlen(pBlockCmds->pCmds);
			
			strcpy(sCmdBlock, (const char *)pBlockCmds->pCmds);
			
			sCmdBlock[iLen] = ' ';
			sCmdBlock[iLen+1] = '\0';
			
			if ( StrContains(pCmd, sCmdBlock, pBlockCmds->bSensitive) == 0) return true;
		}
	}
	
	return false;
}

void CBlockCmds::SetMaxUserCmds(int iMaxUserCmds) {
	g_iMaxUserCmds = iMaxUserCmds;
}

void CBlockCmds::LoadBlockListCmds() {
	char szPath[256];
	g_pSM->BuildPath(Path_SM, szPath, sizeof szPath, "data/protectcmds/blockcmds.txt");
	
	ifstream pFile(szPath);
	
	if (!pFile) {
		ConMsg(0, "%s Failed to load file %s\n", g_cPrefix, szPath);
		
		return;
	}
	
	m_pBlockListCmds.clear();
	string sData;
	
	ConMsg(0, "%s Block Cmds List: \n", g_cPrefix);
	
	while (getline(pFile, sData)) {
		sData.erase(remove(sData.begin(),sData.end(),' '),sData.end());
		
		if (sData.find("//") != string::npos) continue;
		
		if (sData.size() < 2) continue;
		
		TrimString((char *)sData.c_str());
		
		size_t iArgs = sData.find("[args]");
		size_t iSensitive = sData.find("[sensitive]");
		
		BlockCommands *pBlockCmds = new BlockCommands;
		
		if (iArgs != string::npos) pBlockCmds->bArgs = true;
		if (iSensitive != string::npos) pBlockCmds->bSensitive = true;
		
		int iSize = sData.size();
		
		if (pBlockCmds->bArgs && pBlockCmds->bSensitive) {
			iSize = iArgs;
			if (iArgs > iSensitive) iSize = iSensitive;
		}	
		else if (pBlockCmds->bArgs) iSize = iArgs;
		else if (pBlockCmds->bSensitive) iSize = iSensitive;
		
		pBlockCmds->pCmds = new char[iSize+1];
		
		memcpy(pBlockCmds->pCmds, sData.c_str(), iSize);
		pBlockCmds->pCmds[iSize] = '\0';
		
		ConMsg(0, "%s\n", pBlockCmds->pCmds);
		
		m_pBlockListCmds.push_back(pBlockCmds);
	}
	
	pFile.close();
}

void CBlockCmds::Load() {
	if (!g_pExecuteStringCommandDetour) {
		/*
			"CGameClient::ExecuteStringCommand"
			{
				"library"    "engine"
				"linux"        "@_ZN11CGameClient20ExecuteStringCommandEPKc"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN11CGameClient20ExecuteStringCommandEPKc");
		g_pExecuteStringCommandDetour = DETOUR_CREATE_MEMBER(ExecuteStringCommand, pAddrFunc);
		if (g_pExecuteStringCommandDetour) {
			g_pExecuteStringCommandDetour->EnableDetour();
			LoadBlockListCmds();
		}
		else g_pSM->LogError(myself, "Failed to setup CGameClient::ExecuteStringCommand detour");
	}
	
	if (!g_pExecuteStringCommand) g_pExecuteStringCommand = forwards->CreateForward("ProtectCMDS_OnClientCommand", ET_Hook, 2, nullptr, Param_Cell, Param_String);

}
void CBlockCmds::Unload() {
	if (g_pExecuteStringCommandDetour) {
		g_pExecuteStringCommandDetour->Destroy();
		g_pExecuteStringCommandDetour = nullptr;
	}
	
	if (g_pExecuteStringCommand) {
		forwards->ReleaseForward(g_pExecuteStringCommand);
		g_pExecuteStringCommand = nullptr;
	}
	
	m_pBlockListCmds.clear();
}