#ifndef NUFS_STORAGE_H
#define NUFS_STORAGE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "directory.h"

#define STARTING_BLOCKS 12
#define DISK_SIZE 1024 * 1024
#define BLOCK_SIZE 4096
#define INODE_COUNT 2048
#define BLOCK_COUNT DISK_SIZE / BLOCK_SIZE
#define BIG_SIZE BLOCK_SIZE * STARTING_BLOCKS

typedef struct inode {
    mode_t    mode;
    nlink_t   nlink;
    uid_t     uid;
    gid_t     gid;
    dev_t     rdev;
    off_t     size;
    struct timespec atim;
    struct timespec mtim;
    struct timespec ctim;
    int direct;
    int indirect;
} inode;

void storage_init(const char* path);
long get_stat(const char* path, struct stat* st);
long get_stat_inode_id(long inodeId, struct stat* st);
long get_stat_inode(inode* node, struct stat* st);
const char* get_data(const char* path);
inode* get_inode(const char* path);
void get_dirent(const char* path, struct dirent* dirInfo);
int is_directory(const char* path);

#endif
