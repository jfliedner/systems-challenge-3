#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

#include "bitmap.h"
#include "directory.h"
#include "storage.h"

#define NUM_INODES
#define BLOCK_BITFIELD_LENGTH (DISK_SIZE / BLOCK_SIZE) / 8

typedef struct meta_block {
  inode root;
  byte block_status[BLOCK_COUNT / 8 + 1];
  byte inode_status[INODE_COUNT / 8 + 1];
  inode inodes[INODE_COUNT];
  int starting_block_index;
  byte* block_start;
} meta_block;

meta_block* meta;

int
block_taken(int blockId) {
  return get_bit_state((byte*) &meta->block_status, blockId);
}

void
take_block(int blockId) {
  set_bit_high((byte*) &meta->block_status, blockId);
}

void
release_block(int blockId) {
  set_bit_low((byte*) &meta->block_status, blockId);
}

int
get_next_block() {
  for (int i = 0; i < BLOCK_COUNT; ++i) {
    if (!block_taken(i)) {
      take_block(i);
      return i;
    }
  }
  return -1;
}

void*
get_block_address(int blockId) {
  return (void*) ((long) meta + ((long) blockId * BLOCK_SIZE));
}

size_t
write_to_block(int blockId, void* data, size_t size) {
  void* blockAddress = get_block_address(blockId);
  size_t writeSize = (size >= BLOCK_SIZE) ? BLOCK_SIZE : size;
  memcpy(blockAddress, data, writeSize);
  return writeSize;
}

void
write_to_inode(inode* node, void* data, size_t size) {
  size_t oldSize = node->size;
  size_t writtenBytes = write_to_block(node->direct, data, size);
  if (writtenBytes < size && node->indirect == 0) {
    node->indirect = get_next_block();
  }
  /*
  while (writtenBytes < size) {

  }*/
  node->size = size;
}

void configure_root() {
  meta->starting_block_index = sizeof(meta_block) / BLOCK_SIZE + 1;

  for (int i = 0; i < meta->starting_block_index; ++i) {
    take_block(i);
  }

  inode* root = &meta->root;
  root->mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH;
  root->nlink = 1;
  root->uid = 0000;
  root->gid = 0000;
  root->rdev = 0;
  struct timespec spec;
  clock_gettime(CLOCK_REALTIME, &spec);
  memcpy(&root->atim, &spec, sizeof(struct timespec));
  memcpy(&root->mtim, &spec, sizeof(struct timespec));
  memcpy(&root->ctim, &spec, sizeof(struct timespec));
  root->direct = meta->starting_block_index;
  take_block(meta->starting_block_index);
  root->indirect = 0;
  directory* rootDirectory = create_directory("", root->direct, -1);
  void* serialData =  serialize(rootDirectory);
  printf("Writing inode\n");
  write_to_inode(root, serialData, get_size_directory(rootDirectory));
}

void storage_init(const char* path) {
  int fd = open(path, O_CREAT | O_TRUNC | O_RDWR);
  // This guarantees things are filled with zero if increasing.
  // Therefore we can assume that if meta->root.direct == 0,
  // we need to configure root
  ftruncate(fd, DISK_SIZE);
  meta = mmap(0, DISK_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd, 0);
  configure_root();
}

long get_stat(const char* path, struct stat* st) {

}

long get_stat_inode_id(long inodeId, struct stat* st) {

}

long get_stat_inode(inode* node, struct stat* st) {

}

const char* get_data(const char* path) {

}

inode* get_inode(const char* path) {

}

void get_dirent(const char* path, struct dirent* dirInfo) {

}

directory* read_directory(inode* node) {

}

int is_directory(inode* node) {

}
