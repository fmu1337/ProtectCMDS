#ifndef _IFIX_H
#define _IFIX_H

#include <cstring>

class IFix {
	public:
		IFix();
	
	public:
		virtual void Load() =0;
		virtual void Unload() =0;
		
		char *GetName();
		void SetName(const char *pName);
		
	public:
		virtual void OnMapStart();
		
	public:
		char m_sName[255];
};

#endif