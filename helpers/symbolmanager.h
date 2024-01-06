#ifndef _SYMBOLMANAGER_H
#define _SYMBOLMANAGER_H

enum ESymbolModule {
	ENGINE = 0,
	SERVER,
	VALVE_API,
	LAST
};

class CSymbolManager {
	public:
		CSymbolManager();
		~CSymbolManager();
		
	public:
		void* Resolve(ESymbolModule symbolModule, const char* symbol);
		bool AddModule(ESymbolModule symbolModule, const char* moduleName);
		
	private:
		void* m_pLibrary[ESymbolModule::LAST];
};

extern CSymbolManager* g_pSymbolManager;

#endif