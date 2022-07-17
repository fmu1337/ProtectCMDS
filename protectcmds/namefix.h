#ifndef _NAMEFIX_H_
#define _NAMEFIX_H_

#include "ifix.h"

class CNameFix : public IFix {
	public:
		void Load();
		void Unload();
};

#endif