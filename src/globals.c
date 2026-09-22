#include <stdarg.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "globals.h"

// global variables
char* fs = NULL;            //pointer to the memory mapped file system
unsigned short* FAT = NULL; //pointer to the File Allocation Table
block* blocks = NULL;       //pointer to the blocks of the file system
int verbose = 0;            //verbose flag

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

void fsLoadedCheck() {
    // check if the file system is loaded
    if (fs == NULL) {
        fprintf(stderr, "No file system loaded, exiting\n");
        exit(1);
    }
}
