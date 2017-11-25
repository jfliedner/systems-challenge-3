#include <assert.h>
#include <stdlib.h>

#include "directory.h"
#include "storage.h"

void
test_add_file() {
  directory* test = create_directory("", 0, 1);
  add_file(test, "testfile.txt", 1);
  assert(strcmp(test->paths,"/0testfile.txt/1") == 0);
  add_file(test, "testfile2.txt", 2);
  assert(strcmp(test->paths,"/0testfile.txt/1testfile2.txt/2") == 0);
  free_directory(test);
}

void
test_get_inode_num() {
  directory* test = create_directory("", 0, 1);
  add_file(test, "testfile.txt", 1);
  assert(get_file_inode(test, "testfile.txt") == 1);
  add_file(test, "testfile2.txt", 2);
  assert(strcmp(test->paths,"/0testfile.txt/1testfile2.txt/2") == 0);
  assert(get_file_inode(test, "testfile2.txt") == 2);
  free_directory(test);
}

void
test_remove_file() {
  directory* test = create_directory("", 0, 1);
  add_file(test, "testfile.txt", 1);
  assert(strcmp(test->paths,"/0testfile.txt/1") == 0);
  add_file(test, "testfile2.txt", 2);
  assert(strcmp(test->paths,"/0testfile.txt/1testfile2.txt/2") == 0);

  remove_file(test, "testfile.txt");
  assert(strcmp(test->paths,"/0testfile2.txt/2") == 0);
  free_directory(test);
}

void
test_num_files() {
  directory* test = create_directory("", 0, 1);
  add_file(test, "testfile.txt", 1);
  assert(get_num_files(test) == 1);
  free_directory(test);
}

void
test_get_file_names() {
  directory* test = create_directory("", 0, 1);
  add_file(test, "testfile.txt", 1);
  char** names;
  long numFiles = get_file_names(test, &names);
  assert(numFiles == 1);
  assert(strcmp(names[0], "testfile.txt") == 0);
  free_directory(test);
}

void
test_serialize() {
  void* test = create_directory("", 0, 1);
  long* longData = test;
  char* data = (char*) &longData[2];
  assert(longData[0] == 0);
  assert(longData[1] == 1);
  assert(strcmp(data, "/0") == 0);
  free_directory(test);
}

void
test_get_size() {
  directory* dir = create_directory("", 0, 1);
  // Expected contents are long,long,"/0"
  assert(get_size_directory(dir) == sizeof(int) + sizeof(int) + 3);
}

void
test_directory() {
  test_add_file();
  test_get_inode_num();
  test_remove_file();
  test_num_files();
  test_get_file_names();
  test_get_size();
}

void
test_root() {
  inode* node = get_inode("/");
  directory* dir = read_directory(node);
  assert(strcmp(dir->paths, "/0") == 0);
  free_directory(dir);
}

void
test_storage() {
  storage_init("test_fs");
  test_root();
}

int main() {
  test_directory();
  test_storage();
}
