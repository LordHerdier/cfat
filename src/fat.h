#ifndef CFAT_FAT_H
#define CFAT_FAT_H

#include "cfat_types.h"

unsigned short findFreeBlock();
unsigned short allocateNewBlock(unsigned short currentBlockIndex);
unsigned short findLastEntryInBlock(unsigned short blockindex);
unsigned short findLastBlockOfParent(short parentdirIndex);

#endif // CFAT_FAT_H
