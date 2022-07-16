#ifndef _BLOCKBADLOADPLUGIN_H_
#define _BLOCKBADLOADPLUGIN_H_

#include "ifix.h"

class CBlockLoadPluginFix : public IFix {
	public:
		void Load();
		void Unload();
};

#endif