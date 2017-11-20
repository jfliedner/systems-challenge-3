#ifndef NUFS_STORAGE_H
#define NUFS_STORAGE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

typedef struct inode {
    mode_t    mode;
    nlink_t   nlink;
    uid_t     uid;
    gid_t     gid;
    dev_t     rdev;
    off_t     size;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    long block_num;
} inode;

void storage_init(const char* path);
int         get_stat(const char* path, struct stat* st);
const char* get_data(const char* path);
void get_dirent(const char* path, struct dirent* dirInfo);

#endif
