#ifndef _HASHINGSTEAMID_H_
#define _HASHINGSTEAMID_H_

#include "ifix.h"

#include <string>

struct SteamID_Hash {
	uint32_t AccountID_Original;
	uint32_t AccountID_Replaced;
};

class CHashingSteamID : public IFix {
	public:
		void Load();
		void Unload();
		
		static uint32_t HashSteam(uint32_t AccountID);
		static uint32_t CheckHashConfigSteamID(uint32_t AccountID);
		static int HexToDec(std::string sHash, int iLength);
		
	private:
		bool CreateSalt(char *szPath, bool bSaltIsExist);
		void GenerateSalt();	
		bool LoadSalt(char *szPath);
		void LoadHashConfigSteamID();
		bool GetSalt();
};

#endif