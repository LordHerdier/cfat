#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cli.h"
#include "globals.h"
#include "fsops.h"
#include "directory.h"
#include "file.h"
#include "shell.h"
#include "fuse_ops.h"

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
