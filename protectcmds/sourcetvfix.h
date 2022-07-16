#ifndef _SOURCETVFIX_H_
#define _SOURCETVFIX_H_

#include "ifix.h"

class CSourceTVFix : public IFix {
	public:
		void Load();
		void Unload();
};

#endif