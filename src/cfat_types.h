#ifndef CFAT_TYPES_H
#define CFAT_TYPES_H

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

#endif // CFAT_TYPES_H
