#ifndef _OBSERVERLOOPFIX_H_
#define _OBSERVERLOOPFIX_H_

#include "ifix.h"

class CObServerLoopFix : public IFix {
	public:
		void Load();
		void Unload();
};

#endif