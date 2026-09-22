#include <stdio.h>
#include <string.h>
#include "shell.h"
#include "fsops.h"
#include "directory.h"
#include "file.h"
#include "direntry.h"
#include "fuse_ops.h"
#include "globals.h"

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
