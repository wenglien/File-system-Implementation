/**************************************************************
 * Class::  CSC-415-01 Fall 2025
 * Name:: Ian Wang
 * Student IDs:: 924005755
 * GitHub-Name:: IannnWENG
 * Group-Name:: BobaTea
 * Project:: Basic File System
 *
 * File:: test_direntry.c
 *
 * Description:: Test program for DirEntry structure
 *
 **************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

// File type definitions
#define FT_UNUSED 0
#define FT_FILE 1
#define FT_DIR 2

// Directory entry structure
typedef struct
{
    char filename[64];   // filename (max 64 characters)
    uint32_t fileType;   // file type (FT_FILE, FT_DIR)
    uint32_t startBlock; // starting block position
    uint32_t fileSize;   // file size in bytes
    uint32_t createTime; // creation timestamp
    uint32_t modifyTime; // modification timestamp
} DirEntry;

// Function to initialize a directory entry
void initializeDirEntry(DirEntry *entry, const char *name, uint32_t type)
{
    if (entry == NULL)
        return;

    // Clear the structure
    memset(entry, 0, sizeof(DirEntry));

    // Set filename
    strncpy(entry->filename, name, sizeof(entry->filename) - 1);
    entry->filename[sizeof(entry->filename) - 1] = '\0';

    // Set file type
    entry->fileType = type;

    // Set timestamps
    entry->createTime = (uint32_t)time(NULL);
    entry->modifyTime = entry->createTime;

    // Initialize other fields
    entry->startBlock = 0;
    entry->fileSize = 0;
}

// Function to print directory entry information
void printDirEntry(const DirEntry *entry)
{
    if (entry == NULL)
        return;

    printf("=== Directory Entry ===\n");
    printf("Filename: %s\n", entry->filename);
    printf("File Type: %s\n",
           entry->fileType == FT_FILE ? "FILE" : entry->fileType == FT_DIR ? "DIRECTORY"
                                                                           : "UNUSED");
    printf("Start Block: %u\n", entry->startBlock);
    printf("File Size: %u bytes\n", entry->fileSize);
    printf("Create Time: %u\n", entry->createTime);
    printf("Modify Time: %u\n", entry->modifyTime);
    printf("======================\n\n");
}

// Function to test directory entry operations
int testDirEntry()
{
    printf("Testing DirEntry structure...\n\n");

    // Test 1: Create a file entry
    printf("Test 1: Creating a file entry\n");
    DirEntry fileEntry;
    initializeDirEntry(&fileEntry, "test.txt", FT_FILE);
    fileEntry.startBlock = 5;
    fileEntry.fileSize = 1024;
    printDirEntry(&fileEntry);

    // Test 2: Create a directory entry
    printf("Test 2: Creating a directory entry\n");
    DirEntry dirEntry;
    initializeDirEntry(&dirEntry, "documents", FT_DIR);
    dirEntry.startBlock = 10;
    printDirEntry(&dirEntry);

    // Test 3: Create multiple entries
    printf("Test 3: Creating multiple entries\n");
    DirEntry entries[3];
    const char *names[] = {"file1.txt", "file2.txt", "subdir"};
    uint32_t types[] = {FT_FILE, FT_FILE, FT_DIR};

    for (int i = 0; i < 3; i++)
    {
        initializeDirEntry(&entries[i], names[i], types[i]);
        entries[i].startBlock = 20 + i;
        entries[i].fileSize = 512 * (i + 1);
        printDirEntry(&entries[i]);
    }

    // Test 4: Test structure size
    printf("Test 4: Structure size information\n");
    printf("Size of DirEntry: %zu bytes\n", sizeof(DirEntry));
    printf("Size of filename field: %zu bytes\n", sizeof(entries[0].filename));
    printf("Size of fileType field: %zu bytes\n", sizeof(entries[0].fileType));
    printf("Size of startBlock field: %zu bytes\n", sizeof(entries[0].startBlock));
    printf("Size of fileSize field: %zu bytes\n", sizeof(entries[0].fileSize));
    printf("Size of createTime field: %zu bytes\n", sizeof(entries[0].createTime));
    printf("Size of modifyTime field: %zu bytes\n", sizeof(entries[0].modifyTime));
    printf("\n");

    // Test 5: Test filename truncation
    printf("Test 5: Testing filename truncation\n");
    DirEntry longNameEntry;
    char longName[100];
    memset(longName, 'A', 99);
    longName[99] = '\0';

    initializeDirEntry(&longNameEntry, longName, FT_FILE);
    printf("Original name length: %zu\n", strlen(longName));
    printf("Stored name: %s\n", longNameEntry.filename);
    printf("Stored name length: %zu\n", strlen(longNameEntry.filename));
    printf("\n");

    printf("All tests completed successfully!\n");
    return 0;
}

int main()
{
    printf("DirEntry Structure Test Program\n");
    printf("===============================\n\n");

    int result = testDirEntry();

    if (result == 0)
    {
        printf("Test program completed successfully!\n");
    }
    else
    {
        printf("Test program failed!\n");
    }

    return result;
}
