#ifndef _GAMEPACKETFIX_H_
#define _GAMEPACKETFIX_H_

#include "ifix.h"

class CGamePacketFix : public IFix {
	public:
		void Load();
		void Unload();
};

struct dataFragments_t
{
  int a1[48];
  void *file;
  char filename[260];
  char *buffer;
  unsigned int bytes;
  unsigned int bits;
  int transferID;
  bool isCompressed;
  unsigned int nUncompressedSize;
  bool asTCP;
  int numFragments;
  int ackedFragments;
};

#endif