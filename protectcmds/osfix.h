#ifndef _OSFIX_H_
#define _OSFIX_H_

#include "ifix.h"

class COSFix : public IFix {
	public:
		void Load();
		void Unload();
};

#endif