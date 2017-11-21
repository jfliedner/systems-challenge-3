#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <string.h>

typedef struct directory {
    char* paths;
} directory;

directory* create_directory();
void add_file(directory* dir, char* name, long inodeId);
void remove_file(directory* dir, char* name);
long get_file_inode(directory* dir, char* name);
void free_directory(directory* dir);

#endif
