#ifndef _SPECGOTOFIX_H_
#define _SPECGOTOFIX_H_

#include "ifix.h"

class CSpecGotoFix : public IFix {
	public:
		void Load();
		void Unload();
};

#endif