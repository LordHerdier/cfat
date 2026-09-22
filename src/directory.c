#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "directory.h"
#include "direntry.h"
#include "fat.h"
#include "datetime.h"
#include "globals.h"

void createRootDirectory() {
    char name[MAXFILENAME] = "/";       // name of the root directory
    char attributes = ATTR_DIRECTORY;   // root is a directory
    char isLast = LASTENTRY;            // flag to indicate that this is the last entry in the directory list (for now)
    char create_time_tenth = 0;         // create time (tenths of second)
    short create_time = 0;              // create time (seconds)
    short create_date = 0;              // create date
    short last_access_date = 0;         // last access date
    short first_cluster_high = 0;       // high word of the first cluster number
    short last_write_time = 0;          // last write time
    short last_write_date = 0;          // last write date
    short first_cluster_low = 0;        // low word of the first cluster number
    unsigned int size = 0;              // size of the file or directory
    dirEntry* root = NULL;              // pointer to the root directory entry

    // check if the file system is loaded
    fsLoadedCheck();

    // get the pointer to the root directory
    root = (dirEntry*)&blocks[0];

    // get current date time for create and last write
    getDateTime(&create_time, &create_time_tenth, &create_date);

    // set the values of the root directory entry
    setDirEntry(root, name, attributes, create_time_tenth, create_time, create_date, last_access_date,
                first_cluster_high, last_write_time, last_write_date, first_cluster_low, size, isLast);

    // initialize the root directory block
    initializeNewDirectory(root, root);

    // Mark the block in FAT as used
    FAT[0] = USHRT_MAX;

    logMessage("root directory created\n");
}

void _addDirectory(char* directoryName, dirEntry* parentDirEntry) {
    unsigned short currentBlockIndex = USHRT_MAX;     // index on the FAT of the current working block
    unsigned short finalDirIndex = USHRT_MAX;         // directory # of the last entry in the block. not the index
    unsigned short newEntryIndex = USHRT_MAX;         // index on block->data for new entry
    unsigned short parentBlockIndex = USHRT_MAX;      // index on the FAT of the parent directory
    block* currentBlockPtr = NULL;                    // pointer to the current block
    dirEntry* newDirEntry = NULL;                     // pointer to the new directory entry
    dirEntry* previousEntry = NULL;                   // pointer to the previous entry in the block

    // check if the file system is loaded
    fsLoadedCheck();

    logMessage("Attempting to add directory\n");

    // get the parent block index from the parent directory entry
    parentBlockIndex = parentDirEntry->first_cluster_low;

    // traverse the FAT to find the last block of the parent directory
    currentBlockIndex = findLastBlockOfParent(parentBlockIndex);
    currentBlockPtr = &blocks[currentBlockIndex];
    logMessage("Found last block of parent directory\n");

    // find the final dir entry in the last block of the parent directory
    finalDirIndex = findLastEntryInBlock(currentBlockIndex);

    // check if there is space in the parent directory
    if (finalDirIndex == USHRT_MAX) {
        fprintf(stderr, "No space left in parent directory, cannot add directory\n");
        exit(1);
    }

    // check if the name is too long
    if (strlen(directoryName) > MAXFILENAME) {
        fprintf(stderr, "Directory name is too long, cannot add directory\n");
        exit(1);
    }

    // check if the directory name already exists. TODO
    if (findEntryInDirectory(parentDirEntry, directoryName) != NULL) {
        fprintf(stderr, "Directory, %s, already exists, cannot add directory\n", directoryName);
        exit(1);
    }

    // get the pointer to the last entry in the block
    if (finalDirIndex == (BLOCKSIZE / sizeof(dirEntry) - 1)) {
        // No space left in block. Last entry is at the end of the block

        // get the pointer to the current last entry in the block
        previousEntry = (dirEntry*)&blocks[currentBlockIndex].data[finalDirIndex * sizeof(dirEntry)];

        // find a free block, and update the FAT
        currentBlockIndex = allocateNewBlock(currentBlockIndex);

        // get the pointer to the new entry in the block
        newDirEntry = (dirEntry*)&blocks[currentBlockIndex];

        logMessage("Block full. Allocating free block for new directory\n");
    }
    else {
        // Have space left

        // Get the index of the new entry in the block
        newEntryIndex = (finalDirIndex * sizeof(dirEntry)) + sizeof(dirEntry);

        // set the pointer to the next space after the last entry
        newDirEntry = (dirEntry*)&blocks[currentBlockIndex].data[newEntryIndex];

        // set the pointer to the current last entry in the block
        previousEntry = (dirEntry*)&blocks[currentBlockIndex].data[finalDirIndex * sizeof(dirEntry)];

        logMessage("Space left in block. Adding directory to current block\n");
    }

    // update the current last entry in the block to indicate that it is not the last entry
    previousEntry->isLast = NOTLASTENTRY;

    // allocate a new block for the new directory's data
    unsigned short newDirBlock = findFreeBlock();
    FAT[newDirBlock] = USHRT_MAX;

    // get current date time for create and last write
    short create_time = 0;
    char create_time_tenth = 0;
    short create_date = 0;
    getDateTime(&create_time, &create_time_tenth, &create_date);

    // calculate the cluster number of the new directory
    short clusterHigh = (newDirBlock >> 16) & 0xFFFF;
    short clusterLow = newDirBlock & 0xFFFF;

    // set the values of the new directory entry
    setDirEntry(newDirEntry, directoryName, ATTR_DIRECTORY,
                create_time_tenth, create_time, create_date, create_date,
                clusterHigh, create_time, create_date,
                clusterLow, 0, LASTENTRY);

    // set the last entry in the block to indicate that it is the last entry
    newDirEntry->isLast = LASTENTRY;

    // initialize the new directory block
    initializeNewDirectory(newDirEntry, parentDirEntry);

    logMessage("New directory added\n");
}

void addDirectory(char* directoryPath, dirEntry* parentDirEntry) {
    char* token;                            // token for strtok
    char path[MAXPATH];                     // maximum path size
    dirEntry* currentDir = parentDirEntry;  // start from the root directory

    // copy the directory path to a local variable
    strcpy(path, directoryPath);

    // tokenize the path and create directories as needed
    token = strtok(path, "/");
    while (token != NULL) {
        dirEntry* foundEntry = findEntryInDirectory(currentDir, token);
        if (foundEntry == NULL) {
            // directory does not exist, create it
            _addDirectory(token, currentDir);
            foundEntry = findEntryInDirectory(currentDir, token);
        }
        // move to the next directory in the path
        currentDir = foundEntry;
        token = strtok(NULL, "/");
    }
}

void listDirectory(dirEntry* parentDir) {
    unsigned short currentDirBlockIndex = USHRT_MAX;     // index on the FAT of the current working block
    block* currentDirBlock = NULL;                       // pointer to the current directory block
    dirEntry* currentDirEntry = NULL;                    // pointer to the current directory entry
    unsigned short currentDirEntryIndex = USHRT_MAX;     // index of the current directory entry in the block
    char dateTimeStr[20];                                // string to hold the formatted date and time

    // check if the file system is loaded
    fsLoadedCheck();

    // print the header
    printf("%-12s %-20s %-10s\n", "Name", "Date Modified", "Size");
    printf("%-12s %-20s %-10s\n", "------------", "-------------------", "----------");

    // set the current block index to the first cluster of the parent directory
    currentDirBlockIndex = parentDir->first_cluster_low;

    // get the pointer to the current directory block
    currentDirBlock = &blocks[currentDirBlockIndex];

    // set the pointer to the first entry in the block
    currentDirEntryIndex = 0;
    currentDirEntry = (dirEntry*)&currentDirBlock->data[currentDirEntryIndex];

    // iterate through the directory entries in the block until the last entry is found
    while (currentDirEntry->isLast != LASTENTRY && currentDirEntry != NULL) {
        // don't print the . and .. entries
        if (strcmp(currentDirEntry->name, "..") == 0 || strcmp(currentDirEntry->name, ".") == 0) {
            currentDirEntry = getNextEntry(currentDirEntry, parentDir);
            continue;
        }

        // don't print the deleted entries
        if (currentDirEntry->name[0] == 0x5F || currentDirEntry->attributes == ATTR_DELETED) {
            currentDirEntry = getNextEntry(currentDirEntry, parentDir);
            continue;
        }

        // convert the date and time to a readable format
        convertDateTime(currentDirEntry->last_write_time, currentDirEntry->last_write_date, dateTimeStr);

        // print the entry name
        char name[12] = {0};
        strncpy(name, currentDirEntry->name, 11);
        if (currentDirEntry->attributes == ATTR_DIRECTORY) {
            printf("%-12s %-20s %-10s\n", strcat(name, "/"), dateTimeStr, "0");
        } else {
            printf("%-12s %-20s %-10u\n", currentDirEntry->name, dateTimeStr, currentDirEntry->size);
        }

        // get the next entry in the directory
        currentDirEntry = getNextEntry(currentDirEntry, parentDir);
    }

    // make sure we still have a currentDirEntry
    if (currentDirEntry == NULL) {
        return;
    }

    // don't print the . and .. entries
    if (strcmp(currentDirEntry->name, "..") == 0 || strcmp(currentDirEntry->name, ".") == 0) {
        return; // we can return here because we know that we are at the last entry
    }

    // don't print the deleted entries
    if (currentDirEntry->name[0] == 0x5F || currentDirEntry->attributes == ATTR_DELETED) {
        return; // we can return here because we know that we are at the last entry
    }

    // convert the last write date and time to a human-readable format
    convertDateTime(currentDirEntry->last_write_time, currentDirEntry->last_write_date, dateTimeStr);

    // print the last entry in the block
    char name[12] = {0};
    strncpy(name, currentDirEntry->name, 11);
    if (currentDirEntry->attributes == ATTR_DIRECTORY) {
        printf("%-12s %-20s %-10s\n", strcat(name, "/"), dateTimeStr, "0");
    } else {
        printf("%-12s %-20s %-10u\n", currentDirEntry->name, dateTimeStr, currentDirEntry->size);
    }
}


void _printDirectoryTree(dirEntry* parentDir, int depth) {
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
        logMessage("Current entry: %s\n", currentDirEntry->name);
        // don't print the . and .. entries and get the next entry
        if (strcmp(currentDirEntry->name, "..") == 0 || strcmp(currentDirEntry->name, ".") == 0) {
            currentDirEntry = getNextEntry(currentDirEntry, parentDir);
            logMessage("Got next entry, %s\n", currentDirEntry->name);
            continue;
        }

        // don't print the deleted entries
        if (currentDirEntry->name[0] == 0x5F || currentDirEntry->attributes == ATTR_DELETED) {
            currentDirEntry = getNextEntry(currentDirEntry, parentDir);
            logMessage("Got next entry, %s\n", currentDirEntry->name);
            continue;
        }

        if (currentDirEntry->attributes == ATTR_DIRECTORY)
        {
            printf("%*s", depth * 2, "");
            printf("%s/\n", currentDirEntry->name);
        }
        else
        {
            printf("%*s", depth * 2, "");
            printf("%s\n", currentDirEntry->name);
        }

        if (currentDirEntry->attributes == ATTR_DIRECTORY) {
            // logMessage("Found subdirectory\n");
            logMessage("Recursing into %s\n", currentDirEntry->name);
            dirEntry* subDirEntry = (dirEntry*)&blocks[currentDirEntry->first_cluster_low];

            // recursively list the contents of the subdirectory
            _printDirectoryTree(subDirEntry, depth + 1);
        }

        // get the next entry in the directory
        currentDirEntry = getNextEntry(currentDirEntry, parentDir);
    }

    // make sure we still have a currentDirEntry
    if (currentDirEntry == NULL) {
        return;
    }

    // don't print the . and .. entries
    if (strcmp(currentDirEntry->name, "..") == 0 || strcmp(currentDirEntry->name, ".") == 0) {
        return; // we can return here because we know that we are at the last entry
    }

    // don't print the deleted entries
    if (currentDirEntry->name[0] == 0x5F || currentDirEntry->attributes == ATTR_DELETED) {
        return; // we can return here because we know that we are at the last entry
    }

    // print the last entry in the block
    if (currentDirEntry->attributes == ATTR_DIRECTORY)
    {
        printf("%*s", depth * 2, "");
        printf("%s/\n", currentDirEntry->name);
    }
    else
    {
        printf("%*s", depth * 2, "");
        printf("%s\n", currentDirEntry->name);
    }

    // check if the last entry is a directory
    if (currentDirEntry->attributes == ATTR_DIRECTORY) {
        logMessage("Found subdirectory in last entry. Recursing\n");
        dirEntry* subDirEntry = (dirEntry*)&blocks[currentDirEntry->first_cluster_low];

        // recursively list the contents of the subdirectory
        _printDirectoryTree(subDirEntry, depth + 1);
    }

    logMessage("Directory listed. Exiting stack frame\n\n");
}

void printDirectoryTree(dirEntry* parentDir) {
    _printDirectoryTree(parentDir, 0);
}

void removeDirectoryEntry(char* intpath, dirEntry* rootDir) {
    char* parentPath = malloc(MAXPATH);                           // path to the parent directory
    dirEntry* entry = findEntryFromPath(intpath, rootDir);        // find the directory entry to remove
    dirEntry* previousEntry = NULL;                               // pointer to the previous entry in the directory
    dirEntry* parentDir = NULL;                                   // pointer to the parent directory
    unsigned short blockIndex = USHRT_MAX;                        // index of the parent directory
    unsigned short entryIndex = 0;                                // index of the current entry in the block
    block* currentBlock = NULL;                                   // pointer to the current block
    unsigned short isLast = 0;                                    // flag to indicate if the entry is the last in the block

    // check if the file system is loaded
    fsLoadedCheck();

    // get the parent directory of the entry and the block index
    extract_path(intpath, parentPath);  // get the parent directory path (without the filename)
    parentDir = findParentFromPath(parentPath, rootDir);

    // check if the parent directory exists
    if (parentDir == NULL) {
        fprintf(stderr, "Parent directory of \"%s\" does not exist, cannot remove\n", intpath);
        return;
    }

    blockIndex = parentDir->first_cluster_low;

    // check if the entry is valid
    if (entry == NULL) {
        fprintf(stderr, "File or directory \"%s\" does not exist, cannot remove\n", intpath);
        return;
    }

    // check if the entry is a directory, check if it is empty
    if (entry->attributes == ATTR_DIRECTORY) {
        if (!isDirectoryEmpty(entry)) {
            fprintf(stderr, "Directory \"%s\" is not empty, cannot remove\n", intpath);
            return;
        }
    }

    // find the entry in the directory and mark it as deleted
    while (blockIndex != USHRT_MAX) {
        currentBlock = &blocks[blockIndex];
        for (entryIndex = 0; entryIndex < BLOCKSIZE; entryIndex += sizeof(dirEntry)) {
            dirEntry* currentEntry = (dirEntry*)&currentBlock->data[entryIndex];
            if (currentEntry->attributes != ATTR_DELETED && strcmp(currentEntry->name, entry->name) == 0) {
                currentEntry->attributes = ATTR_DELETED;  // mark the entry as deleted

                // change the first character of the name to '_'
                currentEntry->name[0] = '_';

                logMessage("Entry \"%s\" marked as deleted\n", intpath);

                // if the entry is the last one, update the previous entry's isLast flag
                if (currentEntry->isLast == LASTENTRY) {
                    if (previousEntry != NULL) {
                        previousEntry->isLast = LASTENTRY;
                    }
                }

                // free the blocks used by the file or directory
                unsigned short blockToFree = currentEntry->first_cluster_low;
                while (blockToFree != USHRT_MAX) {
                    unsigned short nextBlock = FAT[blockToFree];
                    FAT[blockToFree] = 0;
                    blockToFree = nextBlock;
                }
                logMessage("Blocks used by entry \"%s\" freed\n", intpath);

                logMessage("Entry \"%s\" removed successfully\n", intpath);
                free(parentPath);
                return;
            }
            if (currentEntry->isLast == LASTENTRY) {
                isLast = 1;
                break;
            }
            // Update previousEntry only if currentEntry is not deleted
            if (currentEntry->attributes != ATTR_DELETED) {
                previousEntry = currentEntry;
            }
        }
        if (isLast) break;
        blockIndex = FAT[blockIndex];
    }

    free(parentPath);
    fprintf(stderr, "Failed to remove entry \"%s\"\n", intpath);
}
