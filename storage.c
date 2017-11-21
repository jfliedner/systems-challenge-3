
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>

#include "storage.h"
#include "types.h"
#include "directory.h"

#define DISK_SIZE 1024 * 1024
#define BLOCK_SIZE 512
#define PAGE_SIZE 4096
#define NUM_BLOCKS (DISK_SIZE / BLOCK_SIZE)
#define NUM_BLOCKS_FOR_USED_BLOCK_BITFIELD (NUM_BLOCKS / (sizeof(byte) * 8) / BLOCK_SIZE + 1)
#define ROOT_INODE_OFFSET (NUM_BLOCKS_FOR_USED_BLOCK_BITFIELD * BLOCK_SIZE)

#define INODE_BLOCKS 200
#define INODE_COUNT (INODE_BLOCKS * BLOCK_SIZE) / sizeof(inode)
#define FIRST_DATA_BLOCK INODE_BLOCKS + NUM_BLOCKS_FOR_USED_BLOCK_BITFIELD
#define BLOCK_DATA_SIZE BLOCK_SIZE - sizeof(long)

typedef struct data_block {
    long next_block;
    byte data[BLOCK_DATA_SIZE];
} data_block;

int openFd = -1;
byte usedBlocks[BLOCK_SIZE * NUM_BLOCKS_FOR_USED_BLOCK_BITFIELD];
inode inodes[INODE_COUNT];

void
check_rv(int rv) {
    assert(rv >= 0);
}

void
seek_to(off_t addr) {
  lseek(openFd, addr, SEEK_SET);
}

ssize_t
read_fs(off_t addr, void* buf, size_t count) {
    seek_to(addr);
    return read(openFd, buf, count);
}

ssize_t
write_fs(off_t addr, void* buf, size_t count) {
    seek_to(addr);
    return write(openFd, buf, count);
}

int
block_taken(long id) {
    long byte_id = id / (sizeof(byte) * 8);
    byte bit_id = id % (sizeof(byte) * 8);
    return usedBlocks[byte_id] & (1 << bit_id);
}

void
take_block(long id) {
    long byte_id = id / (sizeof(byte) * 8);
    byte bit_id = id % (sizeof(byte) * 8);
    usedBlocks[byte_id] = usedBlocks[byte_id] | (1 << bit_id);
    write_fs(byte_id, &usedBlocks[byte_id], sizeof(byte));
}

void
free_block(long blockId) {
    long byte_id = blockId / (sizeof(byte) * 8);
    byte bit_id = blockId % (sizeof(byte) * 8);
    usedBlocks[byte_id] = usedBlocks[byte_id] & (((sizeof(byte) << 8) - 1) - (1 << bit_id));
    printf("Freed byte %ld, bit %d\n", byte_id, bit_id);
    write_fs(byte_id, &usedBlocks[byte_id], sizeof(byte));
}

data_block*
get_data_block(long blockId) {
    data_block* block = malloc(sizeof(byte) * BLOCK_SIZE);
    read_fs(blockId * BLOCK_SIZE, block, BLOCK_SIZE);
    return block;
}

void
free_block_recursive(long blockId) {
    data_block* nextBlock = get_data_block(blockId);
    // Need to zero the next field in case this thing gets loaded again
    unsigned long* l = malloc(sizeof(unsigned long));
    *l = 0;
    write_fs(blockId * BLOCK_SIZE, l, sizeof(unsigned long));
    free(l);
    free_block(blockId);
    long nextBlockId = nextBlock->next_block;
    free(nextBlock);
    free_block_recursive(nextBlockId);
}

long
get_next_block() {
    long blockId = 0;
    while (blockId < NUM_BLOCKS && block_taken(blockId)) {
        ++blockId;
    }
    if (blockId >= NUM_BLOCKS) {
      return -1;
    }
    take_block(blockId);
    return blockId;
}

long
write_block(long blockId, void* data, size_t size, int needAnotherBlock) {
    assert(size <= BLOCK_DATA_SIZE);
    // This returns the next block if it already has one
    data_block* block = get_data_block(blockId);
    memcpy(&block->data, data, size);
    long nextBlock = block->next_block;
    if (nextBlock && !needAnotherBlock) {
        free_block_recursive(nextBlock);
    }
    else if (!nextBlock && needAnotherBlock) {
        nextBlock = get_next_block();
    }
    write_fs(blockId * BLOCK_SIZE, block, BLOCK_SIZE);
    free(block);
    return nextBlock;
}

void
write_blocks(long startingBlock, void* data, size_t size) {
    long requiredBlocks = size / BLOCK_DATA_SIZE + 1;
    void* start = data;
    long nextBlock = startingBlock;
    while (requiredBlocks > 0 && size > 0) {
        long writeSize = size;
        int needAnotherBlock = writeSize > BLOCK_DATA_SIZE;
        if (needAnotherBlock) {
            writeSize = BLOCK_DATA_SIZE;
        }
        nextBlock = write_block(nextBlock, start, writeSize, needAnotherBlock);
        size -= writeSize;
        start += writeSize;
        --requiredBlocks;
    }
}

void
write_directory(long startingBlock, directory* dir, size_t size) {
    byte* data = malloc(size * sizeof(byte));
    *data = dir->pnum;
    if (dir->paths) {
        memcpy(((void*) data + sizeof(long)), dir->paths, size - sizeof(long));
    }
    write_blocks(startingBlock, data, size);
    free(data);
}

void
initialize_inode(inode* newInode, mode_t mode, uid_t uid, gid_t gid, dev_t rdev, off_t size) {
    newInode->mode = mode;
    newInode->nlink = 1;
    newInode->uid = uid;
    newInode->gid = gid;
    newInode->rdev = rdev;
    newInode->size = size;
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    memcpy(&newInode->atim, &spec, sizeof(struct timespec));
    memcpy(&newInode->mtim, &spec, sizeof(struct timespec));
    memcpy(&newInode->ctim, &spec, sizeof(struct timespec));
    newInode->block_num = get_next_block();
}

int
get_file_type(inode* node) {
    if (node->mode & S_IFDIR) {
        return DT_DIR;
    }
}

void
storage_init(const char* path)
{
    openFd = open(path, O_CREAT | O_TRUNC | O_RDWR);
    check_rv(openFd);
    int rv = ftruncate(openFd, DISK_SIZE);
    check_rv(rv);

    /*printf("Creating file system within %s with size %d\n", path, DISK_SIZE);
    printf("sizeof(byte) == %ld\n", sizeof(byte));
    printf("Block bitfield has size %ld, root starts at offset %ld\n", NUM_BLOCKS_FOR_USED_BLOCK_BITFIELD, ROOT_INODE_OFFSET);
    printf("sizeof(inode) == %ld\n", sizeof(inode));
    printf("inode count == %ld\n", INODE_COUNT);*/
    // Take all of the blocks used by metadata
    for (int i = 0; i < FIRST_DATA_BLOCK; ++i) {
        take_block(i);
    }

    // read inode data into memory
    read_fs(ROOT_INODE_OFFSET, inodes, INODE_COUNT * sizeof(inode));
    if (inodes[0].mode == 0) {
        initialize_inode(&inodes[0], S_IFDIR | S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH, 0, 0, 4096, 0);
        write_fs(ROOT_INODE_OFFSET, &inodes[0], sizeof(inode));
        directory* rootDir = create_directory(-1);
        write_directory(inodes[0].block_num, rootDir, size_directory(rootDir));
    }
}

long
get_inode(const char* path) {
    return 0;
}

void
get_dirent(const char* path, struct dirent* dir) {
    long inodeId = get_inode(path);
    dir->d_ino = inodeId;
    dir->d_off = 0;
    dir->d_reclen = inodes[inodeId].size;
    dir->d_type = get_file_type(&inodes[inodeId]);
    char name[256] = "";
    memcpy(&dir->d_name, &name, 256);
}

static int
streq(const char* aa, const char* bb)
{
    return strcmp(aa, bb) == 0;
}

int
get_stat(const char* path, struct stat* st)
{
    long inodeId = get_inode(path);
    inode* node = &inodes[inodeId];
    st->st_dev = 0;
    st->st_ino = inodeId;
    st->st_mode = node->mode;
    st->st_nlink = node->nlink;
    st->st_gid = node->gid;
    st->st_uid = node->uid;
    st->st_rdev = node->rdev;
    st->st_size = node->size;
    st->st_blksize = (node->size / PAGE_SIZE) + 1;
    st->st_blocks = (node->size / BLOCK_SIZE) + 1;
    memcpy(&st->st_atim, &node->atim, sizeof(struct timespec));
    memcpy(&st->st_mtim, &node->mtim, sizeof(struct timespec));
    memcpy(&st->st_ctim, &node->ctim, sizeof(struct timespec));

    return 0;
}

const char*
get_data(const char* path)
{
    long inodeId = get_inode(path);
    inode* node = &inodes[inodeId];
    long blockNum = node->block_num;
    long readSize = node->size * sizeof(byte);
    byte* data = malloc(readSize);

    void* nextWriteAddr = data;
    // Don't let people read the first data block
    while (blockNum >= FIRST_DATA_BLOCK && readSize > 0) {
        data_block* b = get_data_block(blockNum);
        if (readSize >= BLOCK_DATA_SIZE) {
            memcpy(nextWriteAddr, &b->data, BLOCK_DATA_SIZE);
            readSize -= BLOCK_DATA_SIZE;
            nextWriteAddr += BLOCK_DATA_SIZE;
        }
        else {
            memcpy(nextWriteAddr, &b->data, readSize);
            nextWriteAddr += readSize;
            readSize = 0;
        }
        blockNum = b->next_block;
        free(b);
    }

    return data;
}
