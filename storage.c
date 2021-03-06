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

typedef struct inode_pair {
  inode* parent;
  inode* child;
} inode_pair;

meta_block* meta;

int
block_taken(int blockId) {
  return get_bit_state((byte*) &meta->block_status, blockId);
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

void
zero_block(int blockId) {
  void* zeros = malloc(BLOCK_SIZE);
  memset(zeros, 0, BLOCK_SIZE);
  write_to_block(blockId, zeros, BLOCK_SIZE, 0);
  free(zeros);
}

void
take_block(int blockId) {
  if (blockId >= meta->starting_block_index) {
    zero_block(blockId);
  }
  set_bit_high((byte*) &meta->block_status, blockId);
}

int
get_next_block() {
  for (int i = 0; i < BLOCK_COUNT; ++i) {
    if (!block_taken(i)) {
      take_block(i);
      return i;
    }
  }
  return -ENOSPC;
}

void
release_block(int blockId) {
  if (meta->root.nlink > 0 && meta->root.direct < blockId) {
    set_bit_low((byte*) &meta->block_status, blockId);
  }
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
read_path(const char* path, char* buf, size_t size, off_t offset) {
  inode* node = get_inode(path);
  read_data* inodeData = read_inode(node);
  int readSize = (inodeData->size < size) ? inodeData->size : size;
  memcpy(buf, &inodeData->data[offset], readSize);
  if (readSize < size) {
    buf[readSize] = 0;
    ++readSize;
  }
  free_read_data(inodeData);
  return readSize;
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
  long inodeIndex = -1;
  if (has_file(dir, name)) {
    inodeIndex = get_file_inode(dir, name);
  }
  free_directory(dir);
  if (inodeIndex >= 0) {
    return &meta->inodes[inodeIndex];
  }
  else {
    return (inode*) -ENOTDIR;
  }
}

inode_pair*
get_inode_pair(const char* path) {
  inode_pair* pair = malloc(sizeof(inode_pair));
  inode* parent = &meta->root;
  string_array* parsedPath = parse_path((char*) path);

  for (int i = 0; i < parsedPath->length - 2; ++i) {
    if (is_dir_inode(parent)) {
      parent = get_inode_from_dir_inode(parent, parsedPath->data[i]);
    }
    else {
      return (inode_pair*) -1;
    }
  }

  char* basename = parsedPath->data[parsedPath->length - 1];
  inode* child = get_inode_from_dir_inode(parent, basename);
  pair->parent = parent;
  pair->child = child;
  free_string_array(parsedPath);
  return pair;
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

void
free_blocks_from_indirect(long indirectId, int currentCount, int desiredIndirectBlocks) {
  int* indirectBlock = (int*) read_block(indirectId, BLOCK_SIZE);
  for (int i = currentCount - 1; i >= desiredIndirectBlocks; --i) {
    release_block(indirectBlock[i]);
    indirectBlock[i] = 0;
  }
  // We don't know if the block is going to be released, so it's best
  // to just save the state
  write_to_block(indirectId, indirectBlock, BLOCK_SIZE, 0);
  free(indirectBlock);
}

int
free_blocks(inode* node, int currentCount, int desiredCount) {
  if (node->indirect) {
    int currentIndirectBlocks = currentCount - 1;
    int desiredIndirectBlocks = (desiredCount <= 0) ? 0 : desiredCount - 1;
    free_blocks_from_indirect(node->indirect, currentIndirectBlocks, desiredIndirectBlocks);
    if (desiredIndirectBlocks <= 0) {
      release_block(node->indirect);
      node->indirect = 0;
    }
  }
  if (desiredCount == 0) {
    release_block(node->direct);
    node->direct = 0;
  }
  return 0;
}

int
get_blocks(inode* node, int currentCount, int desiredCount) {
  if (desiredCount >= 1 && !node->direct) {
    node->direct = get_next_block();
    ++currentCount;
  }
  if (desiredCount > 1 && !node->indirect) {
    node->indirect = get_next_block();
  }
  if (desiredCount > 1) {
    int* indirectBlock = (int*) read_block(node->indirect, BLOCK_SIZE);
    for (int i = currentCount - 1; i < desiredCount - 1; ++i) {
      indirectBlock[i] = get_next_block();
      if (indirectBlock[i] < 0) {
        int rv = indirectBlock[i];
        free(indirectBlock);
        return rv;
      }
    }
    write_to_block(node->indirect, indirectBlock, BLOCK_SIZE, 0);
    //free(indirectBlock);
  }
  return 0;
}

int
change_inode_size(inode* node, off_t size) {
  int currentBlockCount = node->size / BLOCK_SIZE;
  currentBlockCount = (node->size % BLOCK_SIZE == 0) ? currentBlockCount : currentBlockCount + 1;
  int desiredBlockCount = size / BLOCK_SIZE;
  desiredBlockCount = (size % BLOCK_SIZE == 0) ? desiredBlockCount : desiredBlockCount + 1;
  int rv = 0;
  if (currentBlockCount < desiredBlockCount) {
    rv = get_blocks(node, currentBlockCount, desiredBlockCount);
  }
  else if (desiredBlockCount < currentBlockCount) {
    rv = free_blocks(node, currentBlockCount, desiredBlockCount);
    if (size == 0) {
      release_block(node->direct);
      node->direct = 0;
    }
  }
  node->size = size;
  return rv;
}

int
inode_truncate(const char* path, off_t size) {
  inode* node = get_inode(path);
  if ((long) node < 0) {
    return (long) node;
  }
  return change_inode_size(node, size);
}

int
write_to_blocks(int* blockIds, int numBlocks, void* data, size_t size, off_t offset) {
  int blockIndex = offset / BLOCK_SIZE;
  if (blockIndex >= numBlocks) {
    // TODO: better error value
    return -1;
  }
  int blockOffset = offset % BLOCK_SIZE;
  offset -= blockIndex * BLOCK_SIZE;
  int writeSize = (size < BLOCK_SIZE - offset) ? size : BLOCK_SIZE - offset;
  int writtenBytes = write_to_block(blockIds[blockIndex], data, writeSize, blockOffset);
  ++blockIndex;
  blockOffset = 0;
  // While we still have blocks to write on and things to write to blocks
  while (blockIndex < numBlocks && writtenBytes < size) {
    writeSize = (size - writtenBytes < BLOCK_SIZE) ? size - writtenBytes : BLOCK_SIZE;
    writtenBytes += write_to_block(blockIds[blockIndex], data + writtenBytes, writeSize, blockOffset);
    ++blockIndex;
  }
  return writtenBytes;
}

int
write_to_inode(inode* node, void* data, size_t size, off_t offset) {
  size_t oldSize = node->size;
  if (size + offset > node->size) {
    int rv = change_inode_size(node, size + offset);
    if (rv < 0) {
      return rv;
    }
  }
  int numBlocks = node->size / BLOCK_SIZE;
  if (node->size % BLOCK_SIZE != 0) {
    ++numBlocks;
  }
  int* blockIds = malloc(sizeof(int) * numBlocks);
  if (numBlocks) {
    blockIds[0] = node->direct;
  }
  if (node->indirect && numBlocks > 1) {
    int size = sizeof(int) * (numBlocks - 1);
    int* indirectBlock = (int*) read_block(node->indirect, size);
    memcpy(&blockIds[1], indirectBlock, size);
    free(indirectBlock);
  }
  size_t writtenBytes = write_to_blocks(blockIds, numBlocks, data, size, offset);
  free(blockIds);
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
  if (root->size < 9) {
    set_inode_defaults(root, S_IFDIR | S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    //root->uid = 0000;
    //root->gid = 0000;
    root->direct = meta->starting_block_index;
    take_block(meta->starting_block_index);
    root->indirect = 0;
    directory* rootDirectory = create_directory("", -1, -1);
    void* serialData =  serialize(rootDirectory);
    write_to_inode(root, serialData, get_size_directory(rootDirectory), 0);
  }
}

void storage_init(const char* path) {
  int fd = open(path, O_CREAT | O_RDWR, 0666);
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
  string_array* parsedPath = parse_path((char*) path);
  int lastIndex = parsedPath->length - 1;
  for (int i = 0; i < parsedPath->length; ++i) {
    if (is_dir_inode(currentNode)) {
      currentNode = get_inode_from_dir_inode(currentNode, parsedPath->data[i]);
      if ((long) currentNode < 0) {
        return currentNode;
      }
    }
    else {
      // TODO: this needs to be a different error (i think)
      return (inode*) -ENOENT;
    }
  }
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
inode_link(const char* from, const char* to) {
  string_array* parsedFromPath = parse_path((char*) from);
  string_array* parsedToPath = parse_path((char*) to);
  inode_pair* fromPair = get_inode_pair(from);
  inode_pair* toPair = get_inode_pair(to);
  if ((long) fromPair < 0) {
    return (long) fromPair;
  }
  if ((long) toPair < 0) {
    return (long) toPair;
  }
  char* fromBasename = get_last(parsedFromPath);
  char* toBasename = get_last(parsedToPath);
  directory* fromDir = get_dir_from_inode(fromPair->parent);
  directory* toDir = get_dir_from_inode(toPair->parent);
  long inodeId = get_file_inode(fromDir, fromBasename);
  add_file(toDir, toBasename, inodeId);
  ++meta->inodes[inodeId].nlink;
  void* serializedToDir = serialize(toDir);
  write_to_inode(toPair->parent, serializedToDir, get_size_directory(toDir), 0);

  free_string_array(parsedFromPath);
  free_string_array(parsedToPath);
  free_directory(fromDir);
  free_directory(toDir);
  free(serializedToDir);
  return 0;
}

int
delete_link(inode* parent, inode* child, char* basename) {
  --child->nlink;
  // We only want to remove the inode if there are no links left to it
  // we ALWAYS want to remove the reference in this directory though
  directory* dir = get_dir_from_inode(parent);
  int inodeId = get_file_inode(dir, basename);
  remove_file(dir, basename);
  void* reserializedDir = serialize(dir);
  write_to_inode(parent, reserializedDir, get_size_directory(dir), 0);
  free(reserializedDir);
  if (child->nlink <= 0) {
    free_all_inode_blocks(child);
    //printf("Releasing inode %d\n", inodeId);
    release_inode(inodeId);
  }
  free_directory(dir);
  return 0;
}

int
remove_dir_inode(inode* node) {
  char** fileNames;
  directory* parentDir = get_dir_from_inode(node);
  int numFiles = get_file_names(parentDir, &fileNames);
  for (int i = 0; i < numFiles; ++i) {
    int inodeId = get_file_inode(parentDir, fileNames[i]);
    inode* child = &meta->inodes[inodeId];
    if (is_dir_inode(child)) {
      int rv = remove_dir_inode(child);
    }
    delete_link(node, child, fileNames[i]);
  }
  free_directory(parentDir);
  //free_(node);
  return 0;
}

int
inode_unlink(const char* path) {
  string_array* parsedPath = parse_path((char*) path);
  inode_pair* pair = get_inode_pair(path);
  inode* parent = pair->parent;
  inode* child = pair->child;
  int rv = 0;
  if (is_dir_inode(child)) {
    rv = remove_dir_inode(child);
  }
  else {
    char* basename = get_last(parsedPath);
    rv = delete_link(parent, child, basename);
  }
  free_string_array(parsedPath);
  return rv;
}

int
inode_chmod(const char* path, mode_t mode) {
  inode* node = get_inode(path);
  if ((long) node < 0) {
    return (long) node;
  }
  node->mode = mode;
  return 0;
}

long
get_new_inode(const char* path, mode_t mode, dev_t dev) {
  string_array* array = parse_path((char*) path);
  inode* parent = &meta->root;
  for (int i = 0; i < array->length - 1; ++i) {
    parent = get_inode_from_dir_inode(parent, array->data[i]);
    if ((long) parent < 0) {
      return (long) parent;
    }
  }
  char* basename = array->data[array->length - 1];

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
  add_file(dir, basename, newInodeId);
  void* serializedParent = serialize(dir);
  write_to_inode(parent, serializedParent, get_size_directory(dir), 0);
  free_directory(dir);
  free(serializedParent);

  inode* newFileNode = &meta->inodes[newInodeId];
  set_inode_defaults(newFileNode, mode);
  newFileNode->rdev = dev;
  //printf("Giving inode %d\n", newInodeId);
  return newInodeId;
}

int
create_dir_inode(const char* path, mode_t mode) {
  int inodeId = get_new_inode(path, mode, 0);
  inode* node = &meta->inodes[inodeId];
  printf("mode=%d\n", mode);
  node->mode = mode | S_IFDIR;
  string_array* arr = parse_path((char*) path);
  char* basename = get_last(arr);
  // TODO: do we actually are about the pnum field?
  directory* dir = create_directory(basename, inodeId, 0);
  void* serialDir = serialize(dir);

  write_to_inode(node, serialDir, get_size_directory(dir), 0);

  free(serialDir);
  free_directory(dir);
  free_string_array(arr);
  return 0;
}

int
remove_dir(const char* path) {
  return inode_unlink(path);
}
