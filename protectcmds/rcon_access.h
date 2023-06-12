#ifndef _RCON_ACCESS_H_
#define _RCON_ACCESS_H_

#include "ifix.h"

class CRCONAccess : public IFix {
	public:
		void Load();
		void Unload();
};

#endif