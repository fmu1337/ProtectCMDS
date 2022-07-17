#include <iostream>

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

IExtensionManager *g_pExtensionManager = nullptr;

string CPCMDSUpdater::GetFileFromHTTP(const char *pUrl) {
	CHTTP *pHTTP = new CHTTP();
	
	CHOST HOST = pHTTP->GetHost((char *)pUrl);
	string sHost = "Host: " + HOST.sHostName;
	pHTTP->AddHeader((char *)sHost.c_str());
	
	pHTTP->AddHeader((char *)"User-Agent: PCMDS_Updater");
	pHTTP->AddHeader((char *)"Connection: keep-alive");
	
	pHTTP->SetTimeout(900);
	
	CResponseHTTP Response = pHTTP->GET((char *)pUrl);
	
	delete pHTTP;
	
	return Response.Content;
}

string CPCMDSUpdater::GetSHA256FromFile(const char *pFilename) {
	string sReturn;
	
	FILE *pFile = fopen(pFilename, "rb");
	if (pFile) {
		fseek(pFile, 0, SEEK_END);
		int iLength = ftell(pFile);
		rewind(pFile);
		
		char *pBuffer = new char[iLength];
		int iRet = fread(pBuffer, 1, iLength, pFile);
		
		fclose(pFile);
		
		string sBuffer;
		sBuffer.append(pBuffer, iLength);
		free(pBuffer);
		
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

bool CPCMDSUpdater::WriteExtension(int iLibrary, string sFile) {
	string sTempFile = g_sLibraryPath[iLibrary];
	sTempFile.append(".tmp");
	FILE *pFile = fopen(sTempFile.c_str(), "wb");
	
	if (pFile) {
		fwrite(sFile.c_str(), 1, sFile.length(), pFile);
		fclose(pFile);
	}
	else g_pSM->LogError(myself, "Failed open: %s", g_sLibraryPath[iLibrary]);
	
	if (!rename(sTempFile.c_str(), g_sLibraryPath[iLibrary])) return true;

	g_pSM->LogError(myself, "Failed to write extension: %s", g_sLibraryPath[iLibrary]);
	
	return true;
}

string CPCMDSUpdater::GenerateUrl(int iLibrary, string sHash) {
	string sUrl = m_sUrl;
	sUrl.append((char *)"/index.php?&hash=");
	sUrl.append(sHash);
	if (m_bExperimental) sUrl += "&experimental=yes";
	
	return sUrl;
}

bool CPCMDSUpdater::CheckDownloadFile(int iLibrary, string sFile) {
	if (strcmp(sFile.c_str(), "updated")) {
		string sDownloadedFileHash = sha256(sFile);
		
		string sUrl = GenerateUrl(iLibrary, sDownloadedFileHash);
		
		string sAnswer = GetFileFromHTTP(sUrl.c_str());
		if (strcmp(sAnswer.c_str(), "updated")) {
			g_pSM->LogError(myself, "Failed downloaded new version of [%s]", g_sUpdateExtension[iLibrary]);
			
			return false;
		}
	}
	
	return true;
}

void CPCMDSUpdater::CheckUpdates(int iLibrary) {
	string sHash = GetSHA256FromFile(g_sLibraryPath[iLibrary]);
	
	string sUrl = GenerateUrl(iLibrary, sHash);
	
	string sFile = GetFileFromHTTP(sUrl.c_str());
	
	if (!sFile.length() || sFile.length() < 2) {
		g_pSM->LogError(myself, "Failed get data from: %s", g_pSettings->GetString("ProtectCMDS_Updater", "protectcmds_updater_url", "0").c_str());
		
		return;
	}
	
	if (sFile.length() == 7 && !strcmp(sFile.c_str(), "updated")) {
		ConMsg(0, "[ProtectCMDS Updater] Do you have the latest version of [%s]\n", g_sUpdateExtension[iLibrary]);
		
		return;
	}
	
	if (memcmp(sFile.c_str(), "ok", 2)) {
		g_pSM->LogError(myself, "Failed to get file from: %s", g_pSettings->GetString("ProtectCMDS_Updater", "protectcmds_updater_url", "0").c_str());
		
		return;
	}

	memmove((void *)sFile.c_str(), (void *)(sFile.c_str()+2), sFile.length() - 2);
	sFile.resize(sFile.length()-2);	
	
	if (!CheckDownloadFile(iLibrary, sFile)) return;
	
	//if (iLibrary == LAST) UnloadProtectCMDS();
		
	if (!RemoveExtension(iLibrary)) return;
	
	if (iLibrary <= LAST) 
		if (!WriteExtension(LAST, sFile)) return;
		
	//if (iLibrary == LAST) LoadProtectCMDS();
		
	g_pSM->LogMessage(myself, "Successfully updated [%s]", g_sUpdateExtension[iLibrary]);
	if (iLibrary <= LAST)
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
		dlclose(pHandle);
		
		return true;
	}
	
	return false;
}

void CPCMDSUpdater::SetUrl(string sUrl) {
	m_sUrl = sUrl;
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