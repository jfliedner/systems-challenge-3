#include "directory.h"
#include <assert.h>
#include <stdlib.h>

void
test_add_file() {
  directory* test = create_directory();
  add_file(test, "testfile.txt", 1);
  assert(strcmp(test->paths,"testfile.txt/1") == 0);
  add_file(test, "testfile2.txt", 2);
  assert(strcmp(test->paths,"testfile.txt/1testfile2.txt/2") == 0);
  free(test);
}

void
test_get_inode_num() {
  directory* test = create_directory();
  add_file(test, "testfile.txt", 1);
  assert(get_file_inode(test, "testfile.txt") == 1);
  add_file(test, "testfile2.txt", 2);
  assert(strcmp(test->paths,"testfile.txt/1testfile2.txt/2") == 0);
  assert(get_file_inode(test, "testfile2.txt") == 2);
  free(test);
}

void
test_remove_file() {
  directory* test = create_directory();
  add_file(test, "testfile.txt", 1);
  assert(strcmp(test->paths,"testfile.txt/1") == 0);
  add_file(test, "testfile2.txt", 2);
  assert(strcmp(test->paths,"testfile.txt/1testfile2.txt/2") == 0);

  remove_file(test, "testfile.txt");
  assert(strcmp(test->paths,"testfile2.txt/2") == 0);
  free(test);
}

void
test_directory() {
  test_add_file();
  test_get_inode_num();
  test_remove_file();
}

int main() {
  test_directory();
}
