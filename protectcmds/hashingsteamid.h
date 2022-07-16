#ifndef _HASHINGSTEAMID_H_
#define _HASHINGSTEAMID_H_

#include "ifix.h"

#include <string>

struct SteamID_Hash {
	short OriginalBits;
	int iOriginalSteamID;
	short Bits;
	unsigned int iSteamID;
};

class CHashingSteamID : public IFix {
	public:
		void Load();
		void Unload();
		
		static int HashSteam(int iSteamID);
		static int CheckHashConfigSteamID(bool bBits, int iOriginalSteamID);
		static int HexToDec(std::string sHash, int iLength);
		
	private:
		bool CreateSalt(char *szPath, bool bSaltIsExist);
		void GenerateSalt();	
		bool LoadSalt(char *szPath);
		void LoadHashConfigSteamID();
		bool GetSalt();
};

#endif