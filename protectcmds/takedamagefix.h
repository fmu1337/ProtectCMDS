#ifndef _TAKEDAMAGEFIX_H_
#define _TAKEDAMAGEFIX_H_

#include "ifix.h"

class CTakeDamageFix : public IFix {
	public:
		void Load();
		void Unload();
};

#endif