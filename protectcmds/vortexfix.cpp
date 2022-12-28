#include <iostream>
using namespace std;

#include <CDetour/detours.h>
#include <helpers/symbolmanager.h>

#include "vortexfix.h"

#include <edict.h>
#include <vector.h>
#include <ispatialpartition.h>
#include <bitvec.h>

union Voxel_t {
	struct {
		unsigned int x:11;
		unsigned int y:11;
		unsigned int z:10;
	} bitsVoxel;
	unsigned int uiVoxel;  
};

extern char g_cPrefix[];

CDetour *g_pEnumerateElementsInBoxDetour = nullptr;

DETOUR_DECL_MEMBER6(EnumerateElementsInBoxHook, bool, SpatialPartitionListMask_t, listMask, Voxel_t, vmin, Voxel_t, vmax, const Vector &, mins, const Vector *, maxs, IPartitionEnumerator *, pIterator) {
	Voxel_t vdelta;
	vdelta.uiVoxel = vmax.uiVoxel - vmin.uiVoxel;
	int cx = vdelta.bitsVoxel.x;
	int cy = vdelta.bitsVoxel.y;
	int cz = vdelta.bitsVoxel.z;
	
	if (cx > 500 || cy > 500 || cz > 500) return false;
	
	return DETOUR_MEMBER_CALL(EnumerateElementsInBoxHook)(listMask, vmin, vmax, mins, maxs, pIterator);
}

void CVortexFix::Load() {
	if (!g_pEnumerateElementsInBoxDetour) {
		/*
			"CVoxelHash:EnumerateElementsInBox"
			{
				"library"		"engine"
				"linux"		"@_ZN10CVoxelHash22EnumerateElementsInBoxEi7Voxel_tS0_RK6VectorS3_P20IPartitionEnumerator"
			}
		*/
		
		void *pAddrFunc = g_pSymbolManager->Resolve(ESymbolModule::ENGINE, "_ZN10CVoxelHash22EnumerateElementsInBoxEi7Voxel_tS0_RK6VectorS3_P20IPartitionEnumerator");
		g_pEnumerateElementsInBoxDetour = DETOUR_CREATE_MEMBER(EnumerateElementsInBoxHook, pAddrFunc);
		if (g_pEnumerateElementsInBoxDetour) g_pEnumerateElementsInBoxDetour->EnableDetour();
		else g_pSM->LogError(myself, "Failed to setup CVoxelHash:EnumerateElementsInBox detour");
	}
}

void CVortexFix::Unload() {
	if (g_pEnumerateElementsInBoxDetour) {
		g_pEnumerateElementsInBoxDetour->Destroy();
		g_pEnumerateElementsInBoxDetour = nullptr;
	}
}