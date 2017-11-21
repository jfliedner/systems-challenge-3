#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <string.h>

typedef struct directory {
    long pnum; // parent  number
    char* paths;
} directory;

directory* create_directory(long pnum);
int add_file(directory* dir, char* name, long inodeId);
void remove_file(directory* dir, char* name);
long get_file_inode(directory* dir, char* name);
size_t size_directory(directory* dir);
long get_num_files(directory* dir);
long get_file_names(directory* dir, char*** namesPointer);
void free_directory(directory* dir);

#endif
