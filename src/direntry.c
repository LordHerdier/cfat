#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include "direntry.h"
#include "globals.h"

void setDirEntry(dirEntry* entry, char* name, char attributes,char create_time_tenth, short create_time, short create_date,
                 short last_access_date, short first_cluster_high, short last_write_time, short last_write_date,
                  short first_cluster_low, unsigned int size, char isLast)
{
    strcpy(entry->name, name);
    entry->attributes = attributes;
    entry->create_time_tenth = create_time_tenth;
    entry->create_time = create_time;
    entry->create_date = create_date;
    entry->last_access_date = last_access_date;
    entry->first_cluster_high = first_cluster_high;
    entry->last_write_time = last_write_time;
    entry->last_write_date = last_write_date;
    entry->first_cluster_low = first_cluster_low;
    entry->size = size;
    entry->isLast = isLast;
}

void initializeNewDirectory(dirEntry* newDir, dirEntry* parentDir) {
    short newDirBlockIndex = USHRT_MAX;       // index on the FAT of new dir block
    short parentDirBlockIndex = USHRT_MAX;    // index on the FAT of parent dir block
    block* newDirBlock = NULL;                // pointer to the new dir block
    block* parentDirBlock = NULL;             // pointer to the parent dir block
    dirEntry* dotEntry = NULL;                // pointer to the . entry in the new dir block
    dirEntry* dotdotEntry = NULL;             // pointer to the .. entry in the new dir block

    // set the block indexes
    newDirBlockIndex = newDir->first_cluster_low;
    parentDirBlockIndex = parentDir->first_cluster_low;

    // set the block pointers
    newDirBlock = &blocks[newDirBlockIndex];
    parentDirBlock = &blocks[parentDirBlockIndex];

    // set the pointers to the . and .. entries in the new directory block
    dotEntry = (dirEntry*)&newDirBlock->data[0];
    dotdotEntry = dotEntry + 1;

    // zero the block
    bzero(newDirBlock, BLOCKSIZE);

    logMessage("New directory block zeroed\n");

    // set the . entry
    setDirEntry(dotEntry, ".", ATTR_DIRECTORY, newDir->create_time_tenth, newDir->create_time, newDir->create_date,
                newDir->last_access_date, newDir->first_cluster_high, newDir->last_write_time, newDir->last_write_date,
                newDir->first_cluster_low, newDir->size, NOTLASTENTRY);

    // set the .. entry
    setDirEntry(dotdotEntry, "..", ATTR_DIRECTORY, parentDir->create_time_tenth, parentDir->create_time, parentDir->create_date,
                parentDir->last_access_date, parentDir->first_cluster_high, parentDir->last_write_time, parentDir->last_write_date,
                parentDir->first_cluster_low, parentDir->size, LASTENTRY);

    logMessage("Set . and .. entries in new directory block\n");

    logMessage("New directory finished initializing\n");
}

dirEntry* getNextEntry(dirEntry* currentEntry, dirEntry* parentDirEntry) {
    unsigned short isLast;              // flag to indicate if the current entry is the last entry in the BLOCK, NOT DIR
    unsigned short currentBlockIndex;   // index on the FAT of the current working block
    unsigned short currentEntryIndex;   // index of the current entry in the block
    unsigned short entryFound;          // flag to indicate if the entry was found in the block
    block* currentBlock;                // pointer to the current block
    dirEntry* nextEntry;                // pointer to the next entry in the block

    // check if the file system is loaded
    fsLoadedCheck();

    // early return if it's the last entry in the directory
    if (currentEntry->isLast == LASTENTRY) {
        logMessage("Error: Current entry is the last in the directory. No next entry available.\n");
        return NULL;
    }

    // logMessage("Starting search for the next directory entry after '%s'.\n", currentEntry->name);

    // get the working block
    currentBlockIndex = parentDirEntry->first_cluster_low;
    currentBlock = &blocks[currentBlockIndex];

    // logMessage("\tSearching in the block starting at index %u.\n", currentBlockIndex);

    // find the current entry in the block
    entryFound = 0;
    while (entryFound == 0) {
        // search for the entry's index in the block by name
        for (currentEntryIndex = 0; currentEntryIndex < BLOCKSIZE; currentEntryIndex += sizeof(dirEntry)) {
            nextEntry = (dirEntry*)&currentBlock->data[currentEntryIndex];
            char* entryName = nextEntry->name;

            // compare the names of the entries
            if (strcmp(entryName, currentEntry->name) == 0) {
                // found the entry
                // logMessage("\tEntry '%s' found in block at index %u.\n", entryName, currentEntryIndex);
                entryFound = 1;
                break;
            }
        }

        // check if the entry was found in the block
        if (entryFound == 0) {
            // check if there's another block to search
            if (FAT[currentBlockIndex] == USHRT_MAX) {
                // no more blocks in the directory
                logMessage("\tError: Entry '%s' not found in block. No more blocks in directory.\n", currentEntry->name);
                return NULL;
            }
            // get the next block in the FAT
            // logMessage("\tEntry '%s' not found in current block. Moving to next block in FAT.\n", currentEntry->name);
            currentBlockIndex = FAT[currentBlockIndex];
            currentBlock = &blocks[currentBlockIndex];
            // logMessage("\tSearching in the new block starting at index %u.\n", currentBlockIndex);
        }
    }

    // last entry in block?
    if (currentEntryIndex == BLOCKSIZE - sizeof(dirEntry)) {
        isLast = LASTENTRY;
    }
    else {
        isLast = NOTLASTENTRY;
    }

    // if not last entry in block, return the next entry
    if (isLast == NOTLASTENTRY) {
        // return the next entry in the block
        nextEntry = (dirEntry*)&currentBlock->data[currentEntryIndex + sizeof(dirEntry)];
        // logMessage("\tNext entry '%s' found in the current block.\n", nextEntry->name);

        return nextEntry;
    }

    // current entry is the last in the block
    // get the next block in the FAT
    currentBlockIndex = FAT[currentBlockIndex];
    currentEntryIndex = 0;

    // check if current block is the last
    if (currentBlockIndex == USHRT_MAX) {
        return NULL;
        logMessage("\tError: Reached the last block in FAT. No next entry available.\n");
    }

    // get the entry
    nextEntry = (dirEntry*)&blocks[currentBlockIndex].data[currentEntryIndex];
    logMessage("\tNext entry '%s' found in the next block at index %u.\n", nextEntry->name, currentBlockIndex);

    // return the entry
    return nextEntry;
}

dirEntry* findEntryInDirectory(dirEntry* parentDir, char* entryName) {
    unsigned short currentDirBlockIndex = USHRT_MAX;     // index on the FAT of the current working block
    block* currentDirBlock = NULL;                       // pointer to the current directory block
    dirEntry* currentDirEntry = NULL;                    // pointer to the current directory entry
    unsigned short currentDirEntryIndex = USHRT_MAX;     // index of the current directory entry in the block

    // check if the file system is loaded
    fsLoadedCheck();

    // set the current block index to the first cluster of the parent directory
    currentDirBlockIndex = parentDir->first_cluster_low;

    // get the pointer to the current directory block
    currentDirBlock = &blocks[currentDirBlockIndex];

    // set the pointer to the first entry in the block
    currentDirEntryIndex = 0;
    currentDirEntry = (dirEntry*)&currentDirBlock->data[currentDirEntryIndex];

    // iterate through the directory entries in the block until the last entry is found
    while (currentDirEntry->isLast != LASTENTRY && currentDirEntry != NULL) {
        // check if the entry name matches the name we are looking for
        if (strcmp(currentDirEntry->name, entryName) == 0) {
            return currentDirEntry;
        }

        // get the next entry in the directory
        currentDirEntry = getNextEntry(currentDirEntry, parentDir);
    }

    // make sure we still have a currentDirEntry
    if (currentDirEntry == NULL) {
        return NULL;
    }

    // check if the last entry is the one we are looking for
    if (strcmp(currentDirEntry->name, entryName) == 0) {
        return currentDirEntry;
    }

    return NULL;
}

dirEntry* findParentFromPath(char* path, dirEntry* parentDir) {
    char* token;                            // token for strtok
    char filePath[MAXPATH];                 // path
    dirEntry* currentDir = parentDir;       // start from the parent directory
    dirEntry* foundEntry = NULL;            // pointer to the found entry

    // check if the file system is loaded
    fsLoadedCheck();

    // copy the directory path to a local variable
    strcpy(filePath, path);

    // tokenize the path and find the directory to add the file to
    token = strtok(filePath, "/");
    while (token != NULL) {
        dirEntry* foundEntry = findEntryInDirectory(currentDir, token);
        if (foundEntry == NULL) {
            fprintf(stderr, "Directory, %s, does not exist\n", token);
            return NULL;
        }
        // move to the next directory in the path
        currentDir = foundEntry;
        token = strtok(NULL, "/");
    }

    return currentDir;
}

void extract_path(const char *filepath, char *path) {
    const char *last_slash = strrchr(filepath, '/');
    if (last_slash != NULL) {
        size_t path_length = last_slash - filepath + 1;
        strncpy(path, filepath, path_length);
        path[path_length] = '\0';  // null-terminate the string
    } else {
        // no '/' found, assuming the entire input is the file name
        strcpy(path, "");
    }
}

void extract_filename(const char *filepath, char *filename) {
    const char *last_slash = strrchr(filepath, '/');
    if (last_slash != NULL) {
        strcpy(filename, last_slash + 1);
    } else {
        // No '/' found, assuming the entire input is the file name
        strcpy(filename, filepath);
    }
}

dirEntry* findEntryFromPath(char* intpath, dirEntry* parentDir) {
    char* token;                            // token for strtok
    char path[MAXPATH];                     // max path length
    char filename[MAXFILENAME];             // name of the file to retrieve
    dirEntry* currentDir = parentDir;       // start from the parent directory
    dirEntry* file;                         // file to find

    // check if the file system is loaded
    fsLoadedCheck();

    // copy the directory path to a local variable
    logMessage("\tfindEntryFromPath: copying path, %s\n", intpath);
    strcpy(path, intpath);

    // check if the path is the root
    if (strcmp(path, "/") == 0) {
        dirEntry* root = (dirEntry*)&blocks[0];
        return root;
    }

    // get the filename
    logMessage("\tfindEntryFromPath: extracting filename from path, %s\n", intpath);
    extract_filename(intpath, filename);
    logMessage("\tfindEntryFromPath: extracted filename, %s\n", filename);

    if (strcmp(filename, "") == 0) {
        fprintf(stderr, "No filename provided\n");
        return NULL;
    }

    // tokenize the path and find the directory to extract the file from
    logMessage("Finding directory entry for file \"%s\" in %s\n", filename, intpath);
    token = strtok(path, "/");
    while (token != NULL) {
        dirEntry* foundEntry = findEntryInDirectory(currentDir, token);
        if (foundEntry == NULL) {
            logMessage("Directory, %s, does not exist\n", token);
            return NULL;
        }
        // check if we've found our file
        if (strcmp(token, filename) == 0) {
            file = foundEntry;
            break;
        }
        // move to the next directory in the path
        currentDir = foundEntry;
        token = strtok(NULL, "/");
    }

    // check if the file exists
    if (file == NULL) {
        logMessage("Directory, %s, does not exist\n", token);
        return NULL;
    }

    return file;
}

void getFullPath(dirEntry* dir, char* path) {
    if (dir->first_cluster_low == 0) {
        // Root directory
        strcpy(path, "/");
    } else {
        char parentPath[MAXPATH] = {0};
        dirEntry* parentDir = findEntryInDirectory(dir, "..");
        if (parentDir != NULL) {
            getFullPath(parentDir, parentPath);
            if (strcmp(parentPath, "/") != 0) {
                strcat(parentPath, "/");
            }
            strcat(parentPath, dir->name);
            strcpy(path, parentPath);
        }
    }
}

int isDirectoryEmpty(dirEntry* entry) {
    unsigned short blockIndex = entry->first_cluster_low;   // index of the block
    block* currentBlock = NULL;                             // pointer to the current block
    dirEntry* currentEntry = NULL;                          // pointer to the current entry
    unsigned short isLast = 0;                              // flag to indicate if the entry is the last in the block

    // check if the file system is loaded
    fsLoadedCheck();

    // get the block pointer
    currentBlock = &blocks[blockIndex];

    // get the '..' entry
    currentEntry = findEntryInDirectory(entry, "..");
    if (currentEntry == NULL) {
        fprintf(stderr, "An error occurred while checking if the directory is empty\n");
    }

    // check if .. is the last entry in the directory
    if(currentEntry->isLast == LASTENTRY) {
        return 1;
    }

    return 0;
}

int getNumSubdirs(dirEntry* dir) {
    int numSubdirs = 0;
    dirEntry* entry = NULL;

    logMessage("Getting number of subdirectories in directory %s\n", dir->name);

    if (dir == NULL) {
        return 0;
    }

    if (dir->attributes != ATTR_DIRECTORY) {
        return 1;
    }

    entry = (dirEntry*)&blocks[dir->first_cluster_low];
    while (entry->isLast != 1) {
        if (entry->attributes & ATTR_DIRECTORY && entry->name[0] != 0x5F && entry->attributes != ATTR_DELETED) {
            numSubdirs++;
        }
        entry = getNextEntry(entry, dir);
    }

    return numSubdirs + 1; // +1 for the current directory
}
