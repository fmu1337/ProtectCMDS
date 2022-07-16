#ifndef _VOICEFIX_H_
#define _VOICEFIX_H_

#include "ifix.h"

class CVoiceFix : public IFix {
	public:
		void Load();
		void Unload();
		
		void SetMethod(int iMethod);
		
	private:
		int m_iMethod = 3;
};

#endif