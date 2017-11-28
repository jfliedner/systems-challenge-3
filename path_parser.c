#include "path_parser.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

int
occurences(char* str, char c) {
  int count = 0;
  while (str && *str) {
    if (*str == c) {
      ++count;
    }
    ++str;
  }
  return count;
}

void
add_string(string_array* arr, char* buf, long length) {
  // This assumes the thing is already malloc-ed to the correct length
  arr->data[arr->length] = malloc(sizeof(char) * (length + 1));
  memcpy(arr->data[arr->length], buf, length);
  arr->data[arr->length][length] = 0;
  ++arr->length;
}

string_array*
parse_path(char* path)
{
  // I think it's determined elsewhere that 256 is the max
  char temp[256] = "";
  char* start = path;
  int startWithSlash = start && *start == '/';
  while (start && *start == '/') {
    ++start;
  }
  string_array* array = malloc(sizeof(string_array));
  array->length = 0;
  array->data = malloc(sizeof(char*) * (occurences(start, '/') + 1));
  int startIndex = 0;
  int tempIndex = 0;
  while (start && *start) {
    if (*start == '/') {
      add_string(array, temp, tempIndex);
      tempIndex = 0;
      memset(temp, 0, 256);
    }
    else {
      temp[tempIndex] = *start;
      ++tempIndex;
    }
    ++start;
  }
  if (tempIndex > 0) {
    add_string(array, temp, tempIndex);
  }
  return array;
}

char*
get_last(string_array* arr) {
  assert(arr->length);
  return arr->data[arr->length - 1];
}

void
free_string_array(string_array* array) {
  for (int i = 0; i < array->length; ++i) {
    free(array->data[i]);
  }
  free(array);
}
