#ifndef PATH_PARSER_H
#define PATH_PARSER_H

typedef struct string_array {
  char** data;
  long length;
} string_array;

/*
 These are always going to assume that root is implicit.
 Therefore they are going to start at the node below root.
 i.e. if your path is /test then the first node is "test",
 not "/"
*/

string_array* parse_path(char* string);
void free_string_array(string_array* array);

#endif
