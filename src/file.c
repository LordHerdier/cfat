#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include "file.h"
#include "direntry.h"
#include "fat.h"
#include "datetime.h"
#include "globals.h"

void createEmptyFile(char* filename, dirEntry* parent) {
    unsigned short currentBlockIndex = USHRT_MAX;     // index on the FAT of the current working block
    unsigned short fileBlockIndex = USHRT_MAX;        // index on the FAT of the file block
    unsigned short finalDirIndex = USHRT_MAX;         // index of the last entry in the parent directory
    unsigned short newEntryIndex = USHRT_MAX;         // index of the new entry in the parent directory
    block* currentBlock = NULL;                       // pointer to the current working block
    dirEntry* newEntry = NULL;                        // pointer to the new entry
    dirEntry* previousEntry = NULL;                   // pointer to the previous entry in the parent directory

    // check if the file system is loaded
    fsLoadedCheck();

    // check if the file already exists
    if (findEntryInDirectory(parent, filename) != NULL) {
        fprintf(stderr, "File already exists\n");
        return;
    }

    // check if the filename is too long
    if (strlen(filename) > MAXFILENAME) {
        fprintf(stderr, "Filename is too long\n");
        return;
    }

    // reserve a block for the new file
    fileBlockIndex = findFreeBlock();
    FAT[fileBlockIndex] = USHRT_MAX;

    // find the final dir entry in the last block of the parent directory
    currentBlockIndex = findLastBlockOfParent(parent->first_cluster_low);
    finalDirIndex = findLastEntryInBlock(currentBlockIndex);

    // get the pointer to the last entry in the parent directory
    currentBlock = &blocks[currentBlockIndex];
    if (finalDirIndex == (BLOCKSIZE / sizeof(dirEntry) - 1)) {
        // no space left in block. last entry is at the end of the block

        // get the pointer to the current last entry in the block
        previousEntry = (dirEntry*)&blocks[currentBlockIndex].data[finalDirIndex * sizeof(dirEntry)];

        // allocate a new block for the parent directory
        currentBlockIndex = allocateNewBlock(currentBlockIndex);

        // get the pointer to the new entry in the new block
        newEntry = (dirEntry*)&blocks[currentBlockIndex];
        logMessage("Allocated new block for parent directory at %d\n", currentBlockIndex);
    }
    else {
        // have space left in block

        // get the index of the new entry in the block
        newEntryIndex = (finalDirIndex * sizeof(dirEntry)) + sizeof(dirEntry);

        // set the pointer to the next space after the last entry
        newEntry = (dirEntry*)&blocks[currentBlockIndex].data[newEntryIndex];

        // set the pointer to the current last entry in the block
        previousEntry = (dirEntry*)&blocks[currentBlockIndex].data[finalDirIndex * sizeof(dirEntry)];

        logMessage("Space left in block. Adding file to current block\n");
    }

    // update the current last entry in the block to indicate that it is not the last entry
    previousEntry->isLast = NOTLASTENTRY;

    // get the date and time
    short create_time = 0;
    char create_time_tenth = 0;
    short create_date = 0;
    getDateTime(&create_time, &create_time_tenth, &create_date);

    // calculate the cluster number of the new file
    short clusterHigh = (fileBlockIndex >> 16) & 0xFFFF;
    short clusterLow = fileBlockIndex & 0xFFFF;

     // initialize the new file entry
    setDirEntry(newEntry, filename, ATTR_ARCHIVE,
                create_time_tenth, create_time, create_date,
                create_date, clusterHigh, create_time,
                create_date, clusterLow, 0, LASTENTRY);

    logMessage("Added file entry for \"%s\" in directory \"%s\" at block %d\n", filename, parent->name, currentBlockIndex);
}

void _addFile(char* sourceFilename, char* intpath, dirEntry* parentDir) {
    unsigned int fileSize = 0;                        // size of the file
    unsigned short currentBlockIndex = USHRT_MAX;     // index on the FAT of the current working block
    unsigned short fileBlockIndex = USHRT_MAX;        // index on the FAT of the file block
    unsigned short tempBlock = USHRT_MAX;             // temporary index on the FAT
    unsigned short finalDirIndex = USHRT_MAX;         // index of the last entry in the parent directory
    unsigned short newEntryIndex = USHRT_MAX;         // index of the new entry in the parent directory
    unsigned short numBlocksToAllocate = 0;           // number of blocks to allocate for the file
    char* filename = malloc(100);                     // name of the file
    block* currentBlock = NULL;                       // pointer to the current working block
    dirEntry* currentDir = parentDir;                 // start from the parent directory
    dirEntry* newFileEntry = NULL;                    // pointer to the new file entry
    dirEntry* lastEntry = NULL;                       // pointer to the last entry in the parent directory
    FILE* fileContents = NULL;                        // pointer to the file contents

    // check if the file system is loaded
    fsLoadedCheck();

    // get the filename from the source path
    strcpy(filename, basename(sourceFilename));

    // check if the name is too long
    if (strlen(filename) > MAXFILENAME) {
        fprintf(stderr, "File name is too long, cannot add file\n");
        exit(1);
    }

    // check if the file already exists
    if (findEntryInDirectory(parentDir, filename) != NULL) {
        fprintf(stderr, "File, %s, already exists, cannot add file\n", filename);
        exit(1);
    }

    // load reference to the file
    fileContents = fopen(sourceFilename, "r");

    // check if the file was opened
    if (fileContents == NULL) {
        fprintf(stderr, "Error opening file, cannot add file\n");
        exit(1);
    }

    // get the file's size
    fseek(fileContents, 0, SEEK_END);
    fileSize = ftell(fileContents);
    fseek(fileContents, 0, SEEK_SET);

    logMessage("Opened \"%s\" with size %d\n", filename, fileSize);

    // reserve space in the FAT for the file
    if (fileSize % BLOCKSIZE == 0) {
        numBlocksToAllocate = fileSize / BLOCKSIZE;
    }
    else {
        numBlocksToAllocate = fileSize / BLOCKSIZE + 1;
    }

    // allocate the blocks for the file
    fileBlockIndex = findFreeBlock();
    FAT[fileBlockIndex] = USHRT_MAX;

    tempBlock = fileBlockIndex;
    for (int i = 0; i < numBlocksToAllocate - 1; i++) {
        logMessage("\t%d", tempBlock);
        unsigned short newBlock = findFreeBlock();
        FAT[tempBlock] = newBlock;
        FAT[newBlock] = USHRT_MAX;
        tempBlock = newBlock;

        // every 5 blocks, print a newline
        if ((i + 1) % 5 == 0) {
            logMessage("\n");
        }
    }
    logMessage("\t%d\n", tempBlock);

    dirEntry* previousEntry = NULL;
    currentBlockIndex = findLastBlockOfParent(parentDir->first_cluster_low);

    // find the final dir entry in the last block of the parent directory
    finalDirIndex = findLastEntryInBlock(currentBlockIndex);

    // check if there is space in the parent directory
    if (finalDirIndex == USHRT_MAX) {
        fprintf(stderr, "No space left in parent directory, cannot add directory\n");
        exit(1);
    }

    // get the pointer to the last entry in the block
    if (finalDirIndex == (BLOCKSIZE / sizeof(dirEntry) - 1)) {
        // no space left in block. last entry is at the end of the block

        // get the pointer to the current last entry in the block
        previousEntry = (dirEntry*)&blocks[currentBlockIndex].data[finalDirIndex * sizeof(dirEntry)];

        // allocate a new block for the parent directory
        currentBlockIndex = allocateNewBlock(currentBlockIndex);

        // get the pointer to the new entry in the new block
        newFileEntry = (dirEntry*)&blocks[currentBlockIndex];

        logMessage("Allocated new block for parent directory at %d\n", currentBlockIndex);
    }
    else {
        // have space left in block

        // get the index of the new entry in the block
        newEntryIndex = (finalDirIndex * sizeof(dirEntry)) + sizeof(dirEntry);

        // set the pointer to the next space after the last entry
        newFileEntry = (dirEntry*)&blocks[currentBlockIndex].data[newEntryIndex];

        // set the pointer to the current last entry in the block
        previousEntry = (dirEntry*)&blocks[currentBlockIndex].data[finalDirIndex * sizeof(dirEntry)];

        logMessage("Space left in block. Adding directory to current block\n");
    }

    // update the current last entry in the block to indicate that it is not the last entry
    previousEntry->isLast = NOTLASTENTRY;

    // get the time and date of creation
    short create_time = 0;
    char create_time_tenth = 0;
    short create_date = 0;
    getDateTime(&create_time, &create_time_tenth, &create_date);

    // calculate the cluster number of the new file
    short clusterHigh = (fileBlockIndex >> 16) & 0xFFFF;
    short clusterLow = fileBlockIndex & 0xFFFF;

    // initialize the new file entry
    setDirEntry(newFileEntry, filename, ATTR_ARCHIVE,
                create_time_tenth, create_time, create_date,
                create_date, clusterHigh, create_time,
                create_date, clusterLow, fileSize, LASTENTRY);

    logMessage("Added file entry for \"%s\" in directory \"%s\" at block %d\n", filename, parentDir->name, currentBlockIndex);

    // write the file contents to the file block
    fileBlockIndex = newFileEntry->first_cluster_low;

    // read the file contents
    char* buffer = malloc(fileSize);
    int bytesLeft = fileSize;

    // Read the file contents and write them to the blocks
    fread(buffer, 1, fileSize, fileContents);

    logMessage("Writing to %d blocks:\n", numBlocksToAllocate);

    int offset = 0;
    while (bytesLeft > 0) {
        int bytesToWrite = (bytesLeft > BLOCKSIZE) ? BLOCKSIZE : bytesLeft;
        logMessage("\tBytes to write: %d\n", bytesToWrite);
        memcpy(blocks[fileBlockIndex].data, buffer + offset, bytesToWrite);
        logMessage("\tCopied %d bytes to block %d\n", bytesToWrite, fileBlockIndex);
        bytesLeft -= bytesToWrite;
        logMessage("\tBytes left: %d\n", bytesLeft);
        offset += bytesToWrite;
        logMessage("\tOffset: %d\n", offset);

        // logMessage("\tWrote %d bytes to block %d\n", bytesToWrite, fileBlockIndex);

        // Move to the next block if necessary
        if (bytesLeft > 0) {
            fileBlockIndex = FAT[fileBlockIndex];
            logMessage("\tNext block: %d\n\n", fileBlockIndex);
        }
    }

    // free the buffer
    free(buffer);

    // close the file
    fclose(fileContents);

    // log that the file was added
    logMessage("File \"%s\" added successfully\n", sourceFilename);
}

void addFile(char* filename, char* intpath, dirEntry* parentDir) {
    char* token;                            // token for strtok
    char path[MAXPATH];                     // max path length
    dirEntry* currentDir = parentDir;       // start from the parent directory

    // check if the file system is loaded
    fsLoadedCheck();

    // copy the directory path to a local variable
    strcpy(path, intpath);

    // copy the filename to a local variable
    char* file = malloc(strlen(filename) + 1);

    // tokenize the path and find the directory to add the file to
    token = strtok(path, "/");
    while (token != NULL) {
        dirEntry* foundEntry = findEntryInDirectory(currentDir, token);
        if (foundEntry == NULL) {
            fprintf(stderr, "Directory, %s, does not exist, cannot add file\n", token);
            return;
        }
        // move to the next directory in the path
        currentDir = foundEntry;
        token = strtok(NULL, "/");
    }

    // check if the file name is too long
    if (strlen(basename(filename)) > MAXFILENAME) {
        fprintf(stderr, "File name is too long, cannot add file\n");
        return;
    }

    // check if the file already exists
    if (findEntryInDirectory(currentDir, filename) != NULL) {
        fprintf(stderr, "File, %s, already exists, cannot add file\n", filename);
        exit(1);
    }

    // add the file to the directory
    logMessage("Adding file \"%s\" to %s\n", filename, intpath);
    _addFile(filename, intpath, currentDir);

    // free the filename
    free(file);
}

void writeBlockToFile(FILE* f, unsigned short block, unsigned short numBytes) {
    unsigned char* buffer = malloc(BLOCKSIZE);    // buffer to read the block into
    struct block* b = NULL;                       // block to read

    // check if the filesystem is loaded
    fsLoadedCheck();

    // set the block pointer to the block to read
    b = &blocks[block];

    // copy the entire block to the buffer
    memcpy(buffer, b->data, BLOCKSIZE);

    // write the block to the file
    fwrite(buffer, 1, numBytes, f);

    // free the buffer
    free(buffer);
}

void _extractFile(dirEntry* file) {
    unsigned short block = file->first_cluster_low;      // first block of the file
    unsigned int size = file->size;                      // size of the file
    unsigned int bytesToWrite = size;                    // number of bytes to write
    long int offset = 0;                                 // offset in the file
    FILE* f = NULL;                                      // file to write to

    // check if the filesytem is loaded
    fsLoadedCheck();

    // check if the file is a directory
    if (file->attributes == ATTR_DIRECTORY) {
        fprintf(stderr, "Cannot extract directory, %s\n", file->name);
        exit(1);
    }

    // check if the file exists externally
    if (access(file->name, F_OK) != -1) {
        fprintf(stderr, "File \"%s\" already exists externally\n", file->name);
        exit(1);
    }

    // open the file for writing
    if((f = fopen(file->name, "wb")) == NULL) {
        fprintf(stderr, "Error opening file \"%s\" for writing\n", file->name);
        exit(1);
    }
    logMessage("Opened file \"%s\" for writing\n", file->name);

    // write the file to the external file
    logMessage("Starting write of file \"%s\"...\n", file->name);

    // loop through the blocks and write them to the file
    while (bytesToWrite > 0) {
        unsigned int numBytes = (bytesToWrite > BLOCKSIZE) ? BLOCKSIZE : bytesToWrite;
        fseek(f, offset, SEEK_SET);
        writeBlockToFile(f, block, numBytes);
        logMessage("\tWrote %d bytes to offset %ld\n", numBytes, offset);
        offset += numBytes;
        bytesToWrite -= numBytes;
        block = FAT[block];
    }

    logMessage("Finished writing file \"%s\"\n", file->name);

    // close the file
    fclose(f);

}

void extractFile(char* intpath, dirEntry* parentDir) {
    dirEntry* file = NULL;                     // file to extract

    // check if the file system is loaded
    fsLoadedCheck();

    // find the file to extract
    file = findEntryFromPath(intpath, parentDir);

    // check if the file exists
    if (file == NULL) {
        fprintf(stderr, "File, %s, does not exist\n", intpath);
        return;
    }

    // extract the file
    logMessage("Extracting file \"%s\"\n", intpath);
    _extractFile(file);

    printf("Extracted file \"%s\"\n", intpath);
    return;
}

void catFile(char* intpath, dirEntry* parentDir) {
    dirEntry* file = NULL;                     // file to read
    unsigned short block = 0;                  // first block of the file
    unsigned int size = 0;                     // size of the file
    unsigned int bytesRead = 0;                // number of bytes read
    unsigned int bytesToRead = 0;              // number of bytes to read
    char buffer[BLOCKSIZE];                    // buffer to read the file data

    // check if the file system is loaded
    fsLoadedCheck();

    // find the file to read
    file = findEntryFromPath(intpath, parentDir);

    // check if the file exists
    if (file == NULL) {
        fprintf(stderr, "File \"%s\" does not exist\n", intpath);
        exit(1);
    }

    // check if the file is a directory
    if (file->attributes == ATTR_DIRECTORY) {
        fprintf(stderr, "Cannot read directory, %s\n", file->name);
        exit(1);
    }

    // get the file's size and first block
    size = file->size;
    block = file->first_cluster_low;

    // read and print the file contents block by block
    while (size > 0) {
        bytesToRead = (size > BLOCKSIZE) ? BLOCKSIZE : size;
        memcpy(buffer, blocks[block].data, bytesToRead);
        fwrite(buffer, 1, bytesToRead, stdout);
        size -= bytesToRead;
        block = FAT[block];
    }

    // print a newline at the end
    printf("\n");
}

void touchFile(char* intpath, dirEntry* parentDir) {
    dirEntry* file = NULL;                     // file to update/create
    short seconds;
    char tenths;
    short date;

    // check if the file system is loaded
    fsLoadedCheck();

    // find the file to update the timestamp of
    file = findEntryFromPath(intpath, parentDir);

    // check if the file exists
    if (file == NULL) {
        createEmptyFile(basename(intpath), parentDir);
        return;
    }

    // update the file's timestamp
    getDateTime(&seconds, &tenths, &date);

    // set the file's last modified date and time
    file->last_write_time = seconds;
    file->last_write_date = date;
    file->last_access_date = date;

    logMessage("File \"%s\" timestamp updated\n", intpath);
}
