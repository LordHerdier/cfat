#ifndef CFAT_FILE_H
#define CFAT_FILE_H

#include <stdio.h>
#include "cfat_types.h"

void createEmptyFile(char* filename, dirEntry* parent);
void _addFile(char* sourceFilename, char* intpath, dirEntry* parentDir);
void addFile(char* filename, char* intpath, dirEntry* parentDir);
void catFile(char* intpath, dirEntry* parentDir);
void touchFile(char* intpath, dirEntry* parentDir);
void writeBlockToFile(FILE* f, unsigned short block, unsigned short numBytes);
void _extractFile(dirEntry* file);
void extractFile(char *intpath, dirEntry *parentDir);

#endif // CFAT_FILE_H
