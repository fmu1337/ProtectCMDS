#ifndef _GAMEPACKETFIX_H_
#define _GAMEPACKETFIX_H_

#include "ifix.h"

class CGamePacketFix : public IFix {
	public:
		void Load();
		void Unload();
};

#endif