/**************************************************************
 * Class::  CSC-415-01 Fall 2025
 * Name:: Ian Wang
 * Student IDs:: 924005755
 * GitHub-Name:: IannnWENG
 * Group-Name:: BobaTea
 * Project:: Basic File System
 *
 * File:: b_io.c
 *
 * Description:: Basic file I/O operations implementation
 *
 **************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "b_io.h"
#include "fsStruct.h"
#include "fsLow.h"

#define MAXFCBS 20
#define B_CHUNK_SIZE 512

typedef struct b_fcb
{
	/** TODO: Add all the information you need in the file control block **/
	char *buf;
	int index;
	int buflen;
} b_fcb;

b_fcb fcbArray[MAXFCBS];

int startup = 0;

// Method to initialize our file system
void b_init()
{
	// initialize fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
	{
		fcbArray[i].buf = NULL;
	}

	startup = 1;
}

// Method to get a free FCB element
b_io_fd b_getFCB()
{
	for (int i = 0; i < MAXFCBS; i++)
	{
		if (fcbArray[i].buf == NULL)
		{
			return i;
		}
	}
	return (-1);
}

// Interface to open a buffered file
// Modification of interface for this assignment, flags match the Linux flags for open
// O_RDONLY, O_WRONLY, or O_RDWR
b_io_fd b_open(char *filename, int flags)
{
	b_io_fd returnFd;

	if (startup == 0)
		b_init();

	returnFd = b_getFCB();
	if (returnFd == -1)
	{
		printf("No free file descriptors available\n");
		return -1;
	}

	// check if file exists
	DirEntry entry;
	int fileExists = (fs_findFile(filename, &entry) == 0);

	// handle different open modes
	if (!fileExists)
	{
		if (flags & O_CREAT)
		{
			// create new file
			if (fs_createFile(filename, FT_FILE) != 0)
			{
				printf("Failed to create file: %s\n", filename);
				return -1;
			}
		}
		else
		{
			printf("File does not exist: %s\n", filename);
			return -1;
		}
	}

	// re-get file info (whether newly created or existing)
	if (fs_findFile(filename, &entry) != 0)
	{
		printf("Failed to get file info\n");
		return -1;
	}

	// if in read mode, ensure file size is up-to-date
	if (flags == O_RDONLY || (flags & O_RDONLY))
	{
		// re-read directory entry to get latest file size
		DirBlock rootDir;
		uint64_t result = LBAread(&rootDir, 1, g_superBlock.rootDirBlock);
		if (result == 1)
		{
			for (uint32_t i = 0; i < rootDir.entryCount; i++)
			{
				if (strcmp(rootDir.entries[i].filename, filename) == 0)
				{
					entry = rootDir.entries[i];
					break;
				}
			}
		}
	}

	// set file control block
	g_fcbArray[returnFd].inUse = 1;
	strncpy(g_fcbArray[returnFd].filename, filename, MAX_FILENAME_LEN);
	g_fcbArray[returnFd].filename[MAX_FILENAME_LEN] = '\0';
	g_fcbArray[returnFd].currentPos = 0;
	g_fcbArray[returnFd].fileSize = entry.fileSize;
	g_fcbArray[returnFd].startBlock = entry.startBlock;
	g_fcbArray[returnFd].flags = flags;
	g_fcbArray[returnFd].lastAccess = time(NULL);

	// handle O_APPEND flag
	if (flags & O_APPEND)
	{
		g_fcbArray[returnFd].currentPos = entry.fileSize;
	}

	// handle O_TRUNC flag
	if (flags & O_TRUNC)
	{
		g_fcbArray[returnFd].fileSize = 0;
		// free all blocks
		if (entry.startBlock != 0)
		{
			fs_freeBlock(entry.startBlock);
		}
		g_fcbArray[returnFd].startBlock = 0;
	}

	// allocate buffer
	fcbArray[returnFd].buf = malloc(B_CHUNK_SIZE);
	if (fcbArray[returnFd].buf == NULL)
	{
		printf("Failed to allocate buffer\n");
		g_fcbArray[returnFd].inUse = 0;
		return -1;
	}
	fcbArray[returnFd].index = 0;
	fcbArray[returnFd].buflen = 0;

	return returnFd;
}

// Interface to seek function
int b_seek(b_io_fd fd, off_t offset, int whence)
{
	if (startup == 0)
		b_init();

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
	{
		return (-1); // invalid file descriptor
	}

	if (!g_fcbArray[fd].inUse)
	{
		printf("File descriptor not in use: %d\n", fd);
		return -1;
	}

	// calculate new position
	off_t newPos;
	switch (whence)
	{
	case SEEK_SET:
		newPos = offset;
		break;
	case SEEK_CUR:
		newPos = g_fcbArray[fd].currentPos + offset;
		break;
	case SEEK_END:
		newPos = g_fcbArray[fd].fileSize + offset;
		break;
	default:
		printf("Invalid whence value: %d\n", whence);
		return -1;
	}

	// check if position is valid
	if (newPos < 0)
	{
		printf("Invalid seek position: %lld\n", (long long)newPos);
		return -1;
	}

	// update position
	g_fcbArray[fd].currentPos = newPos;

	return 0;
}

// Interface to write function
int b_write(b_io_fd fd, char *buffer, int count)
{
	if (startup == 0)
		b_init();

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
	{
		return (-1); // invalid file descriptor
	}

	if (!g_fcbArray[fd].inUse)
	{
		printf("File descriptor not in use: %d\n", fd);
		return -1;
	}

	// check write permissions
	if (!(g_fcbArray[fd].flags & O_RDWR) && !(g_fcbArray[fd].flags & O_WRONLY))
	{
		printf("File not opened for writing: %d\n", fd);
		return -1;
	}

	// multi-block implementation via FileHeader
	// ensure header exists
	if (g_fcbArray[fd].startBlock == 0)
	{
		g_fcbArray[fd].startBlock = fs_allocateBlock();
		if (g_fcbArray[fd].startBlock == 0)
		{
			printf("Failed to allocate header block\n");
			return -1;
		}
		FileHeader newHeader;
		memset(&newHeader, 0, sizeof(newHeader));
		newHeader.magic = FILEHEADER_MAGIC;
		newHeader.fileSize = 0;
		newHeader.dataBlockCount = 0;
		if (LBAwrite(&newHeader, 1, g_fcbArray[fd].startBlock) != 1)
			return -1;
	}
	FileHeader header;
	if (LBAread(&header, 1, g_fcbArray[fd].startBlock) != 1 || header.magic != FILEHEADER_MAGIC)
	{
		printf("Invalid file header\n");
		return -1;
	}
	uint64_t remaining = (uint64_t)count;
	char *src = buffer;
	while (remaining > 0)
	{
		uint64_t filePos = g_fcbArray[fd].currentPos;
		uint64_t blockIndex = filePos / BLOCK_SIZE;
		uint64_t within = filePos % BLOCK_SIZE;
		// ensure data block exists
		if (blockIndex >= header.dataBlockCount)
		{
			if (header.dataBlockCount >= MAX_FILE_BLOCKS)
			{
				printf("File too large\n");
				break;
			}
			uint64_t nb = fs_allocateBlock();
			if (nb == 0)
			{
				printf("b_write: Failed to allocate block\n");
				break;
			}
			header.dataBlocks[header.dataBlockCount++] = (uint32_t)nb;
		}
		uint32_t dataLBA = header.dataBlocks[blockIndex];
		char blk[BLOCK_SIZE];
		if (LBAread(blk, 1, dataLBA) != 1)
			memset(blk, 0, sizeof(blk));
		uint64_t can = BLOCK_SIZE - within;
		if (can > remaining)
			can = remaining;
		memcpy(blk + within, src, (size_t)can);
		if (LBAwrite(blk, 1, dataLBA) != 1)
		{
			printf("Failed to write data block\n");
			break;
		}
		g_fcbArray[fd].currentPos += (off_t)can;
		if (g_fcbArray[fd].currentPos > (off_t)header.fileSize)
			header.fileSize = g_fcbArray[fd].currentPos;
		src += can;
		remaining -= can;
	}
	// persist header and update fcb
	if (LBAwrite(&header, 1, g_fcbArray[fd].startBlock) != 1)
		return -1;
	g_fcbArray[fd].fileSize = header.fileSize;
	return (int)(count - remaining);
}

// Interface to read a buffer

// Filling the callers request is broken into three parts
// Part 1 is what can be filled from the current buffer, which may or may not be enough
// Part 2 is after using what was left in our buffer there is still 1 or more block
//        size chunks needed to fill the callers request.  This represents the number of
//        bytes in multiples of the blocksize.
// Part 3 is a value less than blocksize which is what remains to copy to the callers buffer
//        after fulfilling part 1 and part 2.  This would always be filled from a refill
//        of our buffer.
//  +-------------+------------------------------------------------+--------+
//  |             |                                                |        |
//  | filled from |  filled direct in multiples of the block size  | filled |
//  | existing    |                                                | from   |
//  | buffer      |                                                |refilled|
//  |             |                                                | buffer |
//  |             |                                                |        |
//  | Part1       |  Part 2                                        | Part3  |
//  +-------------+------------------------------------------------+--------+
int b_read(b_io_fd fd, char *buffer, int count)
{
	if (startup == 0)
		b_init();

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
	{
		return (-1); // invalid file descriptor
	}

	if (!g_fcbArray[fd].inUse)
	{
		printf("File descriptor not in use: %d\n", fd);
		return -1;
	}

	// check read permissions
	if (!(g_fcbArray[fd].flags & O_RDWR) && (g_fcbArray[fd].flags & O_WRONLY))
	{
		printf("File not opened for reading: %d\n", fd);
		return -1;
	}

	// check if reached end of file
	if (g_fcbArray[fd].currentPos >= g_fcbArray[fd].fileSize)
	{
		return 0; // EOF
	}

	// calculate actual readable bytes
	int bytesToRead = count;
	if (g_fcbArray[fd].currentPos + count > g_fcbArray[fd].fileSize)
	{
		bytesToRead = g_fcbArray[fd].fileSize - g_fcbArray[fd].currentPos;
	}

	// if file has no blocks, return 0
	if (g_fcbArray[fd].startBlock == 0)
	{
		return 0;
	}

	// calculate blocks needed for reading
	uint64_t startBlock = g_fcbArray[fd].startBlock;
	uint64_t blockOffset = g_fcbArray[fd].currentPos / BLOCK_SIZE;
	uint64_t byteOffset = g_fcbArray[fd].currentPos % BLOCK_SIZE;

	// multi-block read via FileHeader
	FileHeader header;
	if (LBAread(&header, 1, g_fcbArray[fd].startBlock) != 1 || header.magic != FILEHEADER_MAGIC)
		return -1;
	int totalRead = 0;
	int remaining = bytesToRead;
	char *dst = buffer;
	while (remaining > 0 && g_fcbArray[fd].currentPos < (off_t)header.fileSize)
	{
		uint64_t blockIndex = ((uint64_t)g_fcbArray[fd].currentPos) / BLOCK_SIZE;
		uint64_t within = ((uint64_t)g_fcbArray[fd].currentPos) % BLOCK_SIZE;
		if (blockIndex >= header.dataBlockCount)
			break;
		uint32_t dataLBA = header.dataBlocks[blockIndex];
		char blk[BLOCK_SIZE];
		if (LBAread(blk, 1, dataLBA) != 1)
			return -1;
		uint64_t can = BLOCK_SIZE - within;
		uint64_t leftInFile = header.fileSize - (uint64_t)g_fcbArray[fd].currentPos;
		if (can > (uint64_t)remaining)
			can = (uint64_t)remaining;
		if (can > leftInFile)
			can = leftInFile;
		memcpy(dst, blk + within, (size_t)can);
		g_fcbArray[fd].currentPos += (off_t)can;
		dst += can;
		remaining -= (int)can;
		totalRead += (int)can;
	}
	return totalRead;
}

// Interface to Close the file
int b_close(b_io_fd fd)
{
	if (startup == 0)
		b_init();

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
	{
		return (-1); // invalid file descriptor
	}

	if (!g_fcbArray[fd].inUse)
	{
		printf("File descriptor not in use: %d\n", fd);
		return -1;
	}

	// update file size and startBlock in directory entry (persist header already done in write)
	DirEntry entry;
	uint32_t dirBlock = 0;
	char name[MAX_FILENAME_LEN + 1];
	if (fs_resolvePath(g_fcbArray[fd].filename, &dirBlock, name, sizeof(name)) == 0)
	{
		DirBlock cur;
		uint32_t curBlock = dirBlock;
		while (curBlock)
		{
			if (LBAread(&cur, 1, curBlock) != 1)
				break;
			for (uint32_t i = 0; i < cur.entryCount; i++)
			{
				if (strcmp(cur.entries[i].filename, name) == 0)
				{
					cur.entries[i].fileSize = (uint32_t)g_fcbArray[fd].fileSize;
					cur.entries[i].startBlock = (uint32_t)g_fcbArray[fd].startBlock;
					cur.entries[i].modifyTime = (uint32_t)time(NULL);
					LBAwrite(&cur, 1, curBlock);
					curBlock = 0; // done
					break;
				}
			}
			if (curBlock)
				curBlock = cur.nextDirBlock;
		}
	}

	// free buffer
	if (fcbArray[fd].buf != NULL)
	{
		free(fcbArray[fd].buf);
		fcbArray[fd].buf = NULL;
	}

	// mark as unused
	g_fcbArray[fd].inUse = 0;

	return 0;
}
