/**************************************************************
 * Class::  CSC-415-01 Fall 2025
 * Name:: Ian Wang
 * Student IDs:: 924005755
 * GitHub-Name:: IannnWENG
 * Group-Name:: BobaTea
 * Project:: Basic File System
 *
 * File:: fsStruct.h
 *
 * Description:: File system data structure definitions
 *
 **************************************************************/

#ifndef _FSSTRUCT_H
#define _FSSTRUCT_H

#include <stdint.h>
#include <time.h>

#define BLOCK_SIZE 512
#define MAX_FILENAME_LEN 255
#define MAX_PATH_LEN 4096
#define MAX_OPEN_FILES 20
#define MAX_DIR_ENTRIES 6 // maximum 6 directory entries per directory block
// file data layout constants (fit header into 512 bytes)
#define MAX_FILE_BLOCKS 120 // (512 - 4 - 4 - 8 - 4) / 4 = 123, leave some padding
// bitmap helpers
#define BITS_PER_BYTE 8
#define BITMAP_BITS_PER_BLOCK (BLOCK_SIZE * BITS_PER_BYTE)

// file type definitions
#define FT_UNUSED 0
#define FT_FILE 1
#define FT_DIR 2

// file system magic numbers
#define FS_MAGIC 0x12345678
#define FS_VERSION 1

// superblock structure
typedef struct
{
    uint32_t magic;        // file system magic numbers
    uint32_t version;      // file system version
    uint64_t totalBlocks;  // total blocks
    uint64_t blockSize;    // block size
    uint64_t freeBlocks;   // free blocks
    uint64_t rootDirBlock; // root directory block position
    // bitmap-based free space tracking
    uint64_t bitmapStart;  // first LBA of the free-space bitmap
    uint64_t bitmapBlocks; // number of blocks occupied by the bitmap
    char volumeName[32];   // volume name
    time_t createTime;     // creation time
    time_t lastMountTime;  // last mount time
} SuperBlock;

// directory entry structure
typedef struct
{
    char filename[64];   // filename (simplified)
    uint32_t fileType;   // file type (FT_FILE, FT_DIR)
    uint32_t startBlock; // start block (for FT_DIR: DirBlock; for FT_FILE: FileHeader block)
    uint32_t fileSize;   // file size
    uint32_t createTime; // creation time (simplified to 32-bit)
    uint32_t modifyTime; // modification time (simplified to 32-bit)
} DirEntry;

// directory block structure
typedef struct
{
    uint32_t entryCount;               // directory entry count
    uint32_t nextDirBlock;             // next directory block (0 means none)
    DirEntry entries[MAX_DIR_ENTRIES]; // directory entry array
    char padding[BLOCK_SIZE - sizeof(uint32_t) * 2 - sizeof(DirEntry) * MAX_DIR_ENTRIES];
} DirBlock;

// file header block (for FT_FILE)
typedef struct
{
    uint32_t magic;                       // magic for file header validation
    uint32_t reserved;                    // alignment
    uint64_t fileSize;                    // total file size in bytes
    uint32_t dataBlockCount;              // number of data blocks used
    uint32_t dataBlocks[MAX_FILE_BLOCKS]; // LBA indices for data blocks
    char padding[BLOCK_SIZE - (sizeof(uint32_t) * 2 + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t) * MAX_FILE_BLOCKS)];
} FileHeader;

// file control block (FCB)
typedef struct
{
    int inUse;                           // whether in use
    char filename[MAX_FILENAME_LEN + 1]; // filename
    uint64_t currentPos;                 // current position
    uint64_t fileSize;                   // file size
    uint64_t startBlock;                 // start block
    int flags;                           // open flags
    time_t lastAccess;                   // last access time
} FileControlBlock;

// legacy free list structure (unused after switching to bitmap, kept for compatibility)
typedef struct
{
    uint64_t nextFreeBlock;                                 // next free block
    uint32_t freeBlockCount;                                // free block count
    uint64_t freeBlocks[BLOCK_SIZE / sizeof(uint64_t) - 2]; // free block list
} FreeBlockList;

// global variable declarations
extern SuperBlock g_superBlock;
extern FileControlBlock g_fcbArray[MAX_OPEN_FILES];
extern char g_currentPath[MAX_PATH_LEN];
extern const uint32_t FILEHEADER_MAGIC;
extern uint32_t g_lastDirForStat;

// function declarations
int fs_format(uint64_t totalBlocks, uint64_t blockSize);
int fs_mount(void);
int fs_unmount(void);
uint64_t fs_allocateBlock(void);
int fs_freeBlock(uint64_t blockNumber);
int fs_findFile(const char *path, DirEntry *entry);
int fs_createFile(const char *path, uint32_t fileType);
int fs_deleteFile(const char *path);
int fs_readFile(const char *path, void *buffer, uint64_t offset, uint64_t count);
int fs_writeFile(const char *path, const void *buffer, uint64_t offset, uint64_t count);
int fs_getFileSize(const char *path, uint64_t *size);
int fs_setFileSize(const char *path, uint64_t size);
// helpers
int fs_resolvePath(const char *path, uint32_t *outDirBlock, char *outName, size_t outNameSize);
int fs_loadDir(uint32_t dirBlock, DirBlock *dir);
int fs_storeDir(uint32_t dirBlock, const DirBlock *dir);
int fs_findInDir(uint32_t dirBlock, const char *name, DirEntry *entry, uint32_t *indexInDir);
int fs_rename(const char *srcPath, const char *dstPath);

#endif
