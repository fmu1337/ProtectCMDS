#ifndef _PRECONNECTHOOK_H_
#define _PRECONNECTHOOK_H_

#include "ifix.h"

class CPreConnectHook : public IFix {
	public:
		void Load();
		void Unload();
};

#endif