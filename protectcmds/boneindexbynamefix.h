#ifndef _BONEINDEXBYNAMEFIX_H_
#define _BONEINDEXBYNAMEFIX_H_

#include "ifix.h"

class CBoneIndexByNameFix : public IFix {
	public:
		void Load();
		void Unload();
};

#endif