#ifndef _BLOCKCMDS_H_
#define _BLOCKCMDS_H_

#include "ifix.h"

struct BlockCommands {
	char *pCmds;
	bool bArgs = false;
	bool bSensitive = false;
};

class CBlockCmds : public IFix {
	public:
		void Load();
		void Unload();
		void SetMaxUserCmds(int iMaxUserCmds);
		
		static bool IsCommandBlock(char *pCmd);
		
	public:
		virtual void OnMapStart() override;
		
	private:
		void LoadBlockListCmds();
};

#endif