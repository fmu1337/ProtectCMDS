#include "ifix.h"

#include <iostream>

IFix::IFix() {
	for (int i = 0; i < (int)sizeof(m_sName); i++) m_sName[i] = '0';
}

char *IFix::GetName() {
	return m_sName;
}

void IFix::SetName(const char *pName) {
	strncpy(m_sName, pName, strlen(pName));
}