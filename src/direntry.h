#ifndef CFAT_DIRENTRY_H
#define CFAT_DIRENTRY_H

#include "cfat_types.h"

void setDirEntry(dirEntry* entry, char* name, char attributes,char create_time_tenth, short create_time, short create_date,
                 short last_access_date, short first_cluster_high, short last_write_time, short last_write_date,
                  short first_cluster_low, unsigned int size, char isLast);
void initializeNewDirectory(dirEntry* newDir, dirEntry* parentDir);
dirEntry* findEntryInDirectory(dirEntry* parentDir, char* entryName);
dirEntry* getNextEntry(dirEntry* currentEntry, dirEntry* parentDirEntry);
dirEntry* findParentFromPath(char* path, dirEntry* parentDir);
dirEntry* findEntryFromPath(char* intpath, dirEntry* parentDir);
void extract_path(const char *filepath, char *path);
void extract_filename(const char *filepath, char *filename);
void getFullPath(dirEntry* dir, char* path);
int isDirectoryEmpty(dirEntry* entry);
int getNumSubdirs(dirEntry* dir);

#endif // CFAT_DIRENTRY_H
