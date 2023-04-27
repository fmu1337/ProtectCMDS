#ifndef _VACPATCH_H_
#define _VACPATCH_H_

#include "ifix.h"

class CVACPatch : public IFix {
	public:
		void Load();
		void Unload();
};

#endif