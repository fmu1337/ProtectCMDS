#ifndef _GAMESTATSFIX_H_
#define _GAMESTATSFIX_H_

#include "ifix.h"

class CGameStatsFix : public IFix {
	public:
		void Load();
		void Unload();
};

#endif