#include <iostream>
#include <cctype>

#include "updater.h"
#include <tier1/interface.h>
#include <tier0/dbg.h>

#include <IExtensionSys.h>
#include "http.h"
#include <helpers/sha256.h>
#include <helpers/inireader.h>

using namespace std;

#define EP1 0
#define EP2 1
#define LAST 2

#define MAX_LIBRARY 3

const char g_sLibraryPath[MAX_LIBRARY][63] = {
	"cstrike/addons/sourcemod/extensions/protectcmds.ext.1.ep1.so",
	"cstrike/addons/sourcemod/extensions/protectcmds.ext.2.ep1.so",
	"cstrike/addons/sourcemod/extensions/protectcmds.ext.so"
};

const char g_sUpdateExtension[MAX_LIBRARY][20] = {
	"ProtectCMDS EP1", "ProtectCMDS EP2", "ProtectCMDS"
};

#ifdef ENGINE_CSS34
const char g_sReleaseAsset[] = "protectcmds.css34.ext.so";
#elif ENGINE_CSS
const char g_sReleaseAsset[] = "protectcmds.css.ext.so";
#endif

IExtensionManager *g_pExtensionManager = nullptr;

bool CPCMDSUpdater::GetFileFromHTTP(const string &sUrl, string &sContent) {
	CHTTP HTTP;
	HTTP.SetTimeout(10000);

	if (HTTP.GET(sUrl, sContent)) return true;

	g_pSM->LogError(myself, "Failed to download %s: %s", sUrl.c_str(), HTTP.GetError().c_str());

	return false;
}

string CPCMDSUpdater::GetSHA256FromFile(const char *pFilename) {
	string sReturn;
	
	FILE *pFile = fopen(pFilename, "rb");
	if (pFile) {
		fseek(pFile, 0, SEEK_END);
		int iLength = ftell(pFile);
		rewind(pFile);
		
		string sBuffer;
		if (iLength > 0) {
			sBuffer.resize(iLength);
			sBuffer.resize(fread(&sBuffer[0], 1, iLength, pFile));
		}
		
		fclose(pFile);
		
		sReturn = sha256(sBuffer);
	}
	else g_pSM->LogError(myself, "Failed to open %s", pFilename);
	
	return sReturn;
}

bool CPCMDSUpdater::RemoveOldVersion() {
	bool bRemove = false;
	
	for (int i = EP1; i <= EP2; i++)
		if (libsys->PathExists(g_sLibraryPath[i])) 
			if (remove(g_sLibraryPath[i])) {
				g_pSM->LogError(myself, "Failed to delete file: %s", g_sLibraryPath[i]);
				bRemove = true;
			}

	return bRemove;
}

int CPCMDSUpdater::GetUsePCMDSLibrary() {
	for (int i = 0; i < MAX_LIBRARY; i++) 
		if (IsLibraryLoaded(g_sLibraryPath[i])) return i;
		
	return -1;
}

bool CPCMDSUpdater::RemoveExtension(int iLibrary) {
	if (!libsys->PathExists(g_sLibraryPath[iLibrary]) || remove(g_sLibraryPath[iLibrary])) {
	g_pSM->LogError(myself, "Failed delete file: %s", g_sLibraryPath[iLibrary]);
			
		return false;
	}
	
	return true;
}

bool CPCMDSUpdater::WriteExtension(int iLibrary, const string &sFile) {
	string sTempFile = g_sLibraryPath[iLibrary];
	sTempFile.append(".tmp");
	
	FILE *pFile = fopen(sTempFile.c_str(), "wb");
	if (!pFile) {
		g_pSM->LogError(myself, "Failed to open: %s", sTempFile.c_str());
		
		return false;
	}
	
	bool bWritten = fwrite(sFile.c_str(), 1, sFile.length(), pFile) == sFile.length();
	if (fclose(pFile)) bWritten = false;
	
	//rename() replaces the loaded library atomically, the running copy stays mapped
	if (bWritten && !rename(sTempFile.c_str(), g_sLibraryPath[iLibrary])) return true;
	
	remove(sTempFile.c_str());
	g_pSM->LogError(myself, "Failed to write extension: %s", g_sLibraryPath[iLibrary]);
	
	return false;
}

string CPCMDSUpdater::GetReleaseUrl(const char *pFile) {
	string sUrl = "https://github.com/" + m_sRepository + "/releases/";
	sUrl += m_bExperimental ? "download/experimental/" : "latest/download/";
	sUrl += pFile;
	
	return sUrl;
}

static bool ParseSHA256(const string &sContent, string &sHash) {
	if (sContent.length() < 64) return false;
	
	sHash = sContent.substr(0, 64);
	for (size_t i = 0; i < sHash.length(); i++) {
		if (!isxdigit((unsigned char)sHash[i])) return false;
		sHash[i] = tolower((unsigned char)sHash[i]);
	}
	
	return true;
}

void CPCMDSUpdater::CheckUpdates(int iLibrary) {
	string sAssetHash = g_sReleaseAsset;
	sAssetHash.append(".sha256");
	
	string sContent, sReleaseHash;
	if (!GetFileFromHTTP(GetReleaseUrl(sAssetHash.c_str()), sContent)) return;
	
	if (!ParseSHA256(sContent, sReleaseHash)) {
		g_pSM->LogError(myself, "Bad checksum file: %s", sAssetHash.c_str());
		
		return;
	}
	
	if (sReleaseHash == GetSHA256FromFile(g_sLibraryPath[iLibrary])) {
		ConMsg(0, "[ProtectCMDS Updater] Do you have the latest version of [%s]\n", g_sUpdateExtension[iLibrary]);
		
		return;
	}
	
	string sFile;
	if (!GetFileFromHTTP(GetReleaseUrl(g_sReleaseAsset), sFile)) return;
	
	if (sha256(sFile) != sReleaseHash) {
		g_pSM->LogError(myself, "Checksum mismatch for downloaded [%s], update skipped", g_sUpdateExtension[iLibrary]);
		
		return;
	}
	
	//Old EP1/EP2 libraries are replaced by protectcmds.ext.so
	if (iLibrary != LAST && !RemoveExtension(iLibrary)) return;
	
	if (!WriteExtension(LAST, sFile)) return;
	
	g_pSM->LogMessage(myself, "Successfully updated [%s]", g_sUpdateExtension[iLibrary]);
	g_pSM->LogMessage(myself, "Please, restart this server for load updated extension!");
}

void CPCMDSUpdater::LoadProtectCMDS() {
	char error[255];
	IExtension *pExtension = g_pExtensionManager->LoadExtension("protectcmds.ext.so", error, sizeof(error));
	
	if (!pExtension) {
		g_pSM->LogError(myself, "Failed loaded ProtectCMDS: %s", error);
		
		return;
	}
	
	g_pSM->LogMessage(myself, "Successfully loaded ProtectCMDS");
}

bool CPCMDSUpdater::UnloadProtectCMDS() {
	IProtectCMDS *pProtectCMDS = nullptr;
	Sys_LoadInterface(g_sLibraryPath[LAST], INTERFACE_PROTECTCMDS, nullptr, (void**)&pProtectCMDS);
	
	if (pProtectCMDS) {
		if (g_pExtensionManager->UnloadExtension((IExtension *)pProtectCMDS->GetExtensionPtr())) {
			g_pSM->LogMessage(myself, "Successfully unloaded ProtectCMDS extension!");
			
			return true;
		}
		
		g_pSM->LogError(myself, "Failed to unload ProtectCMDS extension!");
	}
	else g_pSM->LogError(myself, "Could not interface for ProtectCMDS, maybe old version");
	
	return false;
}

bool CPCMDSUpdater::IsLibraryLoaded(const char *pLibraryName) {
	void *pHandle = dlopen(pLibraryName, RTLD_NOW | RTLD_NOLOAD);
	
	if (pHandle) {
		// close - ??
		dlclose(pHandle);
		
		return true;
	}
	
	return false;
}

void CPCMDSUpdater::SetRepository(string sRepository) {
	m_sRepository = sRepository;
}

void CPCMDSUpdater::SetExperimental(bool bExperimental) {
	m_bExperimental = bExperimental;
}

void CPCMDSUpdater::Update() {	
	int iLibrary = GetUsePCMDSLibrary();
	
	if (iLibrary != -1) {
		CheckUpdates(iLibrary);
		RemoveOldVersion();
	}
	else ConMsg(0, "[ProtectCMDS] ProtectCMDS not loaded!\n");
}

void CPCMDSUpdater::Load(void *pExtensionManager) {
	if (!g_pExtensionManager) g_pExtensionManager = (IExtensionManager *)pExtensionManager;
	
	Update();
}

void CPCMDSUpdater::Unload() {
}