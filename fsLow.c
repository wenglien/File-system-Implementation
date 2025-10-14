/**************************************************************
* Class::  CSC-415-01 Fall 2025
* Name:: Ian Wang
* Student IDs:: 924005755
* GitHub-Name:: IannnWENG
* Group-Name:: BobaTea
* Project:: Basic File System
*
* File:: fsLow.c
*
* Description:: Simplified low-level file system implementation
*
**************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "fsLow.h"

static int volume_fd = -1;
static uint64_t volume_size = 0;
static uint64_t block_size = 0;

int startPartitionSystem(char *filename, uint64_t *volSize, uint64_t *blockSize) {
    printf("Starting partition system: %s\n", filename);
    
    // open or create file
    volume_fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (volume_fd == -1) {
        printf("Failed to open volume file: %s\n", filename);
        return -1;
    }
    
    // check file size
    struct stat st;
    if (fstat(volume_fd, &st) == -1) {
        printf("Failed to get file stats\n");
        close(volume_fd);
        return -1;
    }
    
    if (st.st_size == 0) {
        // new file, needs initialization
        printf("Creating new volume file\n");
        volume_size = *volSize;
        block_size = *blockSize;
        
        // create file
        if (ftruncate(volume_fd, volume_size * block_size) == -1) {
            printf("Failed to create volume file\n");
            close(volume_fd);
            return -1;
        }
    } else {
        // existing file
        volume_size = st.st_size / *blockSize;
        block_size = *blockSize;
    }
    
    *volSize = volume_size;
    *blockSize = block_size;
    
    printf("Volume size: %llu blocks, Block size: %llu bytes\n", 
           (unsigned long long)volume_size, (unsigned long long)block_size);
    
    return 0;
}

int closePartitionSystem(void) {
    if (volume_fd != -1) {
        close(volume_fd);
        volume_fd = -1;
    }
    return 0;
}

uint64_t LBAwrite(void *buffer, uint64_t lbaCount, uint64_t lbaPosition) {
    if (volume_fd == -1) {
        printf("Volume not opened\n");
        return 0;
    }
    
    if (lbaPosition + lbaCount > volume_size) {
        printf("Write beyond volume size\n");
        return 0;
    }
    
    off_t offset = lbaPosition * block_size;
    if (lseek(volume_fd, offset, SEEK_SET) == -1) {
        printf("Failed to seek to position %llu\n", (unsigned long long)offset);
        return 0;
    }
    
    ssize_t bytes_written = write(volume_fd, buffer, lbaCount * block_size);
    if (bytes_written == -1) {
        printf("Failed to write data\n");
        return 0;
    }
    
    return bytes_written / block_size;
}

uint64_t LBAread(void *buffer, uint64_t lbaCount, uint64_t lbaPosition) {
    if (volume_fd == -1) {
        printf("Volume not opened\n");
        return 0;
    }
    
    if (lbaPosition + lbaCount > volume_size) {
        printf("Read beyond volume size\n");
        return 0;
    }
    
    off_t offset = lbaPosition * block_size;
    if (lseek(volume_fd, offset, SEEK_SET) == -1) {
        printf("Failed to seek to position %llu\n", (unsigned long long)offset);
        return 0;
    }
    
    ssize_t bytes_read = read(volume_fd, buffer, lbaCount * block_size);
    if (bytes_read == -1) {
        printf("Failed to read data\n");
        return 0;
    }
    
    return bytes_read / block_size;
}

void runFSLowTest(void) {
    printf("Running fsLow test\n");
    // simplified test implementation
}
