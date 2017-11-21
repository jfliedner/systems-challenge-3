#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <string.h>

typedef struct directory {
    long pnum; // parent  number
    char* paths;
} directory;

directory* create_directory(long pnum);
void add_file(directory* dir, char* name, long inodeId);
void remove_file(directory* dir, char* name);
long get_file_inode(directory* dir, char* name);
size_t size_directory(directory* dir);
void free_directory(directory* dir);

#endif
