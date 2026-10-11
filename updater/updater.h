#ifndef _UPDATER_H_
#define _UPDATER_H_

#include <extension.h>

#include <string>

//GitHub repository with releases, CI passes its own (forks update from themselves)
#ifndef PCMDS_REPOSITORY
#define PCMDS_REPOSITORY "fmu1337/protectcmds"
#endif

class CPCMDSUpdater {
	public:
		void Load(void *pExtensionManager);
		void Unload();
	
	public:
		void SetRepository(std::string sRepository);
		void SetExperimental(bool bExperimental);
			
		void Update();
		void CheckUpdates(int iLibrary);
		
		bool GetFileFromHTTP(const std::string &sUrl, std::string &sContent);
		std::string GetSHA256FromFile(const char *pFile);
		std::string GetReleaseUrl(const char *pFile);
			
		void LoadProtectCMDS();
		bool UnloadProtectCMDS();
			
		bool RemoveOldVersion();
			
		int GetUsePCMDSLibrary();
		bool IsLibraryLoaded(const char *pLibraryName);
			
		bool RemoveExtension(int iLibrary);
		bool WriteExtension(int iLibrary, const std::string &sFile);
		
	private:
		std::string m_sRepository;
		bool m_bExperimental = false;
};

#endif