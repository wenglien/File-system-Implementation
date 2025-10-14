/**************************************************************
* Class::  CSC-415-01 Fall 2025
* Name:: Ian Wang
* Student IDs:: 924005755
* GitHub-Name:: IannnWENG
* Group-Name:: BobaTea
* Project:: Basic File System
*
* File:: fsDir.c
*
* Description:: Directory functionality implementation
*
**************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fsLow.h"
#include "mfs.h"
#include "fsStruct.h"

// create directory
int fs_mkdir(const char *pathname, mode_t mode) {
    if (pathname == NULL) {
        return -1;
    }
    
    printf("Creating directory: %s\n", pathname);
    
    // create directory
    int result = fs_createFile(pathname, FT_DIR);
    if (result != 0) {
        printf("Failed to create directory: %s\n", pathname);
        return -1;
    }
    
    return 0;
}

// delete directory
int fs_rmdir(const char *pathname) {
    if (pathname == NULL) {
        return -1;
    }
    
    printf("Removing directory: %s\n", pathname);
    
    // check if it's a directory
    DirEntry entry;
    if (fs_findFile(pathname, &entry) != 0) {
        printf("Directory not found: %s\n", pathname);
        return -1;
    }
    
    if (entry.fileType != FT_DIR) {
        printf("Not a directory: %s\n", pathname);
        return -1;
    }
    
    // simplified implementation: assume directory is empty
    // actual implementation needs to check if directory contains files
    
    // delete directory
    int result = fs_deleteFile(pathname);
    if (result != 0) {
        printf("Failed to delete directory: %s\n", pathname);
        return -1;
    }
    
    return 0;
}

// open directory
fdDir * fs_opendir(const char *pathname) {
    if (pathname == NULL) {
        return NULL;
    }
    
    printf("Opening directory: %s\n", pathname);
    
    // resolve dir block
    uint32_t dirBlock = 0; char name[MAX_FILENAME_LEN+1];
    if (fs_resolvePath(pathname, &dirBlock, name, sizeof(name)) != 0) return NULL;
    if (name[0] != '\0') {
        DirEntry e; if (fs_findInDir(dirBlock, name, &e, NULL) != 0) return NULL; if (e.fileType != FT_DIR) return NULL; dirBlock = e.startBlock;
    }
    
    // allocate directory descriptor
    fdDir *dirp = malloc(sizeof(fdDir));
    if (dirp == NULL) return NULL;
    dirp->d_reclen = sizeof(struct fs_diriteminfo);
    dirp->dirEntryPosition = 0;
    dirp->currentDirBlock = dirBlock;
    fs_loadDir(dirp->currentDirBlock, &dirp->cachedDir);
    dirp->di = NULL;
    return dirp;
}

// read directory entry
struct fs_diriteminfo *fs_readdir(fdDir *dirp) {
    if (dirp == NULL) {
        return NULL;
    }
    // if need next block
    while (dirp->dirEntryPosition >= dirp->cachedDir.entryCount) {
        if (dirp->cachedDir.nextDirBlock == 0) return NULL;
        dirp->currentDirBlock = dirp->cachedDir.nextDirBlock;
        if (fs_loadDir(dirp->currentDirBlock, &dirp->cachedDir) != 0) return NULL;
        dirp->dirEntryPosition = 0;
    }
    
    // allocate directory entry info structure
    if (dirp->di == NULL) {
        dirp->di = malloc(sizeof(struct fs_diriteminfo));
        if (dirp->di == NULL) {
            printf("Failed to allocate memory for directory item info\n");
            return NULL;
        }
    }
    
    // get directory entry
    DirEntry *entry = &dirp->cachedDir.entries[dirp->dirEntryPosition];
    
    // fill directory entry info
    dirp->di->d_reclen = sizeof(struct fs_diriteminfo);
    dirp->di->fileType = (entry->fileType == FT_DIR) ? FT_DIRECTORY : FT_REGFILE;
    strncpy(dirp->di->d_name, entry->filename, 255);
    dirp->di->d_name[255] = '\0';
    
    // move to next entry
    dirp->dirEntryPosition++;
    
    return dirp->di;
}

// close directory
int fs_closedir(fdDir *dirp) {
    if (dirp == NULL) {
        return -1;
    }
    
    printf("Closing directory\n");
    
    // free memory
    if (dirp->di != NULL) {
        free(dirp->di);
        dirp->di = NULL;
    }
    
    free(dirp);
    
    return 0;
}

// get current working directory
char * fs_getcwd(char *pathname, size_t size) {
    if (pathname == NULL || size == 0) {
        return NULL;
    }
    
    // simplified implementation: always return root directory
    strncpy(pathname, g_currentPath, size - 1);
    pathname[size - 1] = '\0';
    
    return pathname;
}

// set current working directory
int fs_setcwd(char * pathname) {
    if (pathname == NULL) {
        return -1;
    }
    
    printf("Changing directory to: %s\n", pathname);
    
    // simplified implementation: only support root directory
    if (strcmp(pathname, "/") == 0 || strcmp(pathname, ".") == 0) {
        strcpy(g_currentPath, "/");
        return 0;
    }
    
    // check if directory exists
    DirEntry entry;
    if (fs_findFile(pathname, &entry) != 0) {
        printf("Directory not found: %s\n", pathname);
        return -1;
    }
    
    if (entry.fileType != FT_DIR) {
        printf("Not a directory: %s\n", pathname);
        return -1;
    }
    
    // update current path
    strncpy(g_currentPath, pathname, MAX_PATH_LEN - 1);
    g_currentPath[MAX_PATH_LEN - 1] = '\0';
    
    return 0;
}

// check if it's a file
int fs_isFile(char * filename) {
    if (filename == NULL) {
        return 0;
    }
    // resolve relative to current path if needed
    char pathbuf[MAX_PATH_LEN];
    if (filename[0] == '/') {
        strncpy(pathbuf, filename, sizeof(pathbuf)-1);
        pathbuf[sizeof(pathbuf)-1] = '\0';
    } else {
        if (strcmp(g_currentPath, "/") == 0)
            snprintf(pathbuf, sizeof(pathbuf), "/%s", filename);
        else
            snprintf(pathbuf, sizeof(pathbuf), "%s/%s", g_currentPath, filename);
    }
    DirEntry entry;
    if (fs_findFile(pathbuf, &entry) != 0) {
        return 0; // file does not exist
    }
    
    return (entry.fileType == FT_FILE) ? 1 : 0;
}

// check if it's a directory
int fs_isDir(char * pathname) {
    if (pathname == NULL) {
        return 0;
    }
    char pathbuf[MAX_PATH_LEN];
    if (pathname[0] == '/') {
        strncpy(pathbuf, pathname, sizeof(pathbuf)-1);
        pathbuf[sizeof(pathbuf)-1] = '\0';
    } else {
        if (strcmp(g_currentPath, "/") == 0)
            snprintf(pathbuf, sizeof(pathbuf), "/%s", pathname);
        else
            snprintf(pathbuf, sizeof(pathbuf), "%s/%s", g_currentPath, pathname);
    }
    DirEntry entry;
    if (fs_findFile(pathbuf, &entry) != 0) {
        return 0; // directory does not exist
    }
    
    return (entry.fileType == FT_DIR) ? 1 : 0;
}

// delete file
int fs_delete(char* filename) {
    if (filename == NULL) {
        return -1;
    }
    
    printf("Deleting file: %s\n", filename);
    
    // check if file exists
    DirEntry entry;
    if (fs_findFile(filename, &entry) != 0) {
        printf("File not found: %s\n", filename);
        return -1;
    }
    
    // delete file
    int result = fs_deleteFile(filename);
    if (result != 0) {
        printf("Failed to delete file: %s\n", filename);
        return -1;
    }
    
    return 0;
}

// get file statistics
int fs_stat(const char *filename, struct fs_stat *buf) {
    if (filename == NULL || buf == NULL) {
        return -1;
    }
    char pathbuf[MAX_PATH_LEN];
    if (filename[0] == '/') {
        strncpy(pathbuf, filename, sizeof(pathbuf)-1);
        pathbuf[sizeof(pathbuf)-1] = '\0';
    } else {
        if (strcmp(g_currentPath, "/") == 0)
            snprintf(pathbuf, sizeof(pathbuf), "/%s", filename);
        else
            snprintf(pathbuf, sizeof(pathbuf), "%s/%s", g_currentPath, filename);
    }
    // find file
    DirEntry entry;
    if (fs_findFile(pathbuf, &entry) != 0) {
        return -1;
    }
    
    // fill statistics info
    uint64_t size = entry.fileSize;
    if (entry.fileType == FT_FILE && entry.startBlock != 0) {
        FileHeader fh; if (LBAread(&fh, 1, entry.startBlock) == 1 && fh.magic == FILEHEADER_MAGIC) {
            size = fh.fileSize;
        }
    }
    buf->st_size = (off_t)size;
    buf->st_blksize = BLOCK_SIZE;
    buf->st_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE; // calculate block count
    buf->st_accesstime = entry.modifyTime; // use modify time as access time
    buf->st_modtime = entry.modifyTime;
    buf->st_createtime = entry.createTime;
    
    return 0;
}
