#include <stdio.h>
#include <stdlib.h>
#include "cli.h"

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
    fprintf(stderr, "Note  Reading large files (>131kb or so) may have undocumented behavior when mounted\n");
    fprintf(stderr, "\n");
    exit(1);
}
