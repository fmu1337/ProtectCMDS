#ifndef _UPDATER_H_
#define _UPDATER_H_

#include <extension.h>

#include <string>

class CPCMDSUpdater {
	public:
		void Load(void *pExtensionManager);
		void Unload();
	
	public:
		void SetUrl(std::string sUrl);
		void SetExperimental(bool bExperimental);
			
		void Update();
		void CheckUpdates(int iLibrary);
		
		std::string GetFileFromHTTP(const char *pUrl);
		std::string GetSHA256FromFile(const char *pFile);
		std::string GenerateUrl(int iLibrary, std::string sHash);
			
		void LoadProtectCMDS();
		bool UnloadProtectCMDS();
			
		bool RemoveOldVersion();
			
		int GetUsePCMDSLibrary();
		bool IsLibraryLoaded(const char *pLibraryName);
			
		bool CheckDownloadFile(int iLibrary, std::string sFile);
		bool RemoveExtension(int iLibrary);
		bool WriteExtension(int iLibrary, std::string sFile);
		
	private:
		std::string m_sUrl;
		bool m_bExperimental = false;
};

#endif