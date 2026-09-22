#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "fat.h"
#include "globals.h"

unsigned short findFreeBlock() {
    unsigned short ret;
    // iterate through the FAT to find a free block
    for (ret = 0; ret < MAXBLOCKS; ret++) {
        if (FAT[ret] == 0) {
            return ret;
        }
    }

    // no free blocks found
    fprintf(stderr, "No free blocks found, exiting\n");
    exit(1);
}

unsigned short allocateNewBlock(unsigned short currentBlockIndex) {
    unsigned short freeBlock = findFreeBlock();
    FAT[currentBlockIndex] = freeBlock;
    FAT[freeBlock] = USHRT_MAX;
    return freeBlock;
}

unsigned short findLastEntryInBlock(unsigned short blockindex) {
    // returns the index of the last directory entry in the block
    // if none are found, returns USHRT_MAX

    int bitsFound = 0;  // number of bits found in the block

    // check if the file system is loaded
    fsLoadedCheck();

    // check if the block index in the FAT is valid
    if (blockindex >= MAXBLOCKS) {
        fprintf(stderr, "Invalid block index or size, cannot find free space in block\n");
        exit(1);
    }

    // get the pointer to the block
    block* blk = &blocks[blockindex];

    // iterate through the block to check if the last entry in the list can be found
    for (int i = 0; i < BLOCKSIZE; i += sizeof(dirEntry)) {
        dirEntry* entry = (dirEntry*)&blk->data[i];
        if (entry->isLast == LASTENTRY) {
            return i / sizeof(dirEntry);
        }
    }

    // check if it's an empty block
    for (int i = 0; i < BLOCKSIZE; i++) {
        if (blk->data[i] != 0) {
            bitsFound++;
        }
    }

    // if no bits are found, return 0
    if (bitsFound == 0) {
        return 0;
    }

    // if no last entry is found, return USHRT_MAX
    return USHRT_MAX;

}

unsigned short findLastBlockOfParent(short parentdirIndex) {
    unsigned short currentBlockIndex = parentdirIndex;
    while (FAT[currentBlockIndex] != USHRT_MAX) {
        currentBlockIndex = FAT[currentBlockIndex];
    }
    return currentBlockIndex;
}
