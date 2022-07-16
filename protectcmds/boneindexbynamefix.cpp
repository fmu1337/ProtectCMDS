#include <iostream>

#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>

#include <bone_setup.h>

#include "boneindexbynamefix.h"

extern char g_cPrefix[];

CDetour *g_pBoneIndexByNameDetour = nullptr;

DETOUR_DECL_MEMBER2(BoneIndexByName, int, CStudioHdr *, pStudioHdr, const char *, szName) {
	register int EAX asm("eax");
	
	if (!EAX) {
		ConMsg(0, "%s prevented server crash with CBaseAnimating::Studio_BoneIndexByName\n", g_cPrefix);
		return -1;
	}
	
	return DETOUR_MEMBER_CALL(BoneIndexByName)(pStudioHdr, szName);
}

void CBoneIndexByNameFix::Load() {
	if (!g_pBoneIndexByNameDetour) {
		/*
			"CBaseAnimating::Studio_BoneIndexByName"
			{
				"library"    "server"
				"linux"        "@_Z22Studio_BoneIndexByNamePK10CStudioHdrPKc"
			}
		*/
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::SERVER, "_Z22Studio_BoneIndexByNamePK10CStudioHdrPKc");
		g_pBoneIndexByNameDetour = DETOUR_CREATE_MEMBER(BoneIndexByName, pAddrFunc);
		if (g_pBoneIndexByNameDetour) g_pBoneIndexByNameDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CBaseAnimating::BoneIndexByName detour");
	}
}

void CBoneIndexByNameFix::Unload() {
	if (g_pBoneIndexByNameDetour) {
		g_pBoneIndexByNameDetour->Destroy();
		g_pBoneIndexByNameDetour = nullptr;
	}
}