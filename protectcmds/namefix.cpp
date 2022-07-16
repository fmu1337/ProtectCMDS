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
extern const char g_cPrefix[];
 
CDetour *g_pClientSettingsChanged = nullptr;

DETOUR_DECL_MEMBER1(ClientSettingsChanged, void *, CBaseClient *, pPlayer) {
	
	//const char *pszName = pPlayer->GetPlayerName();
	
		//int iClient = pClient->GetPlayerSlot() + 1;
	//	const char *pName = pPlayer->GetClientName();
		
	//printf("Был вызов смены ника: %s\n", pName);
	
	return nullptr;
	
	//return DETOUR_MEMBER_CALL(ClientSettingsChanged)(pPlayer);
}

namespace NameFix {
	void SDK_OnAllLoaded(IGameConfig *g_pGameConf) {
		if (g_pClientSettingsChanged == nullptr) {
			g_pClientSettingsChanged = DETOUR_CREATE_MEMBER(ClientSettingsChanged, "CCSGameRules::ClientSettingsChanged");
			if (g_pClientSettingsChanged) g_pClientSettingsChanged->EnableDetour();
			else g_pSM->LogError(myself, "Failed to setup CCSGameRules::ClientSettingsChanged detour");
			LoadBlockCmdsFile();
		}
	}

	void SDK_OnUnload() {
		if (g_pClientSettingsChanged != nullptr) {
			g_pClientSettingsChanged->Destroy();
			g_pClientSettingsChanged = nullptr;
		}
	}
}