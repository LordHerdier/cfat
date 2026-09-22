#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <strings.h>
#include "fsops.h"
#include "directory.h"
#include "globals.h"

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
        fprintf(stderr, "Error creating file system (name not provided), exiting\n");
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
