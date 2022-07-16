#ifndef _FIREGAMEEVENTFIX_H_
#define _FIREGAMEEVENTFIX_H_

#include "ifix.h"

class CFireGameEventFix : public IFix {
	public:
		void Load();
		void Unload();
};

#endif