#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <string.h>

typedef struct directory {
    int pnum;
    int inodeId;
    char* paths;
} directory;

directory* create_directory(char* name, long inodeId, long pnum);
int add_file(directory* dir, char* name, long inodeId);
char* get_name(directory* dir);
void remove_file(directory* dir, char* name);
long get_file_inode(directory* dir, char* name);
size_t get_size_directory(directory* dir);
long get_num_files(directory* dir);
long get_file_names(directory* dir, char*** namesPointer);
void free_directory(directory* dir);
int is_dir_empty(directory* dir);

void* serialize(directory* dir);
directory* deserialize(void* addr, size_t size);

#endif
