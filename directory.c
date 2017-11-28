#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include "directory.h"

char*
smart_cat(char* str1, char* str2) {
    if (!str2) {
        return str1;
    }
    if (!str1) {
        str1 = malloc(sizeof(char) * (strlen(str2) + 1));
        return str1;
    }
    char* outString = malloc(strlen(str1) + strlen(str2) + 1);
    memcpy(outString, str1, strlen(str1));
    memcpy(outString + strlen(str1), str2, strlen(str2));
    outString[strlen(str1) + strlen(str2)] = 0;
    return outString;
}

char* num_to_string(long number) {
    int isNegative = number < 0;
    if (isNegative) {
      number = -number;
    }
    int n = snprintf(NULL, 0, "%lu", number) + 1;
    char* numBuf = malloc(sizeof(char) * n);
    snprintf(numBuf, n, "%lu", number);
    if (isNegative) {
      char* output = malloc(2 * sizeof(char));
      output[0] = '-';
      output[1] = 0;
      output = smart_cat(output, numBuf);
      free(numBuf);
      return output;
    }
    else {
      return numBuf;
    }
}

directory*
create_directory(char* name, long inodeId, long pnum) {
    directory* dir = malloc(sizeof(directory));
    dir->pnum = pnum;
    dir->inodeId = inodeId;
    dir->paths = 0;
    add_file(dir, name, inodeId);
    return dir;
}

char*
get_file_start(directory* dir) {
    char* start = strstr(dir->paths, "/");
    while (start && (*start == '/' || isdigit(*start) || *start == '-')) {
        ++start;
    }
    if (*start) {
      return start;
    }
    else {
      return 0;
    }
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
        dir->paths = malloc((len + 1) * sizeof(char));
        strcpy(dir->paths, name);
    }
    dir->paths = smart_cat(dir->paths, "/");
    char* numBuf = num_to_string(inodeId);
    dir->paths = smart_cat(dir->paths, numBuf);
    free(numBuf);
    return 0;
}

char*
get_name(directory* dir) {
    char* start = get_file_start(dir);
    if (!start) {
      return 0;
    }
    char* firstSlash = strstr(start, "/");
    long length = firstSlash - start;
    char* name = malloc(sizeof(char) * (length + 1));
    strncpy(name, start, length);
    name[length] = 0;
    return name;
}

void
remove_file(directory* dir, char* name) {
    char* start = get_file_start(dir);
    if (!start) {
      return;
    }
    char* location = strstr(start, name);
    if (location != NULL) {
        int index = location - start;
        int slashSeen = 0;
        while (start[index] && (!slashSeen || isdigit(start[index]))) {
            if (start[index] == '/') {
               slashSeen = 1;
            }
            start[index] = 0;
            ++index;
            location = &start[index];
        }
        strcat(start, location);
    }
}

long
get_file_inode(directory* dir, char* name) {
    char* start = get_file_start(dir);
    assert(name);
    if (!start) {
        return -ENOENT;
    }
    char* location = strstr(start, name);
    if (location != 0) {
        int index = location - start;
        char* slash = strstr(location, "/");
        char* inodeNum = slash + 1;
        int len = 0;
        while (inodeNum[len] && isdigit(inodeNum[len])) {
            ++len;
        }
        slash = malloc(sizeof(char) * (len + 1));
        strncpy(slash, inodeNum, len);
        long val = atol(slash);
        free(slash);
        return val;
    }
    return -1;
}

size_t
get_size_directory(directory* dir) {
    return sizeof(int) + sizeof(int) + (1 + strlen(dir->paths)) * sizeof(char);
}

long
get_num_files(directory* dir) {
    char* paths = get_file_start(dir);
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
  long currentLength = 0;
  char* fileNameStart = get_file_start(dir);
  int currentStart = 0;
  for (int i = 0; i < strlen(fileNameStart); ++i) {
    char currentChar = fileNameStart[i];
    if (currentChar == '/') {
      names[nameIndex] = malloc(sizeof(char) * (currentLength + 1));
      strncpy(names[nameIndex], &fileNameStart[currentStart], currentLength);
      names[nameIndex][currentLength] = 0;
      ++nameIndex;
      inName = 0;
      currentLength = 0;
    }
    if (!inName && !isdigit(currentChar) && currentChar != '-' && currentChar != '/') {
      inName = 1;
      currentStart = i;
    }
    if (inName) {
      ++currentLength;
    }
  }
  return numFiles;
}

int
has_file(directory* dir, char* name) {
  char slash[2] = "/";
  char* searchString = smart_cat(name, slash);
  return strstr(dir->paths, searchString) != NULL;
}

void
free_directory(directory* dir) {
    free(dir->paths);
    free(dir);
}

void*
serialize(directory* dir) {
    size_t size = get_size_directory(dir);
    //long stringLen = strlen(dir->paths) + 1
    void* writeArray = malloc(size);
    memcpy(writeArray, &dir->pnum, sizeof(int));
    memcpy(writeArray + sizeof(int), &dir->inodeId, sizeof(int));
    long twoInt = sizeof(int) + sizeof(int);
    memcpy(writeArray + twoInt, dir->paths, size - twoInt);
    return writeArray;
}

directory*
deserialize(void* addr, size_t size) {
    int twoInt = sizeof(int) + sizeof(int);
    assert(size >= twoInt + 1);
    int* intPtr = addr;
    directory* dir = malloc(sizeof(directory));
    dir->pnum = intPtr[0];
    dir->inodeId = intPtr[1];
    long dataSize = size - twoInt;
    dir->paths = malloc(sizeof(char) * dataSize);
    void* dataAddr = addr + twoInt;
    memcpy(dir->paths, dataAddr, dataSize);
    return dir;
}
