#ifndef CFAT_GLOBALS_H
#define CFAT_GLOBALS_H

#include <stdio.h>
#include "cfat_types.h"

// global variables
extern char* fs;            //pointer to the memory mapped file system
extern unsigned short* FAT; //pointer to the File Allocation Table
extern block* blocks;       //pointer to the blocks of the file system
extern int verbose;         //verbose flag

void logMessage(const char* format, ...);
void mapfs(FILE* filetomap);
void fsLoadedCheck();

#endif // CFAT_GLOBALS_H
