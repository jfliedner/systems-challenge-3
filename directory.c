#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "directory.h"

char*
smart_cat(char* str1, char* str2) {
    str1 = realloc(str1, strlen(str1) + strlen(str2));
    return strcat(str1, str2);
}

directory*
create_directory(long pnum) {
    directory* dir = malloc(sizeof(directory));
    dir->pnum = pnum;
    dir->paths = 0;
    return dir;
}

int
add_file(directory* dir, char* name, long inodeId) {
    if (isdigit(*name)) {
        return -EINVAL;
    }
    if (dir->paths) {
        dir->paths = smart_cat(dir->paths, name);
    }
    else {
        int len = strlen(name);
        dir->paths = malloc(len * sizeof(char));
        strcpy(dir->paths, name);
    }
    dir->paths = smart_cat(dir->paths, "/");
    int n = snprintf(NULL, 0, "%lu", inodeId) + 1;
    char* numBuf = malloc(sizeof(char) * n);
    snprintf(numBuf, n, "%lu", inodeId);
    dir->paths = smart_cat(dir->paths, numBuf);
    free(numBuf);
    return 0;
}

void
remove_file(directory* dir, char* name) {
    char* location = strstr(dir->paths, name);
    if (location != NULL) {
        int index = location - dir->paths;
        int slashSeen = 0;
        while (dir->paths[index] && (!slashSeen || isdigit(dir->paths[index]))) {
            if (dir->paths[index] == '/') {
               slashSeen = 1;
            }
            dir->paths[index] = 0;
            ++index;
            location = &dir->paths[index];
        }
        strcat(dir->paths, location);
    }
}

long
get_file_inode(directory* dir, char* name) {
    if (!dir->paths) {
        return -ENOENT;
    }
    char* location = strstr(dir->paths, name);
    if (location != 0) {
        int index = location - dir->paths;
        char* slash = strstr(location, "/");
        char* inodeNum = slash + 1;
        int len = 0;
        while (inodeNum[len] && isdigit(inodeNum[len])) {
            ++len;
        }
        slash = malloc(sizeof(char) * (len + 1));
        strncpy(slash, inodeNum, len);
        return atol(slash);
    }
    return 0;
}

size_t
size_directory(directory* dir) {
    long pathsLen = (dir->paths) ? strlen(dir->paths) : 0;
    return pathsLen + 1 + sizeof(long);
}

long
get_num_files(directory* dir) {
    char* paths = dir->paths;
    if (!paths) {
        return 0;
    }
    long counter = 0;
    while(*paths) {
        if (*paths == '/') {
            ++counter;
        }
        ++paths;
    }
    return counter;
}

long
get_file_names(directory* dir, char*** namesPointer) {
    long numFiles = get_num_files(dir);
    if (!numFiles) {
        return numFiles;
    }
    *namesPointer = malloc(sizeof(char*) * numFiles);
    char** names = *namesPointer;
    int inName = 1;
    long nameIndex = 0;
    long currentChar = 0;
    long currentLength = 0;
    char* currentStart = dir->paths;
    while (dir->paths[currentChar] != 0) {
        if (dir->paths[currentChar] == '/') {
            names[nameIndex] = malloc(sizeof(char) * (currentLength + 1));
            strncpy(names[nameIndex], currentStart, currentLength);
            ++nameIndex;
            inName = 0;
            currentLength = 0;
        }
        if (!inName && !isdigit(dir->paths[currentChar])) {
            inName = 1;
            currentStart = &dir->paths[currentChar];
        }
        if (inName) {
            ++currentLength;
        }
        ++currentChar;
    }
    return numFiles;
}

void
free_directory(directory* dir) {
    free(dir->paths);
    free(dir);
}
