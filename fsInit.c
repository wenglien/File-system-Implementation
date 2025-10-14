/**************************************************************
* Class::  CSC-415-01 Fall 2025
* Name:: Ian Wang
* Student IDs:: 924005755
* GitHub-Name:: IannnWENG
* Group-Name:: BobaTea
* Project:: Basic File System
*
* File:: fsInit.c
*
* Description:: File system initialization and exit functions
*
**************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "fsLow.h"
#include "mfs.h"
#include "fsStruct.h"

// global variable definitions
SuperBlock g_superBlock;
FileControlBlock g_fcbArray[MAX_OPEN_FILES];
char g_currentPath[MAX_PATH_LEN] = "/";

int initFileSystem (uint64_t numberOfBlocks, uint64_t blockSize)
	{
    printf ("Initializing File System with %llu blocks with a block size of %llu\n", (unsigned long long)numberOfBlocks, (unsigned long long)blockSize);
	
	// initialize file control block array
	for (int i = 0; i < MAX_OPEN_FILES; i++) {
		g_fcbArray[i].inUse = 0;
		g_fcbArray[i].filename[0] = '\0';
		g_fcbArray[i].currentPos = 0;
		g_fcbArray[i].fileSize = 0;
		g_fcbArray[i].startBlock = 0;
		g_fcbArray[i].flags = 0;
		g_fcbArray[i].lastAccess = 0;
	}
	
	// format file system
	int result = fs_format(numberOfBlocks, blockSize);
	if (result != 0) {
		printf("Failed to format file system\n");
		return result;
	}
	
	// mount file system
	result = fs_mount();
	if (result != 0) {
		printf("Failed to mount file system\n");
		return result;
	}
	
	printf("File system initialized successfully\n");
	return 0;
	}
	
	
void exitFileSystem ()
	{
	printf ("System exiting\n");
	
	// close all open files
	for (int i = 0; i < MAX_OPEN_FILES; i++) {
		if (g_fcbArray[i].inUse) {
			// close file
			g_fcbArray[i].inUse = 0;
		}
	}
	
	// unmount file system
	fs_unmount();
	}