/**************************************************************
 * Class::  CSC-415-01 Fall 2025
 * Name:: Ian Wang
 * Student IDs:: 924005755
 * GitHub-Name:: IannnWENG
 * Group-Name:: BobaTea
 * Project:: Basic File System
 *
 * File:: fsCore.c
 *
 * Description:: File system core functionality implementation
 *
 **************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "fsLow.h"
#include "fsStruct.h"
#include "mfs.h"
#include "b_io.c"

// magic value for file header validation
const uint32_t FILEHEADER_MAGIC = 0xC5C4F11E; // "CSC4 FILE" stylized

// forward helpers
static int fs_expandDirectoryIfNeeded(uint32_t dirBlock, DirBlock *dir, uint32_t *outUseBlock);
static int fs_isDirectoryEmpty(uint32_t dirBlock);
static int fs_addEntryToDir(uint32_t dirBlock, const DirEntry *newEntry);
static int fs_removeEntryFromDir(uint32_t dirBlock, uint32_t index);

int fs_loadDir(uint32_t dirBlock, DirBlock *dir)
{
    if (LBAread(dir, 1, dirBlock) != 1)
        return -1;
    return 0;
}

int fs_storeDir(uint32_t dirBlock, const DirBlock *dir)
{
    if (LBAwrite((void *)dir, 1, dirBlock) != 1)
        return -1;
    return 0;
}

int fs_findInDir(uint32_t dirBlock, const char *name, DirEntry *entry, uint32_t *indexInDir)
{
    DirBlock cur;
    uint32_t curBlock = dirBlock;
    while (curBlock != 0)
    {
        if (fs_loadDir(curBlock, &cur) != 0)
            return -1;
        for (uint32_t i = 0; i < cur.entryCount; i++)
        {
            if (strcmp(cur.entries[i].filename, name) == 0)
            {
                if (entry)
                    *entry = cur.entries[i];
                if (indexInDir)
                    *indexInDir = i;
                return 0;
            }
        }
        if (cur.nextDirBlock == 0)
            break;
        curBlock = cur.nextDirBlock;
    }
    return -1;
}

// path resolver: returns parent directory block and last name component
int fs_resolvePath(const char *path, uint32_t *outDirBlock, char *outName, size_t outNameSize)
{
    if (!path || !outDirBlock || !outName || outNameSize == 0)
        return -1;
    // handle absolute paths only; treat relative as from g_currentPath but simplified to root
    const char *p = path;
    if (*p == '/')
        p++;
    // copy to temp to tokenize
    char temp[MAX_PATH_LEN];
    strncpy(temp, p, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    uint32_t currentBlock = (uint32_t)g_superBlock.rootDirBlock;
    char *saveptr = NULL;
    char *token = strtok_r(temp, "/", &saveptr);
    char lastName[MAX_FILENAME_LEN + 1] = {0};
    while (token)
    {
        char *next = strtok_r(NULL, "/", &saveptr);
        if (next == NULL)
        {
            // this is the last name
            strncpy(lastName, token, MAX_FILENAME_LEN);
            lastName[MAX_FILENAME_LEN] = '\0';
            break;
        }
        // traverse into directory named token
        DirEntry e;
        if (fs_findInDir(currentBlock, token, &e, NULL) != 0)
            return -1; // not found
        if (e.fileType != FT_DIR)
            return -1; // not a directory
        currentBlock = e.startBlock;
        token = next;
    }
    if (lastName[0] == '\0')
    {
        // path ended with '/' or is root; use empty name
        strncpy(outName, "", outNameSize);
    }
    else
    {
        strncpy(outName, lastName, outNameSize - 1);
        outName[outNameSize - 1] = '\0';
    }
    *outDirBlock = currentBlock;
    return 0;
}

// format file system
int fs_format(uint64_t totalBlocks, uint64_t blockSize)
{
    printf("Formatting file system...\n");

    // initialize superblock
    g_superBlock.magic = FS_MAGIC;
    g_superBlock.version = FS_VERSION;
    g_superBlock.totalBlocks = totalBlocks;
    g_superBlock.blockSize = blockSize;
    // layout: [0]=superblock, [1]=root dir, [2..fatEnd]=File Allocation Table
    g_superBlock.rootDirBlock = 1; // root directory at block 1
    // compute FAT blocks to cover all blocks
    uint64_t fatEntriesNeeded = totalBlocks; // one FAT entry per block
    uint64_t fatBlocks = (fatEntriesNeeded * FAT_ENTRY_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
    g_superBlock.fatStart = 2;
    g_superBlock.fatBlocks = fatBlocks;
    // free blocks exclude: superblock, root dir, FAT region
    uint64_t reserved = 1 /*SB*/ + 1 /*root*/ + fatBlocks;
    g_superBlock.freeBlocks = (totalBlocks > reserved) ? (totalBlocks - reserved) : 0;
    strcpy(g_superBlock.volumeName, "CSC415-FS");
    g_superBlock.createTime = time(NULL);
    g_superBlock.lastMountTime = time(NULL);

    // write superblock to disk
    uint64_t result = LBAwrite(&g_superBlock, 1, 0);
    if (result != 1)
    {
        printf("Failed to write superblock\n");
        return -1;
    }

    // create root directory with "." and ".." entries
    DirBlock rootDir;
    rootDir.entryCount = 2; // "." and ".." entries
    rootDir.nextDirBlock = 0;
    memset(rootDir.entries, 0, sizeof(rootDir.entries));

    // Initialize "." entry (self-reference)
    strcpy(rootDir.entries[0].filename, ".");
    rootDir.entries[0].fileType = FT_DIR;
    rootDir.entries[0].startBlock = g_superBlock.rootDirBlock; // points to itself
    rootDir.entries[0].fileSize = 0;
    rootDir.entries[0].createTime = (uint32_t)time(NULL);
    rootDir.entries[0].modifyTime = rootDir.entries[0].createTime;

    // Initialize ".." entry (parent reference - for root, also points to itself)
    strcpy(rootDir.entries[1].filename, "..");
    rootDir.entries[1].fileType = FT_DIR;
    rootDir.entries[1].startBlock = g_superBlock.rootDirBlock; // root's parent is itself
    rootDir.entries[1].fileSize = 0;
    rootDir.entries[1].createTime = (uint32_t)time(NULL);
    rootDir.entries[1].modifyTime = rootDir.entries[1].createTime;

    result = LBAwrite(&rootDir, 1, g_superBlock.rootDirBlock);
    if (result != 1)
    {
        printf("Failed to write root directory\n");
        return -1;
    }

    // initialize FAT: mark reserved blocks as used, others as free
    {
        uint32_t *fat = malloc((size_t)(fatBlocks * BLOCK_SIZE));
        if (!fat)
        {
            printf("Failed to alloc FAT buffer\n");
            return -1;
        }
        // initialize all entries to FAT_FREE first
        for (uint64_t b = 0; b < totalBlocks; b++)
        {
            fat[b] = FAT_FREE;
        }
        // mark reserved blocks as used (FAT_RESERVED)
        for (uint64_t b = 0; b < reserved; b++)
        {
            fat[b] = FAT_RESERVED;
        }
        // write FAT blocks
        if (LBAwrite(fat, fatBlocks, g_superBlock.fatStart) != fatBlocks)
        {
            printf("Failed to write FAT\n");
            free(fat);
            return -1;
        }
        free(fat);
    }

    printf("File system formatted successfully\n");
    return 0;
}

// mount file system
int fs_mount(void)
{
    printf("Mounting file system...\n");

    // read superblock
    uint64_t result = LBAread(&g_superBlock, 1, 0);
    if (result != 1)
    {
        printf("Failed to read superblock\n");
        return -1;
    }

    // verify magic number
    if (g_superBlock.magic != FS_MAGIC)
    {
        printf("Invalid file system magic number\n");
        return -1;
    }

    // update mount time
    g_superBlock.lastMountTime = time(NULL);
    LBAwrite(&g_superBlock, 1, 0);

    printf("File system mounted successfully\n");
    return 0;
}

// unmount file system
int fs_unmount(void)
{
    printf("Unmounting file system...\n");

    // update superblock
    g_superBlock.lastMountTime = time(NULL);
    LBAwrite(&g_superBlock, 1, 0);

    printf("File system unmounted successfully\n");
    return 0;
}

int fs_rename(const char *srcPath, const char *dstPath)
{
    if (!srcPath || !dstPath)
        return -1;
    // locate src
    uint32_t sDir = 0;
    char sName[MAX_FILENAME_LEN + 1];
    if (fs_resolvePath(srcPath, &sDir, sName, sizeof(sName)) != 0)
        return -1;
    if (sName[0] == '\0')
        return -1;
    DirEntry e;
    uint32_t sIdx = 0;
    if (fs_findInDir(sDir, sName, &e, &sIdx) != 0)
        return -1;
    // locate dst parent + name
    uint32_t dDir = 0;
    char dName[MAX_FILENAME_LEN + 1];
    if (fs_resolvePath(dstPath, &dDir, dName, sizeof(dName)) != 0)
        return -1;
    if (dName[0] == '\0')
        return -1;
    // if target exists -> for simplicity, fail
    DirEntry tmp;
    if (fs_findInDir(dDir, dName, &tmp, NULL) == 0)
        return -1;
    // remove from src dir
    if (fs_removeEntryFromDir(sDir, sIdx) != 0)
        return -1;
    // add to dst dir with new name
    strncpy(e.filename, dName, sizeof(e.filename) - 1);
    e.filename[sizeof(e.filename) - 1] = '\0';
    if (fs_addEntryToDir(dDir, &e) != 0)
        return -1;
    return 0;
}

// allocate a free block using FAT (optimized to read only necessary blocks)
uint64_t fs_allocateBlock(void)
{
    // scan FAT block by block to find a free entry
    uint32_t fatBuffer[FAT_ENTRIES_PER_BLOCK];

    for (uint64_t fatBlock = 0; fatBlock < g_superBlock.fatBlocks; fatBlock++)
    {
        // read one FAT block
        if (LBAread(fatBuffer, 1, g_superBlock.fatStart + fatBlock) != 1)
        {
            printf("Failed to read FAT block %llu\n", (unsigned long long)fatBlock);
            return 0;
        }

        // scan entries in this FAT block
        for (uint32_t i = 0; i < FAT_ENTRIES_PER_BLOCK; i++)
        {
            uint64_t blockNumber = fatBlock * FAT_ENTRIES_PER_BLOCK + i;

            // check if we've scanned all blocks
            if (blockNumber >= g_superBlock.totalBlocks)
                break;

            if (fatBuffer[i] == FAT_FREE)
            {
                // found a free block, mark it as EOF and write back
                fatBuffer[i] = FAT_EOF;
                if (LBAwrite(fatBuffer, 1, g_superBlock.fatStart + fatBlock) != 1)
                {
                    printf("Failed to write FAT block %llu\n", (unsigned long long)fatBlock);
                    return 0;
                }
                g_superBlock.freeBlocks--;
                LBAwrite(&g_superBlock, 1, 0);
                return blockNumber;
            }
        }
    }

    printf("No free blocks available\n");
    return 0;
}

// free a block using FAT (optimized to read only necessary block)
int fs_freeBlock(uint64_t blockNumber)
{
    if (blockNumber >= g_superBlock.totalBlocks)
    {
        printf("Invalid block number: %llu\n", (unsigned long long)blockNumber);
        return -1;
    }

    // calculate which FAT block contains this entry
    uint64_t fatBlock = blockNumber / FAT_ENTRIES_PER_BLOCK;
    uint32_t fatIndex = blockNumber % FAT_ENTRIES_PER_BLOCK;

    // read the specific FAT block
    uint32_t fatBuffer[FAT_ENTRIES_PER_BLOCK];
    if (LBAread(fatBuffer, 1, g_superBlock.fatStart + fatBlock) != 1)
    {
        printf("Failed to read FAT block %llu\n", (unsigned long long)fatBlock);
        return -1;
    }

    // mark the block as free
    fatBuffer[fatIndex] = FAT_FREE;

    // write the updated FAT block back
    if (LBAwrite(fatBuffer, 1, g_superBlock.fatStart + fatBlock) != 1)
    {
        printf("Failed to write FAT block %llu\n", (unsigned long long)fatBlock);
        return -1;
    }

    g_superBlock.freeBlocks++;
    LBAwrite(&g_superBlock, 1, 0);
    return 0;
}

// find file
int fs_findFile(const char *path, DirEntry *entry)
{
    if (path == NULL || entry == NULL)
        return -1;
    // resolve parent dir + name
    uint32_t dirBlock = 0;
    char name[MAX_FILENAME_LEN + 1];
    if (fs_resolvePath(path, &dirBlock, name, sizeof(name)) != 0)
        return -1;
    if (name[0] == '\0')
        return -1; // root itself has no DirEntry by name
    return fs_findInDir(dirBlock, name, entry, NULL);
}

// create file
int fs_createFile(const char *path, uint32_t fileType)
{
    if (path == NULL)
        return -1;
    // parent dir + name
    uint32_t dirBlock = 0;
    char name[MAX_FILENAME_LEN + 1];
    if (fs_resolvePath(path, &dirBlock, name, sizeof(name)) != 0)
        return -1;
    if (name[0] == '\0')
        return -1;
    // already exists?
    DirEntry tmp;
    if (fs_findInDir(dirBlock, name, &tmp, NULL) == 0)
        return -1;
    // prepare entry
    DirEntry newEntry;
    memset(&newEntry, 0, sizeof(newEntry));
    strncpy(newEntry.filename, name, sizeof(newEntry.filename) - 1);
    newEntry.fileType = fileType;
    newEntry.fileSize = 0;
    newEntry.createTime = (uint32_t)time(NULL);
    newEntry.modifyTime = newEntry.createTime;
    if (fileType == FT_DIR)
    {
        newEntry.startBlock = fs_allocateBlock();
        if (newEntry.startBlock == 0)
            return -1;
        DirBlock nd;
        nd.entryCount = 0;
        nd.nextDirBlock = 0;
        memset(nd.entries, 0, sizeof(nd.entries));
        if (LBAwrite(&nd, 1, newEntry.startBlock) != 1)
            return -1;
    }
    else if (fileType == FT_FILE)
    {
        // allocate header and initialize
        newEntry.startBlock = fs_allocateBlock();
        if (newEntry.startBlock == 0)
            return -1;
        FileHeader fh;
        memset(&fh, 0, sizeof(fh));
        fh.magic = FILEHEADER_MAGIC;
        fh.fileSize = 0;
        fh.dataBlockCount = 0;
        if (LBAwrite(&fh, 1, newEntry.startBlock) != 1)
            return -1;
    }
    // add to dir (with expansion if needed)
    if (fs_addEntryToDir(dirBlock, &newEntry) != 0)
        return -1;
    return 0;
}

// delete file
int fs_deleteFile(const char *path)
{
    if (path == NULL)
        return -1;
    uint32_t dirBlock = 0;
    char name[MAX_FILENAME_LEN + 1];
    if (fs_resolvePath(path, &dirBlock, name, sizeof(name)) != 0)
        return -1;
    DirEntry e;
    uint32_t idx = 0;
    if (fs_findInDir(dirBlock, name, &e, &idx) != 0)
        return -1;
    if (e.fileType == FT_DIR)
    {
        if (!fs_isDirectoryEmpty(e.startBlock))
            return -1;
    }
    else if (e.fileType == FT_FILE)
    {
        // free header and data blocks
        FileHeader fh;
        if (LBAread(&fh, 1, e.startBlock) != 1)
            return -1;
        if (fh.magic == FILEHEADER_MAGIC)
        {
            for (uint32_t i = 0; i < fh.dataBlockCount; i++)
                fs_freeBlock(fh.dataBlocks[i]);
        }
    }
    if (e.startBlock)
        fs_freeBlock(e.startBlock);
    if (fs_removeEntryFromDir(dirBlock, idx) != 0)
        return -1;
    return 0;
}

// dir helpers
static int fs_expandDirectoryIfNeeded(uint32_t dirBlock, DirBlock *dir, uint32_t *outUseBlock)
{
    if (dir->entryCount < MAX_DIR_ENTRIES)
    {
        if (outUseBlock)
            *outUseBlock = dirBlock;
        return 0;
    }
    if (dir->nextDirBlock == 0)
    {
        uint64_t nb = fs_allocateBlock();
        if (nb == 0)
            return -1;
        DirBlock nd;
        nd.entryCount = 0;
        nd.nextDirBlock = 0;
        memset(nd.entries, 0, sizeof(nd.entries));
        if (LBAwrite(&nd, 1, nb) != 1)
            return -1;
        dir->nextDirBlock = (uint32_t)nb;
        if (fs_storeDir(dirBlock, dir) != 0)
            return -1;
        if (outUseBlock)
            *outUseBlock = (uint32_t)nb;
        return 0;
    }
    if (outUseBlock)
        *outUseBlock = dir->nextDirBlock;
    return 0;
}

static int fs_addEntryToDir(uint32_t dirBlock, const DirEntry *newEntry)
{
    uint32_t targetBlock = dirBlock;
    DirBlock cur;
    while (1)
    {
        if (fs_loadDir(targetBlock, &cur) != 0)
            return -1;
        if (cur.entryCount < MAX_DIR_ENTRIES)
        {
            cur.entries[cur.entryCount] = *newEntry;
            cur.entryCount++;
            return fs_storeDir(targetBlock, &cur);
        }
        if (cur.nextDirBlock == 0)
        {
            // expand
            uint32_t useBlock = 0;
            if (fs_expandDirectoryIfNeeded(targetBlock, &cur, &useBlock) != 0)
                return -1;
            if (useBlock == targetBlock)
                continue; // retry same block
            targetBlock = useBlock;
        }
        else
        {
            targetBlock = cur.nextDirBlock;
        }
    }
}

static int fs_removeEntryFromDir(uint32_t dirBlock, uint32_t index)
{
    DirBlock cur;
    uint32_t curBlock = dirBlock;
    while (curBlock)
    {
        if (fs_loadDir(curBlock, &cur) != 0)
            return -1;
        if (index < cur.entryCount)
        {
            for (uint32_t i = index; i + 1 < cur.entryCount; i++)
                cur.entries[i] = cur.entries[i + 1];
            cur.entryCount--;
            return fs_storeDir(curBlock, &cur);
        }
        index -= cur.entryCount;
        curBlock = cur.nextDirBlock;
    }
    return -1;
}

static int fs_isDirectoryEmpty(uint32_t dirBlock)
{
    DirBlock cur;
    uint32_t curBlock = dirBlock;
    while (curBlock)
    {
        if (fs_loadDir(curBlock, &cur) != 0)
            return 0;
        if (cur.entryCount != 0)
            return 0;
        curBlock = cur.nextDirBlock;
    }
    return 1;
}
