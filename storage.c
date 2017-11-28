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
#include "path_parser.h"

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
  return (void*)((long)meta + ((long)blockId * BLOCK_SIZE));
}

size_t
write_to_block(int blockId, void* data, size_t size, off_t offset) {
  // We know that root's block is the first in the fs
  byte* blockAddress = get_block_address(blockId);
  size_t writeSize = (size >= BLOCK_SIZE) ? BLOCK_SIZE : size;
  memcpy(&blockAddress[offset], data, writeSize);
  return writeSize;
}

const char*
read_block(int blockId, size_t size) {
  void* blockAddress = get_block_address(blockId);
  size_t readSize = (size >= BLOCK_SIZE) ? BLOCK_SIZE : size;
  const char* outData = malloc(sizeof(char) * readSize);
  memcpy((void*) outData, blockAddress, readSize);
  return outData;
}

read_data*
read_inode(inode* node) {
  read_data* data = malloc(sizeof(read_data));
  data->type = node->mode;
  data->size = node->size;
  data->data = malloc(node->size);
  byte* readData = (byte*) read_block(node->direct, node->size);
  int readSize = (node->size < BLOCK_SIZE) ? node->size : BLOCK_SIZE;
  memcpy(data->data, readData, readSize);
  free(readData);
  if (node->indirect != 0) {
    int* indirectNodes = (int*) read_block(node->indirect, BLOCK_SIZE);
    int indirectIndex = 0;
    while (indirectNodes[indirectIndex] != 0 && readSize < data->size) {
      int nextReadSize = (data->size > readSize + BLOCK_SIZE) ? BLOCK_SIZE : data->size - readSize;
      readData = (byte*) read_block(indirectNodes[indirectIndex], node->size);
      memcpy(&data->data[readSize], readData, nextReadSize);
      free(readData);
      ++indirectIndex;
      readSize += nextReadSize;
    }
    free(indirectNodes);
  }
  return data;
}

int
is_dir_inode(inode* node) {
  return node->mode & S_IFDIR;
}

directory*
get_dir_from_inode(inode* node) {
  read_data* nodeData = read_inode(node);
  directory* dir = deserialize(nodeData->data, nodeData->size);
  free_read_data(nodeData);
  return dir;
}

inode*
get_inode_from_dir_inode(inode* node, char* name) {
  directory* dir = get_dir_from_inode(node);
  long inodeIndex = get_file_inode(dir, name);
  free_directory(dir);
  return &meta->inodes[inodeIndex];
}

void
free_all_inode_blocks(inode* node) {
  if (node->direct != 0) {
    release_block(node->direct);
  }
  if (node->indirect != 0) {
    int* indirectBlockIds = (int*) read_block(node->indirect, BLOCK_SIZE);
    for (int i = 0; i < BLOCK_SIZE / sizeof(int); ++i) {
      if (indirectBlockIds[i] >= meta->starting_block_index && indirectBlockIds[i] <= BLOCK_COUNT) {
        release_block(indirectBlockIds[i]);
      }
      else {
        // We got junk (probably 0) abort
        break;
      }
    }
    release_block(node->indirect);
  }
}

int
write_to_inode(inode* node, void* data, size_t size, off_t offset) {
  size_t oldSize = node->size;
  if (node->direct == 0) {
    node->direct = get_next_block();
  }
  assert(node->direct >= meta->root.direct);
  size_t writtenBytes = 0;
  if (size + offset > BLOCK_SIZE && node->indirect == 0) {
    // Make sure the block is initialized to zero because that's
    // going to matter
    node->indirect = get_next_block();
    void* data = malloc(BLOCK_SIZE);
    memset(data, 0, BLOCK_SIZE);
    write_to_block(node->indirect, data, BLOCK_SIZE, 0);
    free(data);
  }
  int* indirectBlock = (int*) read_block(node->indirect, BLOCK_SIZE);
  int blockId = offset / BLOCK_SIZE;
  int startingBlockOffset = offset % BLOCK_SIZE;
  while (writtenBytes < size) {
    int blockLow = blockId * BLOCK_SIZE;
    int blockHigh = blockLow + BLOCK_SIZE;
    if (blockId == 0) {
      int writeSize = (size + offset > blockHigh) ? blockHigh - startingBlockOffset : size;
      writtenBytes += write_to_block(node->direct, data, writeSize, startingBlockOffset);
    }
    else {
      int writeSize = (size - writtenBytes >= BLOCK_SIZE) ? BLOCK_SIZE : size - writtenBytes;
      int blockOffset = (writeSize == 0) ? startingBlockOffset : 0;
      if (indirectBlock[blockId - 1] == 0) {
        indirectBlock[blockId - 1] = get_next_block();
      }
      writtenBytes += write_to_block(indirectBlock[blockId - 1], data, writeSize, blockOffset);
    }
    ++blockId;
  }
  if (node->size < offset + writtenBytes) {
    node->size = offset + writtenBytes;
  }
  return writtenBytes;
}

void
set_inode_defaults(inode* node, int mode) {
  node->mode = mode;
  node->nlink = 1;
  node->uid = getuid();
  node->gid = getgid();
  node->rdev = 0;
  node->size = 0;
  struct timespec spec;
  clock_gettime(CLOCK_REALTIME, &spec);
  memcpy(&node->atim, &spec, sizeof(struct timespec));
  memcpy(&node->mtim, &spec, sizeof(struct timespec));
  memcpy(&node->ctim, &spec, sizeof(struct timespec));
  node->direct = 0;
  node->indirect = 0;
}

void configure_root() {
  meta->starting_block_index = sizeof(meta_block) / BLOCK_SIZE + 1;

  for (int i = 0; i < meta->starting_block_index; ++i) {
    take_block(i);
  }

  inode* root = &meta->root;
  set_inode_defaults(root, S_IFDIR | S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  //root->uid = 0000;
  //root->gid = 0000;
  root->direct = meta->starting_block_index;
  take_block(meta->starting_block_index);
  root->indirect = 0;
  directory* rootDirectory = create_directory("", root->direct, -1);
  void* serialData =  serialize(rootDirectory);
  write_to_inode(root, serialData, get_size_directory(rootDirectory), 0);
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

inode*
get_inode(const char* path) {
  inode* currentNode = &meta->root;
  //printf("Checking path %s\n", path);
  if (strcmp(path, "/") == 0) {
    return currentNode;
  }
  char* start = (char*)path + 1;
  char* slash = strstr(start, "/");
  int length;
  do {
    if (slash) {
      length = slash - start;
    }
    else {
      length = strlen(start);
    }
    char* name = malloc(sizeof(char) * (length + 1));
    strncpy(name, start, length);
    name[length] = 0;
    read_data* data = read_inode(currentNode);
    directory* dir = deserialize(data->data, data->size);
    free_read_data(data);
    long inodeId = get_file_inode(dir, name);
    if (inodeId < 0) {
      return (inode*) -ENOENT;//printf("Failed to find name %s\n", name);
    }
    free(name);
    free_directory(dir);
    if (inodeId < 0) {
      return (inode*) -1;
    }
    currentNode = &meta->inodes[inodeId];
    if (start[length] == '/') {
      start = slash + 1;
    }
  } while (start[length] == '/');
  //printf("Returning node with pointer %p (root is %p)\n", currentNode, &meta->root);
  return currentNode;
}

inode*
get_or_create_inode(const char* path) {
  inode* node = get_inode(path);
  if ((long) node < 0) {
    long inodeId = get_new_inode(path, S_IFDIR | S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH, 0);
    return &meta->inodes[inodeId];
  }
  return node;
}

long get_stat(const char* path, struct stat* st) {
  if (!path || *path != '/') {
    return -1;
  }
  inode* node = get_inode(path);
  if ((long) node < 0) {
    return -ENOENT;
  }
  return get_stat_inode(node, st);
}

long get_stat_inode_id(long inodeId, struct stat* st) {
  return get_stat_inode(&meta->inodes[inodeId], st);
}

long get_stat_inode(inode* node, struct stat* st) {
  if ((long) node <= 0) {
      return (long) node;
  }
  long diff = (long) node - (long) meta->inodes;
  // Fudge a little on root
  if (diff < 0) {
    diff = 0;
  }
  st->st_dev = 0;
  st->st_ino = diff / sizeof(inode);
  st->st_mode = node->mode;
  st->st_nlink = node->nlink;
  st->st_gid = node->gid;
  st->st_uid = node->uid;
  st->st_rdev = node->rdev;
  st->st_size = node->size;
  st->st_blksize = BLOCK_SIZE;
  st->st_blocks = (node->size / BLOCK_SIZE) + 1;
  memcpy(&st->st_atim, &node->atim, sizeof(struct timespec));
  memcpy(&st->st_mtim, &node->mtim, sizeof(struct timespec));
  memcpy(&st->st_ctim, &node->ctim, sizeof(struct timespec));

  return 0;
}

read_data*
get_data(const char* path) {
  if (!path || *path != '/') {
    return (read_data*) -1;
  }
  inode* node = get_inode(path);
  if ((long) node < 0) {
    return (read_data*) -1;
  }
  return read_inode(node);
}

void
free_read_data(read_data* data) {
  free(data->data);
  free(data);
}

int
get_file_type(inode* node) {
    if (node->mode & S_IFDIR) {
        return DT_DIR;
    }
    else {
        return node->mode;
    }
}

long
get_dirent(const char* path, struct dirent* dir) {
  if (!path || *path != '/') {
    return -1;
  }
  inode* node = get_inode(path);
  if ((long) node < 0) {
    return -1;
  }
  long diff = (long) node - (long) meta->inodes;
  // Fudge a little on root
  if (diff < 0) {
    diff = 0;
  }
  dir->d_ino = diff / sizeof(inode);
  dir->d_off = 0;
  dir->d_reclen = node->size;
  dir->d_type = get_file_type(node);
  char name[256] = "";
  memcpy(&dir->d_name, &name, 256);
  return 0;
}

int
is_directory(const char* path) {
  if (!path || *path != '/') {
    return 0;
  }
  inode* node = get_inode(path);
  if ((long) node < 0) {
    return -1;
  }
  return is_dir_inode(node);
}

void
release_inode(long inodeId) {
  set_bit_low(meta->inode_status, inodeId);
}

int
inode_unlink(const char* path) {
  string_array* parsedPath = parse_path((char*) path);
  inode* parent = &meta->root;

  for (int i = 0; i < parsedPath->length - 2; ++i) {
    if (is_dir_inode(parent)) {
      parent = get_inode_from_dir_inode(parent, parsedPath->data[i]);
    }
    else {
      return -1;
    }
  }
  char* basename = parsedPath->data[parsedPath->length - 1];
  inode* child = get_inode_from_dir_inode(parent, basename);
  --child->nlink;
  if (child->nlink <= 0) {
    directory* dir = get_dir_from_inode(parent);
    // Do actual unlinking before we remove the name from the dir
    free_all_inode_blocks(child);
    long inodeId = get_file_inode(dir, basename);
    release_inode(inodeId);
    remove_file(dir, basename);
    void* reserializedDir = serialize(dir);
    write_to_inode(parent, reserializedDir, get_size_directory(dir), 0);

    free_directory(dir);
    free(reserializedDir);
  }
  free_string_array(parsedPath);
  return 0;
}

long
get_new_inode(const char* path, mode_t mode, dev_t dev) {
  // TODO: replace this with the new path parsed code
  if (!path || *path != '/') {
    return -1;
  }
  char* slash = strstr(path, "/");
  char* lastSlash;
  do {
    lastSlash = slash;
    slash = strstr(slash + 1, "/");
  } while(slash);
  int pathLength;
  inode* parent;
  char* pathName;
  if (lastSlash == path) {
    pathLength = strlen(path);
    parent = &meta->root;
  }
  else {
    pathLength = lastSlash - path;
    parent = get_inode(pathName);
    if ((long) parent < 0) {
      // Return path is bad
      return -ENOENT;
    }
  }
  // We actually wanna handle if this is -1
  char* fileName = lastSlash + 1;

  if ((long) parent < 0) {
    printf("Error on parent inode get\n");
    // TODO: This should probably return a directory not found error
    return (long) parent;
  }

  // Check the directory data
  read_data* directoryData = read_inode(parent);
  directory* dir = deserialize(directoryData->data, directoryData->size);
  int newInodeId = 0;
  while (get_bit_state(meta->inode_status, newInodeId)) {
    ++newInodeId;
  }
  if (newInodeId >= INODE_COUNT) {
    return -1;
  }
  set_bit_high(meta->inode_status, newInodeId);
  // Add new file to the directory
  add_file(dir, fileName, newInodeId);
  void* serializedParent = serialize(dir);
  write_to_inode(parent, serializedParent, get_size_directory(dir), 0);
  free_directory(dir);
  free(serializedParent);

  printf("Got new inode id %d\n", newInodeId);
  inode* newFileNode = &meta->inodes[newInodeId];
  set_inode_defaults(newFileNode, mode);
  newFileNode->rdev = dev;
  return newInodeId;
}
