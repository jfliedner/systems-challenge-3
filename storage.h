#ifndef NUFS_STORAGE_H
#define NUFS_STORAGE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "directory.h"

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
    long block_num;
} inode;

void storage_init(const char* path);
long get_stat(const char* path, struct stat* st);
long get_stat_inode_id(long inodeId, struct stat* st);
long get_stat_inode(inode* node, struct stat* st);
const char* get_data(const char* path);
inode* get_inode(const char* path);
void get_dirent(const char* path, struct dirent* dirInfo);
directory* read_directory(inode* node);
int is_directory(inode* node);

#endif
