#include "directory.h"
#include <assert.h>
#include <stdlib.h>

void
test_add_file() {
  directory* test = create_directory(0);
  add_file(test, "testfile.txt", 1);
  assert(strcmp(test->paths,"testfile.txt/1") == 0);
  add_file(test, "testfile2.txt", 2);
  assert(strcmp(test->paths,"testfile.txt/1testfile2.txt/2") == 0);
  free_directory(test);
}

void
test_get_inode_num() {
  directory* test = create_directory(0);
  add_file(test, "testfile.txt", 1);
  assert(get_file_inode(test, "testfile.txt") == 1);
  add_file(test, "testfile2.txt", 2);
  assert(strcmp(test->paths,"testfile.txt/1testfile2.txt/2") == 0);
  assert(get_file_inode(test, "testfile2.txt") == 2);
  free_directory(test);
}

void
test_remove_file() {
  directory* test = create_directory(0);
  add_file(test, "testfile.txt", 1);
  assert(strcmp(test->paths,"testfile.txt/1") == 0);
  add_file(test, "testfile2.txt", 2);
  assert(strcmp(test->paths,"testfile.txt/1testfile2.txt/2") == 0);

  remove_file(test, "testfile.txt");
  assert(strcmp(test->paths,"testfile2.txt/2") == 0);
  free_directory(test);
}

void
test_num_files() {
  directory* test = create_directory(0);
  add_file(test, "testfile.txt", 1);
  assert(get_num_files(test) == 1);
  free_directory(test);
}

void
test_get_file_names() {
  directory* test = create_directory(0);
  add_file(test, "testfile.txt", 1);
  char** names;
  long numFiles = get_file_names(test, &names);
  assert(numFiles == 1);
  assert(strcmp(names[0], "testfile.txt") == 0);
  free_directory(test);
}

void
test_directory() {
  test_add_file();
  test_get_inode_num();
  test_remove_file();
  test_num_files();
  test_get_file_names();
}

int main() {
  test_directory();
}
