#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <limits.h>
#include <time.h>
#include <libgen.h>
#include <errno.h>
#include <fuse.h>
#include <sys/statvfs.h>

#define FSSIZE 10000000
#define BLOCKSIZE 512
#define MAXBLOCKS 19000

#define MAXFILENAME 11
#define MAXPATH 255
#define NOTLASTENTRY 0x00
#define LASTENTRY 0x01

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_DELETED 0xE5


typedef struct dirEntry {
    char name[MAXFILENAME];      // name of the file or directory
    char attributes;             // attributes of the file or directory
    // char _reserved;           // reserved. FAT specs it, but i'm using it for an end bit
    char isLast;                 // flag to indicate if this is the last entry in the directory
    char create_time_tenth;      // create time (tenths of second)
    short create_time;           // create time
    short create_date;           // create date
    short last_access_date;      // last access date
    short first_cluster_high;    // high word of the first cluster number
    short last_write_time;       // last write time
    short last_write_date;       // last write date
    short first_cluster_low;     // low word of the first cluster number
    unsigned int size;           // size of the file or directory
} dirEntry;

typedef struct block{
    char data[BLOCKSIZE];   //data of the block
}block;

// prototypes
unsigned short allocateNewBlock(unsigned short currentBlockIndex);
unsigned short findFreeBlock();
unsigned short findLastEntryInBlock(unsigned short blockindex);
unsigned short findLastBlockOfParent(short parentdirIndex);
int getNumSubdirs(dirEntry* dir);
int isDirectoryEmpty(dirEntry* entry);
int mountfs(char* mountpath, char* fsname);
dirEntry* findEntryFromPath(char* intpath, dirEntry* parentDir);
dirEntry* findEntryInDirectory(dirEntry* parentDir, char* entryName);
dirEntry* findParentFromPath(char* path, dirEntry* parentDir);
dirEntry* getNextEntry(dirEntry* currentEntry, dirEntry* parentDirEntry);
time_t convertFATDateTime(short date, short time);
void _addDirectory(char* directoryName, dirEntry* parentDirEntry);
void addDirectory(char* directoryPath, dirEntry* parentDirEntry);
void _addFile(char* filename, char* intpath, dirEntry* parentDir);
void addFile(char* filename, char* intpath, dirEntry* parentDir);
void catFile(char* intpath, dirEntry* parentDir);
void convertDateTime(short time, short date, char* dateTimeStr);
void createEmptyFile(char* filename, dirEntry* parent);
void createfs(char* fsname);
void createRootDirectory();
void _extractFile(dirEntry* file);
void extractFile(char *intpath, dirEntry *parentDir);
void extract_filename(const char *filepath, char *filename);
void extract_path(const char *filepath, char *path);
void fsLoadedCheck();
void formatfs();
void getDateTime(short* seconds, char* tenths, short* date);
void listDirectory(dirEntry* parentDir);
void loadfs(char* fsname);
void logMessage(const char* format, ...);
void mapfs(FILE* filetomap);
void initializeNewDirectory(dirEntry* newDir, dirEntry* parentDir);
void _printDirectoryTree(dirEntry* parentDir, int depth);
void printDirectoryTree(dirEntry* parentDir);
void printUsage(char* progname);
void removeDirectoryEntry(char* intpath, dirEntry* rootDir);
void setDirEntry(dirEntry* entry, char* name, char attributes,char create_time_tenth, short create_time, short create_date,
                 short last_access_date, short first_cluster_high, short last_write_time, short last_write_date,
                  short first_cluster_low, unsigned int size, char isLast);
void writeBlockToFile(FILE* f, unsigned short block, unsigned short numBytes);


// global variables
char* fs = NULL;            //pointer to the memory mapped file system
unsigned short* FAT = NULL; //pointer to the File Allocation Table
block* blocks = NULL;       //pointer to the blocks of the file system
int verbose = 0;            //verbose flag


// functions

void logMessage(const char* format, ...) {
    if (verbose) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

void mapfs(FILE* filetomap) {
    // map the file system to the memory
    fs = mmap(NULL, FSSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(filetomap), 0);

    // check if mmap failed
    if (fs == NULL) {
        fprintf(stderr, "mmap failed, exiting\n");
        exit(1);
    }

    // set the pointers to the correct locations
    FAT = (unsigned short*)fs;
    blocks = (block*)(fs + MAXBLOCKS*sizeof(short));

    logMessage("file system mapped to memory\n");
}

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

void formatfs() {
    // check if the file system is mapped
    if (fs == NULL) {
        fprintf(stderr, "no fs mapped, format failed\n");
        exit(1);
    }

    // clear the file system
    bzero(fs, FSSIZE);

    // make block 0 the first and last block of root directory (for now). Using USHRT_MAX to indicate the end of the list
    FAT[0] = USHRT_MAX;
    // printf("first free block is at %hu\n", findFreeBlock());

    logMessage("file system formatted\n");
}

void fsLoadedCheck() {
    // check if the file system is loaded
    if (fs == NULL) {
        fprintf(stderr, "No file system loaded, exiting\n");
        exit(1);
    }
}

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

void getDateTime(short* seconds, char* tenths, short* date) {
    // Get the current time
    time_t now = time(NULL);
    struct tm* currentTime = localtime(&now);

    // Seconds: count of 2-second increments
    *seconds = (currentTime->tm_sec / 2) & 0x1F; // 5 bits for seconds (0-29)

    // Tenths of a second
    *tenths = 0; // Assuming we do not track tenths of a second

    // Minutes
    short minutes = (currentTime->tm_min & 0x3F) << 5; // 6 bits for minutes (0-59)

    // Hours
    short hours = (currentTime->tm_hour & 0x1F) << 11; // 5 bits for hours (0-23)

    // Combine hours and minutes into the seconds field
    *seconds |= minutes | hours;

    // Day of the month
    short day = (currentTime->tm_mday & 0x1F); // 5 bits for day (1-31)

    // Month of the year
    short month = ((currentTime->tm_mon + 1) & 0x0F) << 5; // 4 bits for month (1-12)

    // Year from 1980
    short year = ((currentTime->tm_year - 80) & 0x7F) << 9; // 7 bits for year (0-127)

    // Combine day, month, and year into the date field
    *date = day | month | year;
}

time_t convertFATDateTime(short date, short time) {
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));

    tm.tm_sec = (time & 0x1F) * 2;
    tm.tm_min = (time >> 5) & 0x3F;
    tm.tm_hour = (time >> 11) & 0x1F;
    tm.tm_mday = date & 0x1F;
    tm.tm_mon = ((date >> 5) & 0x0F) - 1;
    tm.tm_year = ((date >> 9) & 0x7F) + 80;

    return mktime(&tm);
}

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

void createfs(char* fsname) {
    FILE* fsfile = NULL;        // file system file pointer

    // check if file name already exists
    if (fopen(fsname, "r") != NULL) {
        fprintf(stderr, "File system already exists, exiting\n");
        exit(1);
    }

    // create the file system file
    printf("Creating file system %s\n", fsname);
    fsfile = fopen(fsname, "w+");

    // check if the file system file was created
    if (fsfile == NULL) {
        fprintf(stderr, "Error creating file system, exiting\n");
        exit(1);
    }

    // set the size and fill it with zeros
    fseek(fsfile, FSSIZE-1, SEEK_SET);
    fwrite("\0", 1, 1, fsfile);
    fseek(fsfile, 0, SEEK_SET);

    // map and format the file system
    mapfs(fsfile);
    formatfs();

    // create the root directory
    createRootDirectory();

    logMessage("file system created\n");
}

void loadfs(char* fsname) {
    FILE* fsfile = NULL;        // file system file pointer

    // open the file system file
    fsfile = fopen(fsname, "r+");

    // check if the file system file was opened
    if (fsfile == NULL) {
        fprintf(stderr, "error opening file system, exiting\n");
        exit(1);
    }

    // map the file system
    mapfs(fsfile);

    logMessage("file system loaded\n");
}

void printUsage(char* progname) {
    fprintf(stderr, "Usage: %s -f <somename.CFAT> [options]\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -f <filesystem>    Specify the file system name\n");
    fprintf(stderr, "  -c                 Create a new file system\n");
    fprintf(stderr, "  -l                 List the contents of the file system\n");
    fprintf(stderr, "  -v                 Enable verbose mode\n");
    fprintf(stderr, "  -a <file>          Add a file to the file system\n");
    fprintf(stderr, "  -i <internal path> Specify the internal path in the file system for adding a file\n");
    fprintf(stderr, "  -r <internal path> Remove a file or directory from the file system\n");
    fprintf(stderr, "  -d <directory>     Add a directory to the file system\n");
    fprintf(stderr, "  -e <internal path> Extract a file from the file system\n");
    fprintf(stderr, "  -h                 Display this help message\n");
    fprintf(stderr, "  -m <mountpoint>    Mount the file system to a directory\n");
    fprintf(stderr, "  -I                 Launch interactive mode\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  Create a new file system:\n");
    fprintf(stderr, "    %s -f myfilesystem.CFAT -c\n", progname);
    fprintf(stderr, "  List the contents of the file system:\n");
    fprintf(stderr, "    %s -f myfilesystem.CFAT -l\n", progname);
    fprintf(stderr, "  Add a file to the file system:\n");
    fprintf(stderr, "    %s -f myfilesystem.CFAT -a myfile.txt -i /myfolder/\n", progname);
    fprintf(stderr, "  Remove a file or directory from the file system:\n");
    fprintf(stderr, "    %s -f myfilesystem.CFAT -r /myfolder/myfile.txt\n", progname);
    fprintf(stderr, "  Add a directory to the file system:\n");
    fprintf(stderr, "    %s -f myfilesystem.CFAT -d /myfolder\n", progname);
    fprintf(stderr, "  Extract a file from the file system:\n");
    fprintf(stderr, "    %s -f myfilesystem.CFAT -e /myfolder/myfile.txt\n", progname);
    fprintf(stderr, "  Mount the file system to a directory:\n");
    fprintf(stderr, "    %s -f myfilesystem.CFAT -m /mnt/myfilesystem\n", progname);
    fprintf(stderr, "\n");
    fprintf(stderr, "Note: The internal path should start with a forward slash (/)\n");
    fprintf(stderr, "Note: Maximum file/directory name length is 11 characters, including extension\n");
    fprintf(stderr, "Note: Passing no flags will result in the interactive console launching\n");
    exit(1);
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

unsigned short allocateNewBlock(unsigned short currentBlockIndex) {
    unsigned short freeBlock = findFreeBlock();
    FAT[currentBlockIndex] = freeBlock;
    FAT[freeBlock] = USHRT_MAX;
    return freeBlock;
}

unsigned short findLastBlockOfParent(short parentdirIndex) {
    unsigned short currentBlockIndex = parentdirIndex;
    while (FAT[currentBlockIndex] != USHRT_MAX) {
        currentBlockIndex = FAT[currentBlockIndex];
    }
    return currentBlockIndex;
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

// helper function to convert the date and time to a human-readable format
void convertDateTime(short time, short date, char* dateTimeStr) {
    // extract components from the date and time fields
    int seconds = (time & 0x1F) * 2;
    int minutes = (time >> 5) & 0x3F;
    int hours = (time >> 11) & 0x1F;

    int day = date & 0x1F;
    int month = (date >> 5) & 0x0F;
    int year = ((date >> 9) & 0x7F) + 1980;

    // format the date and time as a string
    sprintf(dateTimeStr, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hours, minutes, seconds);
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
            exit(1);
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

void addFile(char* filename, char* intpath, dirEntry* parentDir) {
    char* token;                            // token for strtok
    char path[MAXPATH];                     // max path length
    dirEntry* currentDir = parentDir;       // start from the parent directory

    // check if the file system is loaded
    fsLoadedCheck();

    // copy the directory path to a local variable
    strcpy(path, intpath);

    // copy the filename to a local variable
    char* file = malloc(strlen(filename));

    // tokenize the path and find the directory to add the file to
    token = strtok(path, "/");
    while (token != NULL) {
        dirEntry* foundEntry = findEntryInDirectory(currentDir, token);
        if (foundEntry == NULL) {
            fprintf(stderr, "Directory, %s, does not exist, cannot add file\n", token);
            exit(1);
        }
        // move to the next directory in the path
        currentDir = foundEntry;
        token = strtok(NULL, "/");
    }

    // check if the file name is too long
    if (strlen(basename(filename)) > MAXFILENAME) {
        fprintf(stderr, "File name is too long, cannot add file\n");
        exit(1);
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

dirEntry* findEntryFromPath(char* intpath, dirEntry* parentDir) {
    char* token;                            // token for strtok
    char path[MAXPATH];                     // max path length
    char filename[MAXFILENAME];             // name of the file to retrieve
    dirEntry* currentDir = parentDir;       // start from the parent directory
    dirEntry* file;                         // file to find

    // check if the file system is loaded
    fsLoadedCheck();

    // copy the directory path to a local variable
    strcpy(path, intpath);

    // get the filename
    extract_filename(intpath, filename);

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
        exit(1);
    }

    // extract the file
    logMessage("Extracting file \"%s\"\n", intpath);
    _extractFile(file);

    printf("Extracted file \"%s\"\n", intpath);
    return;
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
    blockIndex = parentDir->first_cluster_low;

    // check if the entry is valid
    if (entry == NULL) {
        fprintf(stderr, "File or directory \"%s\" does not exist, cannot remove\n", intpath);
        exit(1);
    }

    // check if the entry is a directory, check if it is empty
    if (entry->attributes == ATTR_DIRECTORY) {
        if (!isDirectoryEmpty(entry)) {
            fprintf(stderr, "Directory \"%s\" is not empty, cannot remove\n", intpath);
            exit(1);
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

void interactiveShell(char* fsname) {
    char command[256];
    char arg1[256];
    char arg2[256];
    dirEntry* root = NULL;
    dirEntry* currentDir = NULL;
    char fullPath[MAXPATH] = "/";

    // Load the filesystem
    if (fsname == NULL) {
        fprintf(stderr, "No filesystem loaded. Use 'createfs' to create a new filesystem, or 'loadfs' to load one.\n\n");
    }
    else {
        loadfs(fsname);
        root = (dirEntry*)&blocks[0];
        currentDir = root;
    }

    printf("Interactive shell for filesystem %s. Type 'help' for a list of commands.\n", fsname);

    while (1) {
        getFullPath(currentDir, fullPath);
        printf("%s > ", fullPath);
        if (fgets(command, sizeof(command), stdin) == NULL) {
            printf("\nExiting shell.\n");
            break;
        }

        // Remove trailing newline
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "exit") == 0) {
            printf("Exiting shell.\n");
            break;
        } else if (strcmp(command, "help") == 0) {
            printf("Available commands:\n");
            printf("  help                            - Show this help message\n");
            printf("  exit                            - Exit the shell\n");
            printf("  ls                              - List the contents of the current directory\n");
            printf("  cd <internal path>              - Change the current directory\n");
            printf("  cat <internal path>             - Display the contents of a file\n");
            printf("  rm <internal path>              - Remove a file or directory from the file system\n");
            printf("  mkdir <internal path>           - Add a directory to the file system\n");
            printf("  tree                            - List the contents of the file system\n");
            printf("  addfile <path> <internal path>  - Add a file to the file system\n");
            printf("  touch <internal path>           - Create a new file/update the timestamp of a file\n");
            printf("  extract <internal path>         - Extract a file from the file system\n");
            printf("  createfs <fsname>               - Create a new file system\n");
            printf("  loadfs <fsname>                 - Load a file system\n");
            printf("  mount <mountpath>               - Mount the file system at the specified path\n");
        } else if (strcmp(command, "tree") == 0) {
            printDirectoryTree(currentDir);
            printf("\n");
        } else if (strcmp(command, "ls") == 0) {
            listDirectory(currentDir);
            printf("\n");
        } else if (sscanf(command, "cat %s", arg1)) {
            catFile(arg1, currentDir);
        } else if (sscanf(command, "addfile %s %s", arg1, arg2) == 2) {
            addFile(arg1, arg2, root);
        } else if (sscanf(command, "touch %s", arg1)) {
            touchFile(arg1, currentDir);
        } else if (sscanf(command, "mkdir %s", arg1) == 1) {
            addDirectory(arg1, root);
        } else if (sscanf(command, "rm %s", arg1) == 1) {
            removeDirectoryEntry(arg1, root);
        } else if (sscanf(command, "extract %s", arg1) == 1) {
            extractFile(arg1, root);
        } else if (sscanf(command, "createfs %s", arg1) == 1) {
            createfs(arg1);
            printf("Created new file system '%s'\n", arg1);
        } else if (sscanf(command, "loadfs %s", arg1) == 1) {
            loadfs(arg1);
            root = (dirEntry*)&blocks[0];
            currentDir = root;
            printf("Loaded file system '%s'\n", arg1);
        } else if (sscanf(command, "cd %s", arg1) == 1) {
            // if the path starts with '/', start from the root
            if (arg1[0] == '/') {
                currentDir = root;
            }
            dirEntry* newDir = findEntryFromPath(arg1, currentDir);
            if (newDir != NULL && newDir->attributes & ATTR_DIRECTORY) {
                currentDir = newDir;
            } else {
                printf("Directory not found: %s\n", arg1);
            }
        } else if (sscanf(command, "mount %s", arg1) == 1) {
            mountfs(arg1, fsname);
        } else {
            printf("Unknown command. Type 'help' for a list of commands.\n");
        }
    }
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

// Section for FUSE

dirEntry* fuseRoot = NULL;

static int fs_getattr(const char *path, struct stat *st) {
    int res = 0;
    char* localpath = malloc(strlen(path));
    dirEntry* file = NULL;
    int numSubdirs = 0;

    logMessage("Getting attributes for %s\n", path);

    strcpy(localpath, path);
    file = findEntryFromPath(localpath, fuseRoot);

    if (file == NULL) {
        return -ENOENT;
    } else

    if (file->name[0] == 0x5F || file->attributes == ATTR_DELETED) {
        res = -ENOENT;
    }

    numSubdirs = getNumSubdirs(file);

    memset(st, 0, sizeof(struct stat));

    if(strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = numSubdirs;
    } else if (file->attributes & ATTR_DIRECTORY) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = numSubdirs;
    } else {
        st->st_mode = S_IFREG | 0644;
        st->st_nlink = 1;
        st->st_size = file->size;
    }

    logMessage("Attributes for %s: mode: %d, nlink: %d, size: %d\n", path, st->st_mode, st->st_nlink, st->st_size);

    // set date and time attributes
    st->st_ctime = convertFATDateTime(file->create_date, file->create_time);
    st->st_mtime = convertFATDateTime(file->last_write_date, file->last_write_time);
    st->st_atime = convertFATDateTime(file->last_access_date, 0);

    free(localpath);
    return res;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    // function is very similar to listDirectory, but uses the FUSE filler function to add entries to the directory
    // see listDirectory for more detailed comments

    fprintf(stderr, "Reading directory %s\n", path);

    (void) offset;
    (void) fi;

    unsigned short currentDirBlockIndex = USHRT_MAX;
    block* currentDirBlock = NULL;
    dirEntry* parentDirEntry = NULL;
    dirEntry* currentDirEntry = NULL;
    unsigned short currentDirEntryIndex = USHRT_MAX;
    char* localpath = malloc(strlen(path));

    logMessage("Reading directory %s\n", path);

    strcpy(localpath, path);

    // if (strcmp (path, "/") != 0) {
    //     return -ENOENT;
    // }

    parentDirEntry = findEntryFromPath(localpath, fuseRoot);

    if (parentDirEntry == NULL) {
        return -ENOENT;
    }

    currentDirBlockIndex = parentDirEntry->first_cluster_low;
    currentDirBlock = &blocks[currentDirBlockIndex];

    currentDirEntryIndex = 0;
    currentDirEntry = (dirEntry*)&currentDirBlock->data[currentDirEntryIndex];

    // iterate through the directory entries in the block until the last entry is found
    while (currentDirEntry->isLast != LASTENTRY && currentDirEntry != NULL) {
        // don't list the deleted entries
        if (currentDirEntry->name[0] == 0x5F || currentDirEntry->attributes == ATTR_DELETED) {
            currentDirEntry = getNextEntry(currentDirEntry, parentDirEntry);
            continue;
        }

        // get the attributes of the entry
        struct stat st;

        if (currentDirEntry->attributes & ATTR_DIRECTORY) {
            st.st_mode = S_IFDIR | 0755;
        } else {
            st.st_mode = S_IFREG | 0644;
        }

        const struct stat* st_const = &st;

        char name[12] = {0};
        strncpy(name, currentDirEntry->name, 11);
        filler(buf, name, st_const, 0);

        currentDirEntry = getNextEntry(currentDirEntry, parentDirEntry);
    }

    if (currentDirEntry == NULL) {
        free(localpath);
        return -ENOENT;
    }

    // if it's deleted, don't list it
    if (currentDirEntry->name[0] == 0x5F || currentDirEntry->attributes == ATTR_DELETED) {
        free(localpath);
        return -ENOENT;
    }

    // get the attributes of the entry
    struct stat st;

    if (currentDirEntry->attributes & ATTR_DIRECTORY) {
        st.st_mode = S_IFDIR | 0755;
    } else {
        st.st_mode = S_IFREG | 0644;
    }

    const struct stat* st_const = &st;

    char name[12] = {0};
    strncpy(name, currentDirEntry->name, 11);
    filler(buf, name, st_const, 0);

    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // function to read data from a file
    // this function is very similar to _extractFile, but uses the FUSE buffer to write the data to
    // see _extractFile for more detailed comments

    unsigned short block = USHRT_MAX;
    unsigned int bytesToWrite = 0;
    char* localpath = malloc(strlen(path));
    dirEntry* file = NULL;
    (void) fi;

    logMessage("Reading file %s\n", path);

    strcpy(localpath, path);
    file = findEntryFromPath(localpath, fuseRoot);

    if (file == NULL || file->attributes & ATTR_DIRECTORY) {
        free(localpath);
        return -ENOENT;
    }

    size = file->size;

    if (offset > size) {
        free(localpath);
        return 0;
    }

    if (offset + size > size) {
        size = size - offset;
    }

    block = file->first_cluster_low;
    bytesToWrite = size;

    // loop through the blocks and write the data to the buffer
    while (bytesToWrite > 0) {
        unsigned int numBytes = (bytesToWrite > BLOCKSIZE) ? BLOCKSIZE : bytesToWrite;
        memcpy(buf, &blocks[block].data, numBytes);
        buf += numBytes;
        bytesToWrite -= numBytes;
        block = FAT[block];
    }

    free(localpath);

    return size;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    char* localpath = malloc(strlen(path));
    dirEntry* file = NULL;

    logMessage("Opening file %s\n", path);

    strcpy(localpath, path);
    file = findEntryFromPath(localpath, fuseRoot);

    if (file == NULL || file->attributes & ATTR_DIRECTORY) {
        free(localpath);
        return -ENOENT;
    }

    fi->fh = (uint64_t)file;

    free(localpath);
    return 0;
}

static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    // function to create a new file
    // this function is very similar to createEmptyFile
    // see createEmptyFile for more detailed comments

    char *localpath = malloc(strlen(path));
    dirEntry *parentDir = NULL;
    char parentPath[MAXPATH];
    char filename[MAXFILENAME];

    (void) mode;
    (void) fi;

    logMessage("Creating file %s\n", path);

    strcpy(localpath, path);
    extract_path(localpath, parentPath);
    extract_filename(localpath, filename);

    parentDir = findParentFromPath(parentPath, fuseRoot);

    if (parentDir == NULL) {
        free(localpath);
        return -ENOENT;
    }

    createEmptyFile(filename, parentDir);

    free(localpath);
    return 0;
}

static int fs_mkdir(const char* path, mode_t mode) {
    char *localpath = malloc(strlen(path));
    dirEntry *parentDir = NULL;
    char parentPath[MAXPATH];
    char dirname[MAXFILENAME];

    (void) mode;

    logMessage("Creating directory %s\n", path);

    strcpy(localpath, path);
    extract_path(localpath, parentPath);
    extract_filename(localpath, dirname);

    parentDir = findParentFromPath(parentPath, fuseRoot);

    if (parentDir == NULL) {
        free(localpath);
        return -ENOENT;
    }

    _addDirectory(dirname, parentDir);

    free(localpath);
    return 0;
}

static int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    unsigned short block = USHRT_MAX;
    unsigned int bytesToWrite = 0;
    unsigned int bytesWritten = 0;
    char *localpath = malloc(strlen(path));
    dirEntry *file = NULL;

    (void) fi;

    strcpy(localpath, path);

    logMessage("Writing to file %s\n", localpath);
    file = findEntryFromPath(localpath, fuseRoot);

    logMessage("Offset: %ld\n", offset);
    logMessage("Size: %ld\n", size);
    logMessage("File size: %d\n", file->size);
    if (file == NULL || file->attributes & ATTR_DIRECTORY) {
        logMessage("File not found\n");
        free(localpath);
        return -ENOENT;
    }

    // TODO: this doesn't account for if the block was not fully written to and space is left in the block
    if (offset > file->size) {
        logMessage("Offset greater than file size\n");
        free(localpath);
        return 0;
    }

    if (offset + size > file->size) {
        logMessage("Expanding file size\n");
        file->size = offset + size;
    }

    block = file->first_cluster_low;

    logMessage("Block: %d\n", block);

    // navigate to the block based on the offset
    off_t blockOffset = offset / BLOCKSIZE;
    off_t localOffset = offset % BLOCKSIZE;

    logMessage("Block offset: %d\n", blockOffset);
    logMessage("Local offset: %d\n", localOffset);

    // loop through the allocated blocks to find the block at the offset
    for (int i = 0; i < blockOffset; i++) {
        // if the block is the last block, allocate a new block
        if (FAT[block] == USHRT_MAX) {
            logMessage("Allocating new block\n");
            unsigned short newBlock = findFreeBlock();
            FAT[block] = newBlock;
            FAT[newBlock] = USHRT_MAX;
        }

        block = FAT[block];
    }

    // if

    // while (blockOffset > 0) {
    //     block = FAT[block];
    //     blockOffset--;
    //     if (block == USHRT_MAX) {
    //         logMessage("Allocating new block\n");
    //         unsigned short newBlock = findFreeBlock();
    //         FAT[block] = newBlock;
    //         FAT[newBlock] = USHRT_MAX;
    //         block = newBlock;
    //     }
    //     logMessage("\tBlock: %d\n", block);
    //     logMessage("\tBlock offset: %d\n", blockOffset);
    // }

    logMessage("Starting write at block %d\n", block);

    // write the data
    bytesToWrite = size;
    while (bytesToWrite > 0) {
        unsigned int numBytes = (bytesToWrite > (BLOCKSIZE - localOffset)) ? (BLOCKSIZE - localOffset) : bytesToWrite;

        logMessage("\tNum bytes: %d\n", numBytes);
        logMessage("\tBytes written: %d\n", bytesWritten);
        logMessage("\tBytes to write: %d\n", bytesToWrite);

        memcpy(&blocks[block].data[localOffset], buf + bytesWritten, numBytes);

        bytesWritten += numBytes;
        bytesToWrite -= numBytes;

        if (bytesToWrite > 0) {
            if (FAT[block] == USHRT_MAX) {
                logMessage("\tAllocating new block\n");
                unsigned short newBlock = findFreeBlock();
                FAT[block] = newBlock;
                FAT[newBlock] = USHRT_MAX;
            }
            logMessage("\tMoving to block %d\n", FAT[block]);
            block = FAT[block];
            localOffset = 0;
        }
    }

    free(localpath);
    return size;
}

static int fs_rmdir(const char *path) {
    dirEntry *entry;
    char *localpath = malloc(strlen(path));

    logMessage("Removing directory %s\n", path);

    strcpy(localpath, path);
    entry = findEntryFromPath(localpath, fuseRoot);

    if (entry == NULL) {
        free(localpath);
        return -ENOENT;
    }

    if (!isDirectoryEmpty(entry)) {
        free(localpath);
        return -ENOTEMPTY;
    }

    removeDirectoryEntry(localpath, fuseRoot);
    free(localpath);
    return 0;
}

static int fs_unlink(const char *path) {
    dirEntry *entry;
    char *localpath = malloc(strlen(path));

    logMessage("Unlinking file %s\n", path);

    strcpy(localpath, path);
    entry = findEntryFromPath(localpath, fuseRoot);

    if (entry == NULL) {
        free(localpath);
        return -ENOENT;
    }

    removeDirectoryEntry(localpath, fuseRoot);
    free(localpath);
    return 0;
}

static int fs_statfs(const char *path, struct statvfs *st) {
    (void) path;

    logMessage("Getting filesystem stats\n");

    // Zero out the statvfs structure
    memset(st, 0, sizeof(struct statvfs));

    // Fill the statvfs structure with information about the filesystem
    st->f_bsize = BLOCKSIZE;                // Filesystem block size
    st->f_frsize = BLOCKSIZE;               // Fragment size
    st->f_blocks = MAXBLOCKS;               // Total number of blocks
    st->f_bfree = 0;                        // Total number of free blocks
    st->f_bavail = 0;                       // Number of free blocks available to non-privileged processes
    st->f_files = 0;                        // Total number of file nodes (inodes)
    st->f_ffree = 0;                        // Total number of free file nodes
    st->f_favail = 0;                       // Number of free file nodes available to non-privileged processes
    st->f_fsid = 0;                         // Filesystem ID
    st->f_flag = 0;                         // Mount flags
    st->f_namemax = MAXFILENAME;            // Maximum length of filenames

    // Calculate the number of free blocks and file nodes
    for (int i = 0; i < MAXBLOCKS; i++) {
        if (FAT[i] == 0) {
            st->f_bfree++;
            st->f_bavail++;
        }
    }

    // Calculate the number of file nodes
    for (int i = 0; i < MAXBLOCKS * (BLOCKSIZE / sizeof(dirEntry)); i++) {
        dirEntry *entry = (dirEntry *)&blocks[i / (BLOCKSIZE / sizeof(dirEntry))].data[(i % (BLOCKSIZE / sizeof(dirEntry))) * sizeof(dirEntry)];
        if (entry->name[0] == 0 || entry->attributes == ATTR_DELETED) {
            st->f_ffree++;
            st->f_favail++;
        } else {
            st->f_files++;
        }
    }

    return 0;
}

static int fs_release(const char *path, struct fuse_file_info *fi) {
    (void) path;
    (void) fi;
    logMessage("File released\n");
    return 0;
}

static int fs_getxattr(const char *path, const char *name, char *value, size_t size) {
    dirEntry *file;
    char *localpath = malloc(strlen(path));

    logMessage("Getting xattr %s for file %s\n", name, path);

    strcpy(localpath, path);
    file = findEntryFromPath(localpath, fuseRoot);

    if (file == NULL) {
        free(localpath);
        return -ENOENT;
    }

    if (strcmp(name, "user.attr") == 0) {
        if (size == 0) {
            free(localpath);
            return strlen(file->name);
        }

        if (size < strlen(file->name)) {
            free(localpath);
            return -ERANGE;
        }

        strcpy(value, file->name);
        free(localpath);
        return strlen(file->name);
    } else if (strcmp(name, "user.size") == 0) {
        free(localpath);
        return file->size;
    } else if (strcmp(name, "security.capability") == 0) {
        free(localpath);
        return 0; // No capabilities
    }

    free(localpath);
    return -ENODATA; // Attribute not found
}

static int fs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
    dirEntry *file;
    char *localpath = malloc(strlen(path));

    logMessage("Setting xattr %s for file %s\n", name, path);

    strcpy(localpath, path);
    file = findEntryFromPath(localpath, fuseRoot);

    if (file == NULL) {
        free(localpath);
        return -ENOENT;
    }

    if (strcmp(name, "user.attr") == 0) {
        if (size > MAXFILENAME) {
            free(localpath);
            return -ENOSPC;
        }

        strncpy(file->name, value, size);
        file->name[size] = '\0'; // Null terminate the string
        free(localpath);
        return 0;
    }

    free(localpath);
    return -ENOTSUP; // Operation not supported
}

static int fs_utimens(const char *path, const struct timespec tv[2]) {
    dirEntry *file;
    char *localpath = malloc(strlen(path));
    struct tm *tm;
    time_t t;
    short date, time;

    logMessage("Updating timestamps for file %s\n", path);

    strcpy(localpath, path);
    file = findEntryFromPath(localpath, fuseRoot);

    if (file == NULL) {
        free(localpath);
        return -ENOENT;
    }

    // convert timespec to FAT date and time format
    // updating the last access time
    t = tv[0].tv_sec;
    tm = localtime(&t);

    date = ((tm->tm_year - 80) << 9) | ((tm->tm_mon + 1) << 5) | (tm->tm_mday);
    time = (tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2);

    file->last_access_date = date;

    // updating the last modification time
    t = tv[1].tv_sec;
    tm = localtime(&t);

    date = ((tm->tm_year - 80) << 9) | ((tm->tm_mon + 1) << 5) | (tm->tm_mday);
    time = (tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2);

    file->last_write_date = date;
    file->last_write_time = time;

    free(localpath);
    return 0;
}

static int fs_truncate(const char *path, off_t size) {
    char* localpath = malloc(strlen(path));
    dirEntry* file = NULL;
    (void) size;

    strcpy(localpath, path);
    file = findEntryFromPath(localpath, fuseRoot);

    if (file == NULL || file->attributes & ATTR_DIRECTORY) {
        free(localpath);
        return -ENOENT;
    }

    logMessage("Truncating file %s to size %ld\n", path, size);

    if (size == 0) {
        // free the blocks used by the file
        unsigned short firstBlock = file->first_cluster_low;
        unsigned short blockToFree = firstBlock;
        logMessage("\tTruncate: first block: %d\n", firstBlock);
        logMessage("\tTruncate: size: %d\n", size);
        while (blockToFree != USHRT_MAX) {
            unsigned short nextBlock = FAT[blockToFree];
            // 0 out the block
            memset(&blocks[blockToFree].data, 0, BLOCKSIZE);
            FAT[blockToFree] = 0;
            logMessage("\tFreeing block %d\n", blockToFree);
            blockToFree = nextBlock;
        }
        FAT[firstBlock] = USHRT_MAX;

        file->size = 0;

        free(localpath);
        return 0;
    }

    // Implement the logic to truncate the file to the specified size
    if (size < file->size) {
        unsigned short block = file->first_cluster_low;
        off_t offset = size;
        while (offset > BLOCKSIZE) {
            block = FAT[block];
            offset -= BLOCKSIZE;
        }

        if (offset > 0) {
            memset(&blocks[block].data[offset], 0, BLOCKSIZE - offset);
        }

        block = FAT[block];
        while (block != USHRT_MAX) {
            unsigned short next_block = FAT[block];
            FAT[block] = 0;
            block = next_block;
        }
    }

    file->size = size;

    free(localpath);
    return 0;
}

static struct fuse_operations fuse_ops = {
    .getattr = fs_getattr,
    .truncate = fs_truncate,
    .readdir = fs_readdir,
    .open = fs_open,
    .read = fs_read,
    .write = fs_write,
    .create = fs_create,
    .mkdir = fs_mkdir,
    .unlink = fs_unlink,
    .rmdir = fs_rmdir,
    .statfs = fs_statfs,
    .release = fs_release,
    .getxattr = fs_getxattr,
    .setxattr = fs_setxattr,
    .utimens = fs_utimens
};


int mountfs(char* mountpath, char* filesystem) {
    int fuse_argc = 3;
    char* fuse_argv[3] = {"myfs", "-d", mountpath};
    int ret;

    fprintf(stderr, "Mounting filesystem %s at %s\n", filesystem, mountpath);

    fuseRoot = (dirEntry*)&blocks[0];

    ret = fuse_main(fuse_argc, fuse_argv, &fuse_ops, NULL);

    return ret;
}


int main(int argc, char *argv[]) {
    int create_flag = 0;        // flag to check if we need to create a new file system
    int list_flag = 0;          // flag to check if we need to list the contents of the file system
    int add_flag = 0;           // flag to check if we need to add a file to the file system
    int remove_flag = 0;        // flag to check if we need to remove a file from the file system
    int add_dir_flag = 0;       // flag to check if we need to add a directory to the file system
    int extract_flag = 0;       // flag to check if we need to extract a file from the file system
    int interactive_flag = 0;   // flag to check if we need to start the interactive shell
    int mount_flag = 0;         // flag to check if we need to mount the file system
    int opt;                    // option for the command line arguments
    char* fsname = NULL;        // name of the file system
    char* filename = NULL;      // name of the file to add
    char* intpath = NULL;       // internal path of the file to add
    char* mountpath = NULL;     // path to mount the file system
    FILE* fsfile = NULL;        // file system file
    dirEntry* root = NULL;      // pointer to the root directory

    // parse the command line arguments
    while ((opt = getopt(argc, argv, "f:clvi:a:r:d:e:Im:h")) != -1) {
        switch (opt) {
        case 'f': // file system name
            fsname = malloc(strlen(optarg));
            strcpy(fsname, optarg);
            break;
        case 'c': // create a new file system
            create_flag = 1;
            break;
        case 'l': // list the contents of the file system
            list_flag = 1;
            break;
        case 'v': // verbose flag
            verbose = 1;
            break;
        case 'a': // add a file to the file system
            add_flag = 1;
            filename = malloc(strlen(optarg));
            strcpy(filename, optarg);
            break;
        case 'i': // internal path of file to add
            intpath = malloc(strlen(optarg));
            strcpy(intpath, optarg);
            break;
        case 'r': // remove a file from the file system
            remove_flag = 1;
            intpath = strdup(optarg);
            break;
        case 'd': // add a directory to the file system
            add_dir_flag = 1;
            filename = malloc(strlen(optarg));
            strcpy(filename, optarg);
            break;
        case 'e': // extract a file from the file system
            extract_flag = 1;
            intpath = strdup(optarg);
            break;
        case 'I': // interactive shell
            interactive_flag = 1;
            break;
        case 'm': // mount the file system
            mount_flag = 1;
            mountpath = strdup(optarg);
            break;
        case 'h': // help
            printUsage(argv[0]);
            exit(EXIT_SUCCESS);
            break;
        default: /* '?' */
            fprintf(stderr, "Use %s -h for help\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // check if the file system name is provided
    if (fsname == NULL && !interactive_flag && !create_flag) {
        fprintf(stderr, "No file system name provided, exiting\n");
        fprintf(stderr, "Use %s -h for help\n", argv[0]);
        exit(1);
    }


    // check if we need to start the interactive shell
    if (interactive_flag) {
        interactiveShell(fsname);
        exit(EXIT_SUCCESS);
    }

    // check if we need to create the fs
    if (create_flag) {
      // create the file system
      createfs(fsname);
    }
    else {
      // load the file system
      loadfs(fsname);
    }

    // check if a file system is loaded
    fsLoadedCheck();

    // set the root directory
    root = (dirEntry*)&blocks[0];

    // check if we are adding a directory
    if (add_dir_flag && filename != NULL) {
        addDirectory(filename, root);
    }

    // check if we are adding a file
    if (add_flag) {
        // check if the internal path is set
        if (intpath == NULL) {
            fprintf(stderr, "No internal path provided, exiting\n");
            exit(1);
        }

        //check if the filename is set
        if (filename == NULL) {
            fprintf(stderr, "No filename provided, exiting\n");
            exit(1);
        }

        // check that we're not adding a directory
        if (add_dir_flag) {
            fprintf(stderr, "Cannot add a directory and a file at the same time, exiting\n");
            exit(1);
        }

        addFile(filename, intpath, root);

        printf("Added %s to %s\n", filename, intpath);
    }

    // check if we are extracting a file
    if (extract_flag) {
        // check that the filename is set
        if (intpath == NULL) {
            fprintf(stderr, "No file provided, exiting\n");
            exit(1);
        }

        // check that we're not adding a directory or a file
        if (add_dir_flag || add_flag) {
            fprintf(stderr, "Cannot add a directory or a file and extract a file at the same time, exiting\n");
            exit(1);
        }

        // extract the file
        extractFile(intpath, root);
    }

    // check if we need to list the contents of the file system
    if (list_flag) {
        printDirectoryTree(root);
        printf("\n");
    }

    // check if we need to remove a file
    if (remove_flag) {
        // check that the filename is set
        if (intpath == NULL) {
            fprintf(stderr, "No file provided, exiting\n");
            exit(1);
        }

        // check that we're not adding a directory or a file
        if (add_dir_flag || add_flag) {
            fprintf(stderr, "Cannot add a directory or a file and remove a file at the same time, exiting\n");
            exit(1);
        }

        // remove the file
        removeDirectoryEntry(intpath, root);
    }

    // check if we need to mount the file system
    if (mount_flag) {
        // check that the mount path is set
        if (mountpath == NULL) {
            fprintf(stderr, "No mount path provided, exiting\n");
            exit(1);
        }

        // mount the file system
        return mountfs(mountpath, fsname);
    }

    // Free the memory
    free(fsname);
    free(filename);
    free(intpath);

    return 0;
}
