#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <bsd/string.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "storage.h"
#include "directory.h"

// implementation for: man 2 access
// Checks if a file exists.
int
nufs_access(const char *path, int mask)
{
    printf("access(%s, %04o)\n", path, mask);
    return 0;
}

// implementation for: man 2 stat
// gets an object's attributes (type, permissions, size, etc)
int
nufs_getattr(const char *path, struct stat *st)
{
    // TODO: Might want to make this just return the value gotten from get_stat
    // would require returning the right error codes
    printf("getattr(%s)\n", path);
    int rv = get_stat(path, st);
    if (rv < 0) {
        return -ENOENT;
    }
    else {
        return 0;
    }
}

// implementation for: man 2 readdir
// lists the contents of a directory
int
nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi)
{
    struct stat st;

    printf("readdir(%s)\n", path);

    int rv = get_stat(path, &st);
    if (rv < 0) {
      printf("Stats got less than zero\n");
      return rv;
    }
    // filler is a callback that adds one item to the result
    // it will return non-zero when the buffer is full

    if (!is_directory(path)) {
      // TODO: get the basename here
      filler(buf, path, &st, 0);
      return 0;
    }
    else {
      filler(buf, ".", &st, 0);
    }

    read_data* data = get_data(path);
    directory* dir = deserialize((void*) data->data, data->size);
    char** fileNames;
    long numFiles = get_file_names(dir, &fileNames);
    for (long i = 0; i < numFiles; ++i) {
        get_stat_inode_id(get_file_inode(dir, fileNames[i]), &st);
        filler(buf, fileNames[i], &st, 0);
    }

    free((void*) data);
    free_directory(dir);
    return 0;
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
int
nufs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    printf("mknod(%s, %04o)\n", path, mode);
    long rv = (long) get_new_inode(path, mode, rdev);
    if (rv < 0) {
      return rv;
    }
    return 0;
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int
nufs_mkdir(const char *path, mode_t mode)
{
    printf("mkdir(%s)\n", path);
    return create_dir_inode(path, mode);
}

int
nufs_link(const char *from, const char *to) {
  printf("link(%s => %s)\n", from, to);
  return inode_link(from, to);
}

int
nufs_unlink(const char *path)
{
    printf("unlink(%s)\n", path);
    return inode_unlink(path);
}

int
nufs_rmdir(const char *path)
{
    printf("rmdir(%s)\n", path);
    return -1;
}

// implements: man 2 rename
// called to move a file within the same filesystem
int
nufs_rename(const char *from, const char *to)
{
    printf("rename(%s => %s)\n", from, to);
    int rv = inode_link(from, to);
    if (rv != 0) {
      return rv;
    }
    return inode_unlink(from);
}

int
nufs_chmod(const char *path, mode_t mode)
{
    printf("chmod(%s, %04o)\n", path, mode);
    return inode_chmod(path, mode);
}

int
nufs_truncate(const char *path, off_t size)
{
    printf("truncate(%s, %ld bytes)\n", path, size);
    return inode_truncate(path, size);
}

// this is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
int
nufs_open(const char *path, struct fuse_file_info *fi)
{
    printf("open(%s)\n", path);
    inode* node = get_inode(path);
    if ((long) node < 0) {
      return (long) node;
    }
    else {
      return 0;
    }
}

// Actually read data
int
nufs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("read(%s, %ld bytes, @%ld)\n", path, size, offset);
    return read_path(path, buf, size, offset);
}

// Actually write data
int
nufs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("write(%s, %ld bytes, @%ld)\n", path, size, offset);
    inode* node = get_or_create_inode(path);
    int writeSize = write_to_inode(node, (void*) buf, size, offset);
    return writeSize;
}

// Update the timestamps on a file or directory.
int
nufs_utimens(const char* path, const struct timespec ts[2])
{
  inode* node = get_inode(path);
  if ((long) node < 0) {
    return (long) node;
  }
  // inodes are mapped straight into memory, so updating the inode should
  // update the memory
  memcpy(&node->atim, &ts[0], sizeof(struct timespec));
  memcpy(&node->mtim, &ts[1], sizeof(struct timespec));
	return 0;
}

void
nufs_init_ops(struct fuse_operations* ops)
{
    memset(ops, 0, sizeof(struct fuse_operations));
    ops->access   = nufs_access;
    ops->getattr  = nufs_getattr;
    ops->readdir  = nufs_readdir;
    ops->mknod    = nufs_mknod;
    ops->mkdir    = nufs_mkdir;
    ops->link     = nufs_link;
    ops->unlink   = nufs_unlink;
    ops->rmdir    = nufs_rmdir;
    ops->rename   = nufs_rename;
    ops->chmod    = nufs_chmod;
    ops->truncate = nufs_truncate;
    ops->open	  = nufs_open;
    ops->read     = nufs_read;
    ops->write    = nufs_write;
    ops->utimens  = nufs_utimens;
};

struct fuse_operations nufs_ops;

int
main(int argc, char *argv[])
{
    assert(argc > 2 && argc < 6);
    storage_init(argv[--argc]);
    nufs_init_ops(&nufs_ops);
    return fuse_main(argc, argv, &nufs_ops, NULL);
}
