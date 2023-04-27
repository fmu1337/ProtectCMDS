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

#ifndef _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
 
 
/**
 * @file extension.h
 * @brief Sample extension code header.
 */

#include "smsdk_ext.h"
#include "iprotectcmds.h"
/* Include */
#include "protectcmds/voicefix.h"
#include "protectcmds/gameuifix.h"
#include "protectcmds/preconnecthook.h"
#include "protectcmds/sourcetvfix.h"
#include "protectcmds/boneindexbynamefix.h"
#include "protectcmds/takedamagefix.h"
#include "protectcmds/blockcmds.h"
#include "protectcmds/gamepacketfix.h"
#include "protectcmds/blockloadplugin.h"
#include "protectcmds/hashingsteamid.h"
#include "protectcmds/osfix.h"
#include "protectcmds/gamestatsfix.h"
#include "protectcmds/observerloopfix.h"
#include "protectcmds/filterpacket.h"
#include "protectcmds/firegameeventfix.h"
#include "protectcmds/namefix.h"
#include "protectcmds/voxelhashfix.h"
#include "protectcmds/specgotofix.h"
#include "protectcmds/vacpatch.h"

#include "updater/updater.h"

class CPCMDSUpdater;

class CProtectCMDS : public IProtectCMDS, public SDKExtension
{
public:
	/**
	 * @brief This is called after the initial loading sequence has been processed.
	 *
	 * @param error		Error message buffer.
	 * @param maxlength	Size of error message buffer.
	 * @param late		Whether or not the module was loaded after map load.
	 * @return			True to succeed loading, false to fail.
	 */
	virtual bool SDK_OnLoad(char *error, size_t maxlength, bool late);
	
	/**
	 * @brief This is called right before the extension is unloaded.
	 */
	virtual void SDK_OnUnload();

	/**
	 * @brief This is called once all known extensions have been loaded.
	 * Note: It is is a good idea to add natives here, if any are provided.
	 */
	virtual void SDK_OnAllLoaded();
	
	virtual void OnCoreMapStart(edict_t *pEdictList, int edictCount, int clientMax);
	
	bool LoadSettings();
	
	bool LoadLibraries();
	
	void *GetExtensionPtr();
	
	bool GetEngineNameLoaded(char *pEngine, int iMaxLength);

	/**
	 * @brief Called when the pause state is changed.
	 */
	//virtual void SDK_OnPauseChange(bool paused);

	/**
	 * @brief this is called when Core wants to know if your extension is working.
	 *
	 * @param error		Error message buffer.
	 * @param maxlength	Size of error message buffer.
	 * @return			True if working, false otherwise.
	 */
	//virtual bool QueryRunning(char *error, size_t maxlength);
public:
#if defined SMEXT_CONF_METAMOD
	/**
	 * @brief Called when Metamod is attached, before the extension version is called.
	 *
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @param late			Whether or not Metamod considers this a late load.
	 * @return				True to succeed, false to fail.
	 */
	virtual bool SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlength, bool late);

	/**
	 * @brief Called when Metamod is detaching, after the extension version is called.
	 * NOTE: By default this is blocked unless sent from SourceMod.
	 *
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @return				True to succeed, false to fail.
	 */
	//virtual bool SDK_OnMetamodUnload(char *error, size_t maxlength);

	/**
	 * @brief Called when Metamod's pause state is changing.
	 * NOTE: By default this is blocked unless sent from SourceMod.
	 *
	 * @param paused		Pause state being set.
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @return				True to succeed, false to fail.
	 */
	//virtual bool SDK_OnMetamodPauseChange(bool paused, char *error, size_t maxlength);
#endif
public: // IPluginsListener
	//virtual void OnPluginLoaded(IPlugin *plugin);
	//virtual void OnPluginUnloaded(IPlugin *plugin);
private:
	CNameFix *m_pNameFix						=	nullptr;
	CPCMDSUpdater *m_pUpdater					=	nullptr;
	CFilterPacket *m_pFilterPacket 				= 	nullptr;	
	CBlockLoadPluginFix *m_pBlockLoadPluginFix 	= 	nullptr;
	CBlockCmds *m_pBlockCmds 					= 	nullptr;
	CBoneIndexByNameFix *m_pBoneIndexByNameFix 	= 	nullptr;
	CGameStatsFix *m_pGameStatsFix 				= 	nullptr;
	CGameUIFix *m_pGameUIFix 					=	nullptr;
	CHashingSteamID *m_pHashingSteamID 			=	nullptr;
	CGamePacketFix *m_pGamePacketFix 			=	nullptr;
	CObServerLoopFix *m_pObServerLoopFix 		=	nullptr;
	COSFix *m_pOSFix 							=	nullptr;
	CPreConnectHook *m_pPreConnectHook			=	nullptr;
	CSourceTVFix *m_pSourceTVFix				=	nullptr;
	CTakeDamageFix *m_pTakeDamageFix			=	nullptr;
	CVoiceFix *m_pVoiceFix						=	nullptr;
	CFireGameEventFix *m_pFireGameEventFix		=	nullptr;
	CVoxelHashFix *m_pVoxelHashFix				=	nullptr;
	CSpecGotoFix *m_pSpecGotoFix				=	nullptr;
	CVACPatch	*m_pVACPatch					=	nullptr;
};

cell_t GetAbsoluteFrameTime(IPluginContext *pContext, const cell_t *params);

#endif // _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
