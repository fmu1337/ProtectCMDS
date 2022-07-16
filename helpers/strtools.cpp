#include <cstring>
#include <tier1/strtools.h>
#include <iostream>

#include "strtools.h"

using namespace std;

int hex_dump(unsigned char *data, int len) {
register int i;
register int j;
	for(i = 0; i < len; i += 16) {
		printf("\n");
		for(j = 0; j < 16; j++) {
			if(i+j < len) printf("%02X ", data[i+j]);
			else printf(" ");
		}
		printf(" | ");

		for(j = 0; j < 16; j++) {
			unsigned char character = (i+j < len) ? data[i+j] : ' ';
			if((character <= 0x20) || (character >= 0x7e)) character = '.';
			printf("%c", character);
		}
	}
	printf("\n");
	return 0;
}

void CharToLower(char *CStr) {
	int iLen = strlen(CStr);
	char c;
	for (int i = 0; i < iLen; i++)  CStr[i] = tolower(CStr[i]);
}

bool StrEqual(const char *Str1, const char *Str2, bool bCaseSensitive) {
	char CString1[1024], CString2[1024];
	
	strcpy(CString1, Str1);
	strcpy(CString2, Str2);
	
	if (!bCaseSensitive) {
		CharToLower(CString1);
		CharToLower(CString2);
	}
	
	if ( !strcmp(CString1, CString2) ) return true;
	
	return false;
}

int Contains(const char *Str1, const char *Str2) {
	//ConMsg(0, "Строки: %s и %s\n", Str1, Str2);
	int iResult = -1;
	int iLen = strlen(Str1);
	unsigned int iLenStr2 = strlen(Str2);
	
	unsigned int j = 0; //Количество совпадений
	for (int i = 0; i < iLen; i++) {		
		//ConMsg(0, "Сравнение %c == %c ? || j = %d\n", Str1[i], Str2[j], j);
		
		//Совпадение
		if (Str1[i] == Str2[j]) {
			iResult = i;
			//ConMsg(0, "Сравнение %c == %c ? || j = %d\n", Str1[i], Str2[j], j);
			j++;
			unsigned int jj = j;
			for (int ii = i+1; ii < iLen; ii++) {
				//ConMsg(0, "Сравнение %c == %c ? || jj = %d\n", Str1[ii], Str2[jj], jj);
				
				if (Str1[ii] == Str2[jj]) {
					jj++;
					if (iLenStr2 == jj) return iResult;
				}
				else break;
				
				if (Str2[jj] == '\0') break;
				
				i = ii;
			}
			
			j = 0;
		}
		
		if (Str2[j] == '\0') break;
	}
	
	/*ConMsg(0, "Размеры: strlen(Str2) = %d || j = %d\n", strlen(Str2), j);
	if (strlen(Str2) != j) iResult = -1;
	
	ConMsg(0, "return iResult = %d\n", iResult);
	
	return iResult;*/
	
	return -1;
}

int StrContains(const char *Str1, const char *Str2, bool bCaseSensitive) {
	int iResult = 1;
	if (!bCaseSensitive) {
		char CString1[1024], CString2[1024];
	
		strcpy(CString1, Str1);
		strcpy(CString2, Str2);
		
		CharToLower(CString1);
		CharToLower(CString2);
		
		return Contains(CString1, CString2);
	}
	
	return Contains(Str1, Str2);
}

void TrimString(char *s) {
	int i=0,j;
	int iLen = strlen(s);
	while( (s[i] == ' ') || (s[i] == '\t') || (s[i] == '\n') || (s[i] == '\r')) i++;
	if (i>0) {
		for(j=0; j < iLen; j++)  s[j]=s[j+i];
		s[j] = '\0';
	}
	i=iLen-1;
	while( (s[i] == ' ') || (s[i] == '\t') || (s[i] == '\n') || (s[i] == '\r'))  i--;
	if (i < (iLen-1)) s[i+1] = '\0';
}