
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>

#include "storage.h"
#include "types.h"

#define DISK_SIZE 1024 * 1024
#define BLOCK_SIZE 512
#define NUM_BLOCKS (DISK_SIZE / BLOCK_SIZE)
#define NUM_BLOCKS_FOR_USED_BLOCK_BITFIELD (NUM_BLOCKS / (sizeof(byte) * 8) / BLOCK_SIZE + 1)
#define ROOT_INODE_OFFSET (NUM_BLOCKS_FOR_USED_BLOCK_BITFIELD * BLOCK_SIZE)

#define INODE_BLOCKS 200
#define INODE_COUNT (INODE_BLOCKS * BLOCK_SIZE) / sizeof(inode)
#define FIRST_DATA_BLOCK INODE_BLOCKS + NUM_BLOCKS_FOR_USED_BLOCK_BITFIELD

typedef struct data_block {
    long next_block;
    byte data[BLOCK_SIZE - sizeof(long)];
} data_block;

typedef struct file_data {
    const char* path;
    int         mode;
    const char* data;
} file_data;

int openFd = -1;
byte usedBlocks[BLOCK_SIZE * NUM_BLOCKS_FOR_USED_BLOCK_BITFIELD];
inode inodes[INODE_COUNT];

static file_data file_table[] = {
    {"/", 040755, 0},
    {"/hello.txt", 0100644, "hello\n"},
    {0, 0, 0},
};

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
    memcpy(&newInode->st_atim, &spec, sizeof(struct timespec));
    memcpy(&newInode->st_mtim, &spec, sizeof(struct timespec));
    memcpy(&newInode->st_ctim, &spec, sizeof(struct timespec));
    newInode->block_num = get_next_block();
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
    }
}

long
get_inode(const char* path) {
    // just return root for now
    return 0;
}

void
get_dirent(const char* path, struct dirent* dir) {
    long inodeId = get_inode(path);
    dir->d_ino = inodeId;
    dir->d_off = 0;
    dir->d_reclen = inodes[inodeId].size;
    dir->d_type = DT_DIR;
    char name[256] = "";
    memcpy(&dir->d_name, &name, 256);
}

static int
streq(const char* aa, const char* bb)
{
    return strcmp(aa, bb) == 0;
}

static file_data*
get_file_data(const char* path) {
    for (int ii = 0; 1; ++ii) {
        file_data row = file_table[ii];

        if (file_table[ii].path == 0) {
            break;
        }

        if (streq(path, file_table[ii].path)) {
            return &(file_table[ii]);
        }
    }

    return 0;
}

int
get_stat(const char* path, struct stat* st)
{
    file_data* dat = get_file_data(path);
    if (!dat) {
        return -1;
    }

    memset(st, 0, sizeof(struct stat));
    st->st_uid  = getuid();
    st->st_mode = dat->mode;
    if (dat->data) {
        st->st_size = strlen(dat->data);
    }
    else {
        st->st_size = 0;
    }
    return 0;
}

const char*
get_data(const char* path)
{
    file_data* dat = get_file_data(path);
    if (!dat) {
        return 0;
    }

    return dat->data;
}
