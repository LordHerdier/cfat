#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fuse.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "fuse_ops.h"
#include "direntry.h"
#include "file.h"
#include "directory.h"
#include "fat.h"
#include "datetime.h"
#include "globals.h"

static dirEntry* fuseRoot = NULL;

static int fs_getattr(const char *path, struct stat *st) {
    int res = 0;
    char* localpath = malloc(strlen(path) + 1);
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
    char* localpath = malloc(strlen(path) + 1);

    logMessage("Reading directory %s\n", path);

    strcpy(localpath, path);

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
    dirEntry* file = NULL;                     // file to read
    unsigned short block = 0;                  // first block of the file
    unsigned int fileSize = 0;                 // size of the file
    unsigned int bytesRead = 0;                // number of bytes read
    unsigned int bytesToRead = 0;              // number of bytes to read
    unsigned int blockOffset = 0;              // offset within the block
    char* localpath = strdup(path);            // duplicate the path for manipulation

    // check if the file system is loaded
    fsLoadedCheck();

    // find the file to read
    file = findEntryFromPath(localpath, fuseRoot);

    // check if the file exists
    if (file == NULL) {
        free(localpath);
        return -ENOENT;
    }

    // check if the file is a directory
    if (file->attributes == ATTR_DIRECTORY) {
        free(localpath);
        return -EISDIR;
    }

    // get the file's size and first block
    fileSize = file->size;
    block = file->first_cluster_low;

    // check if offset is beyond file size
    if (offset >= fileSize) {
        free(localpath);
        return 0;
    }

    // adjust size if read goes beyond file size
    if (offset + size > fileSize) {
        size = fileSize - offset;
    }

    // navigate to the correct block based on the offset
    while (offset >= BLOCKSIZE) {
        block = FAT[block];
        offset -= BLOCKSIZE;
    }

    blockOffset = offset; // offset within the block
    bytesRead = 0;

    // read data block by block
    while (size > 0) {
        bytesToRead = (size > (BLOCKSIZE - blockOffset)) ? (BLOCKSIZE - blockOffset) : size;
        memcpy(buf + bytesRead, blocks[block].data + blockOffset, bytesToRead);
        size -= bytesToRead;
        bytesRead += bytesToRead;
        blockOffset = 0; // reset block offset for subsequent reads

        // move to the next block if necessary
        if (size > 0) {
            block = FAT[block];
            if (block == USHRT_MAX) {
                break;
            }
        }
    }

    free(localpath);
    return bytesRead;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    char* localpath = malloc(strlen(path) + 1);
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

    char *localpath = malloc(strlen(path) + 1);
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
    char *localpath = malloc(strlen(path) + 1);
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
    char *localpath = malloc(strlen(path) + 1);
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
    char *localpath = malloc(strlen(path) + 1);

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
    char *localpath = malloc(strlen(path) + 1);

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
    char *localpath = malloc(strlen(path) + 1);

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
    char *localpath = malloc(strlen(path) + 1);

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
    char *localpath = malloc(strlen(path) + 1);
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
    char* localpath = malloc(strlen(path) + 1);
    dirEntry* file = NULL;
    (void) size;

    strcpy(localpath, path);

    logMessage("Truncating file %s to size %ld\n", path, size);

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
    int fuse_argc;
    char* fuse_argv[3];
    int ret;

    fprintf(stderr, "Mounting filesystem %s at %s\n", filesystem, mountpath);

    fuseRoot = (dirEntry*)&blocks[0];

    fuse_argv[0] = "cfs";
    fuse_argv[1] = mountpath;

    if (verbose == 1) {
        logMessage("Mounting filesystem in debug mode\n");
        fuse_argv[2] = "-d";
        fuse_argc = 3;
    } else {
        fuse_argc = 2;
    }

    ret = fuse_main(fuse_argc, fuse_argv, &fuse_ops, NULL);

    return ret;
}
