#include <string.h>
#include <iostream>

#include "smsdk_ext.h"
#include <tier1/interface.h>
#include "symbolmanager.h"

using namespace std;

CSymbolManager* g_pSymbolManager = nullptr;

CSymbolManager::CSymbolManager() {
	memset(m_pLibrary, 0, sizeof(m_pLibrary));
}

CSymbolManager::~CSymbolManager() {
	
}

void* CSymbolManager::Resolve(ESymbolModule symbolModule, const char* symbol) {
	if (!m_pLibrary[symbolModule]) return nullptr;
	
	return memutils->ResolveSymbol(m_pLibrary[symbolModule], symbol);
}

bool CSymbolManager::AddModule(ESymbolModule symbolModule, const char* moduleName) {
	void* pLibrary = dlopen(moduleName, RTLD_NOW);
	
	if(!pLibrary) {
		cout << "Модуль: " << moduleName << " не был загружен!" << endl;
		return false;
	}
	
	m_pLibrary[symbolModule] = pLibrary;
	
	return true;
}