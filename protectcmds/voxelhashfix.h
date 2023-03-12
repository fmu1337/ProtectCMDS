#ifndef _VORTEXFIX_H_
#define _VORTEXFIX_H_

#include "ifix.h"

class CVoxelHashFix : public IFix {
	public:
		void Load();
		void Unload();
};

#endif