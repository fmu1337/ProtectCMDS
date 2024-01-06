/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Sample Extension
 * Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"

CProtectCMDS g_ProtectCMDS;		/**< Global singleton for extension's main interface */
SMEXT_LINK(&g_ProtectCMDS);
#ifdef ENGINE_CSS34
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CProtectCMDS, IProtectCMDS, INTERFACE_PROTECTCMDS, g_ProtectCMDS);
#endif

#include <iostream>

char g_cPrefix[] = "[ProtectCMDS]";

using namespace std;
using std::string;

#include <tier1/interface.h>
#ifdef ENGINE_CSS34
#include <dlls/iplayerinfo.h>
#elif ENGINE_CSS
#include <edict.h>
#include <game/server/iplayerinfo.h>
#endif

IPlayerInfoManager* g_pPlayerInfoManager = nullptr;
CGlobalVars* g_pGlobals = nullptr;

#include "helpers/symbolmanager.h"
#include "helpers/inireader.h"
#include "CDetour/detours.h"
#include "helpers/util.h"

#define ENGINE_MAX 4
const char sEngineName[ENGINE_MAX][19] = {
	"bin/engine_i686.so", "bin/engine_i486.so", "bin/engine_amd.so", "bin/engine_srv.so"
};

bool CProtectCMDS::GetEngineNameLoaded(char *pEngine, int iMaxLength) {
	for (int i = 0; i < ENGINE_MAX; i++) {
		void *pHandle = (void *)dlopen(sEngineName[i], RTLD_NOW | RTLD_NOLOAD);
		
		if (!pHandle) continue;
		
		strncpy(pEngine, sEngineName[i], iMaxLength);
		dlclose(pHandle);
		
		return true;
	}
	
	return false;
}

bool CProtectCMDS::LoadLibraries() {
	g_pSymbolManager = new CSymbolManager();
	
	char sEngine[20];

	if (!GetEngineNameLoaded(sEngine, sizeof(sEngine))) {
		g_pSM->LogError(myself, "Сould not find engine file loaded");
		
		return false;
	}
	
	if (!g_pSymbolManager->AddModule(ESymbolModule::ENGINE, sEngine)) return false;
	
	#ifdef ENGINE_CSS34	
	if (!g_pSymbolManager->AddModule(ESymbolModule::SERVER, "cstrike/bin/server_i486.so")) return false;
	if (!g_pSymbolManager->AddModule(ESymbolModule::VALVE_API, "bin/valve_api.so")) return false;
	Sys_LoadInterface("cstrike/bin/server_i486.so", INTERFACEVERSION_PLAYERINFOMANAGER, nullptr, (void**)&g_pPlayerInfoManager);
	#else
	if (!g_pSymbolManager->AddModule(ESymbolModule::SERVER, "cstrike/bin/server_srv.so")) return false;
	Sys_LoadInterface("cstrike/bin/server_srv.so", INTERFACEVERSION_PLAYERINFOMANAGER, nullptr, (void**)&g_pPlayerInfoManager);
	#endif

	if (!g_pPlayerInfoManager) {
		g_pSM->LogError(myself, "Сould not find interface: %s", INTERFACEVERSION_PLAYERINFOMANAGER);
		
		return false;
	}
	
	g_pGlobals = g_pPlayerInfoManager->GetGlobalVars();
	
	return true;
}

bool CProtectCMDS::LoadSettings() {
	if (g_pSettings) delete g_pSettings;
	
	char szPathSettings[256];
	g_pSM->BuildPath(Path_SM, szPathSettings, sizeof szPathSettings, "data/protectcmds/protectcmds.ini");
	
	g_pSettings = new INIReader(szPathSettings);
	
	if (g_pSettings->ParseError() < 0) {
		g_pSM->LogError(myself, "Can't load '%s'", szPathSettings);
		return false;
    }
	
	if (!g_pSettings->GetInteger("ProtectCMDS", "protectcmds_enable", 0)) {
		g_pSM->LogMessage(myself, "Disabled!\n");
		return false;
	}
	
	return true;
}

void CProtectCMDS::OnCoreMapStart(edict_t *pEdictList, int edictCount, int clientMax) {
	if (m_pBlockCmds) m_pBlockCmds->OnMapStart();
	if (m_pVoiceFix) m_pVoiceFix->OnMapStart();
}

sp_nativeinfo_t protectcmds_natives[] = {
	{"ProtectCMDS_GetAbsoluteFrameTime",    GetAbsoluteFrameTime},
	{nullptr, nullptr}
};

cell_t GetAbsoluteFrameTime(IPluginContext *pContext, const cell_t *params) {
	return sp_ftoc(g_pGlobals->absoluteframetime);
}

bool CProtectCMDS::SDK_OnLoad(char* error, size_t maxlength, bool late) {
	if (!LoadLibraries()) return false;
	if (!LoadSettings()) return false;
	
	CDetourManager::Init(g_pSM->GetScriptingEngine(), nullptr);
	
	IExtension *pExtensionManager = nullptr;
	SM_GET_IFACE(EXTENSIONMANAGER, pExtensionManager);
	
	if (!pExtensionManager) {
		g_pSM->Format(error, maxlength, "[ProtectCMDS] Could not interface IExtensionManager");
		
		return false;
	}
	
	if (g_pSettings->GetInteger("Updater", "protectcmds_updater_enable", 1)) {
		m_pUpdater = new CPCMDSUpdater;
		if (m_pUpdater) {
			m_pUpdater->SetUrl(g_pSettings->GetString("Updater", "protectcmds_updater_url", "http://protectcmds.gm-world.ru"));
			m_pUpdater->SetExperimental(g_pSettings->GetInteger("Updater", "protectcmds_updater_experimental", 0));
			m_pUpdater->Load((void *)pExtensionManager);
		}
		else g_pSM->LogError(myself, "Updater failed to load!");
	}
	
	if (g_pSettings->GetInteger("FilterPacket", "filterpacket_enable", 1)) {
		m_pFilterPacket = new CFilterPacket;
		if (m_pFilterPacket) m_pFilterPacket->Load();
		else g_pSM->LogError(myself, "Filter Packet failed to load!");
	}
	
	sharesys->AddNatives(myself, protectcmds_natives);
	
	sharesys->RegisterLibrary(myself, "ProtectCMDS");
	
	return true;
}

void CProtectCMDS::SDK_OnAllLoaded() {	
	if (g_pSettings->GetInteger("BlockCmds", "blockcmds_enable", 1)) {
		m_pBlockCmds = new CBlockCmds;
		if (m_pBlockCmds) {
			m_pBlockCmds->Load();
			m_pBlockCmds->SetMaxUserCmds(g_pSettings->GetInteger("blockCmds", "blockcmds_maxcommands", 30));
		}
		else g_pSM->LogError(myself, "BlockCmds failed to load!");
	}
	
	if (g_pSettings->GetInteger("SteamIDHashing", "steamidhashing_enable", 1)) {
		m_pHashingSteamID = new CHashingSteamID;
		if (m_pHashingSteamID) m_pHashingSteamID->Load();
		else g_pSM->LogError(myself, "SteamID Hashing failed to load!");
	}
	
	if (g_pSettings->GetInteger("GamePacketFix", "gamepacketfix_enable", 1)) {
		m_pGamePacketFix = new CGamePacketFix;
		if (m_pGamePacketFix) m_pGamePacketFix->Load();
		else g_pSM->LogError(myself, "GamePacketFix failed to load!");
	}
	
	m_pPreConnectHook = new CPreConnectHook;
	if (m_pPreConnectHook) m_pPreConnectHook->Load();
	else g_pSM->LogError(myself, "PreConnectHook failed to load!");
	
	m_pVoiceFix = new CVoiceFix;
	if (m_pVoiceFix) m_pVoiceFix->Load();
	else g_pSM->LogError(myself, "VoiceFix failed to load!");
	
	#ifdef ENGINE_CSS34
	m_pBlockLoadPluginFix = new CBlockLoadPluginFix;
	if (m_pBlockLoadPluginFix) m_pBlockLoadPluginFix->Load();
	else g_pSM->LogError(myself, "Block bad load plugin failed to load!");
	
	m_pBoneIndexByNameFix = new CBoneIndexByNameFix;
	if (m_pBoneIndexByNameFix) m_pBoneIndexByNameFix->Load();
	else g_pSM->LogError(myself, "BoneIndexByNameFix failed to load!");
	
	if (g_pSettings->GetInteger("GameStats", "gamestats_enable", 1)) {
		m_pGameStatsFix = new CGameStatsFix;
		if (m_pGameStatsFix) m_pGameStatsFix->Load();
		else g_pSM->LogError(myself, "GameStatsFix failed to load!");
	}
	
	m_pGameUIFix = new CGameUIFix;
	if (m_pGameUIFix) m_pGameUIFix->Load();
	
	m_pObServerLoopFix = new CObServerLoopFix;
	if (m_pObServerLoopFix) m_pObServerLoopFix->Load();
	else g_pSM->LogError(myself, "ObServerLiipFix failed to load!");
	
	if (g_pSettings->GetInteger("OSFix", "osfix_enable", 1)) {
		m_pOSFix = new COSFix;
		if (m_pOSFix) m_pOSFix->Load();
		else g_pSM->LogError(myself, "OSFix failed to load!");
	}
	
	m_pSourceTVFix = new CSourceTVFix;
	if (m_pSourceTVFix) m_pSourceTVFix->Load();
	else g_pSM->LogError(myself, "SourceTVFix failed to load!");
	
	m_pTakeDamageFix = new CTakeDamageFix;
	if (m_pTakeDamageFix) m_pTakeDamageFix->Load();
	else g_pSM->LogError(myself, "TakeDamageFix failed to load!");
	
	m_pFireGameEventFix = new CFireGameEventFix;
	if (m_pFireGameEventFix) m_pFireGameEventFix->Load();
	else g_pSM->LogError(myself, "FireGameEventFix failed to load!");
	
	m_pNameFix = new CNameFix;
	if (m_pNameFix) m_pNameFix->Load();
	else g_pSM->LogError(myself, "NameFix failed to load!");
	
	m_pVoxelHashFix = new CVoxelHashFix;
	if (m_pVoxelHashFix) m_pVoxelHashFix->Load();
	else g_pSM->LogError(myself, "VoxelHashFix failed to load!");
	
	m_pSpecGotoFix = new CSpecGotoFix;
	if (m_pSpecGotoFix) m_pSpecGotoFix->Load();
	else g_pSM->LogError(myself, "SpecGotoFix failed to load!");
	
	if (g_pSettings->GetInteger("VACPatch", "vacpatch_enable", 1)) {
		m_pVACPatch = new CVACPatch;
		if (m_pVACPatch) m_pVACPatch->Load();
		else g_pSM->LogError(myself, "VACPatch failed to load!");
	}
	
	/*if (g_pSettings->GetInteger("RCONAccess", "rconaccess_enable", 1)) {
		ConMsg(0, "rcon access load\n");
		m_pRCONAccess = new CRCONAccess;
		if (m_pRCONAccess) m_pRCONAccess->Load();
		else g_pSM->LogError(myself, "RCON Access failed to load!");
	}*/
	#endif
}

void CProtectCMDS::SDK_OnUnload() {
	if (m_pFilterPacket) m_pFilterPacket->Unload();
	if (m_pBlockLoadPluginFix) m_pBlockLoadPluginFix->Unload();
	if (m_pBlockCmds) m_pBlockCmds->Unload();
	if (m_pBoneIndexByNameFix) m_pBoneIndexByNameFix->Unload();
	if (m_pGameStatsFix) m_pGameStatsFix->Unload();
	if (m_pGameUIFix) m_pGameUIFix->Unload();
	if (m_pHashingSteamID) m_pHashingSteamID->Unload();
	if (m_pGamePacketFix) m_pGamePacketFix->Unload();
	if (m_pObServerLoopFix) m_pObServerLoopFix->Unload();
	if (m_pOSFix) m_pOSFix->Unload();
	if (m_pPreConnectHook) m_pPreConnectHook->Unload();
	if (m_pSourceTVFix) m_pSourceTVFix->Unload();
	if (m_pTakeDamageFix) m_pTakeDamageFix->Unload();
	if (m_pVoiceFix) m_pVoiceFix->Unload();
	if (m_pFireGameEventFix) m_pFireGameEventFix->Unload();
	if (m_pNameFix) m_pNameFix->Unload();
	
	if (m_pUpdater) {
		m_pUpdater->Unload();
		
		delete m_pUpdater;
		m_pUpdater = nullptr;
	}
	
	if (g_pSymbolManager) {
		delete g_pSymbolManager;
		g_pSymbolManager = nullptr;
	}
	
	if (g_pSettings) {
		delete g_pSettings;
		g_pSettings = nullptr;
	}
	
	g_pSM->LogMessage(myself, "Unloaded");
}

void *CProtectCMDS::GetExtensionPtr() {
	return myself;
}