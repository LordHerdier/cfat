#ifndef CFAT_DIRECTORY_H
#define CFAT_DIRECTORY_H

#include "cfat_types.h"

void createRootDirectory();
void _addDirectory(char* directoryName, dirEntry* parentDirEntry);
void addDirectory(char* directoryPath, dirEntry* parentDirEntry);
void listDirectory(dirEntry* parentDir);
void _printDirectoryTree(dirEntry* parentDir, int depth);
void printDirectoryTree(dirEntry* parentDir);
void removeDirectoryEntry(char* intpath, dirEntry* rootDir);

#endif // CFAT_DIRECTORY_H
