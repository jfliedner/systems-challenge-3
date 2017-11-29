#include <assert.h>
#include <stdlib.h>

#include "directory.h"
#include "storage.h"
#include "path_parser.h"

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
test_get_multiple_file_names() {
  directory* test = create_directory("", -1, -1);
  add_file(test, "test", 0);
  add_file(test, "test2", 1);
  char** names;
  long numFiles = get_file_names(test, &names);
  assert(numFiles == 2);
  assert(strcmp(names[0], "test") == 0);
  assert(strcmp(names[1], "test2") == 0);
  free_directory(test);
}

void
test_serialize() {
  void* test = create_directory("", 0, 1);
  int* intData = test;
  char* data = (char*) &intData[2];
  assert(intData[0] == 0);
  assert(intData[1] == 1);
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
test_dot_names() {
  directory* dir = create_directory("", 0, 1);
  add_file(dir, ".test.swp", 1);
  assert(get_file_inode(dir, ".test.swp") == 1);
  free_directory(dir);
}

void
test_number_ending_names() {
  directory* dir = create_directory("", 0, 1);
  add_file(dir, "test2", 1);
  char** names;
  get_file_names(dir, &names);
  // We know num files is 1
  assert(strcmp(names[0], "test2") == 0);
  free_directory(dir);
}

void
test_can_have_negative_inode_number() {
  directory* dir = create_directory("", 0, -1);
  add_file(dir, "test2", 1);
  char** names;
  get_file_names(dir, &names);
  // We know num files is 1
  assert(strcmp(names[0], "test2") == 0);
  free_directory(dir);
}

void
test_can_use_negative_inode_ids() {
  directory* dir = create_directory("", -1, -1);
  assert(strcmp(dir->paths, "/-1") == 0);
  free_directory(dir);
}

void
test_get_names_with_negative() {
  directory* dir = create_directory("", -1, -1);
  add_file(dir, "test", 0);
  char** names;
  get_file_names(dir, &names);
  assert(strcmp("test", names[0]) == 0);
  free_directory(dir);
}

void
test_has_file() {
  directory* dir = create_directory("", -1, -1);
  add_file(dir, "test", 0);
  assert(has_file(dir, "test"));
  free_directory(dir);
}

void
test_distinguish_swap_files() {
  directory* dir = create_directory("", -1, -1);
  add_file(dir, ".testing.swp", 0);
  add_file(dir, "testing", 1);
  assert(get_file_inode(dir, "testing") == 1);
  free_directory(dir);
}

void
test_can_have_file_name_start_with_digit() {
  directory* dir = create_directory("", -1, -1);
  add_file(dir, "2k.txt", 0);
  assert(get_file_inode(dir, "2k.txt") == 0);
  free_directory(dir);
}

void
test_no_leading_bslash_in_filename() {
  directory* dir = create_directory("", -1, -1);
  add_file(dir, "normal", 0);
  add_file(dir, "2k.txt", 1);
  add_file(dir, "othernormal", 2);
  char** fileNames;
  assert(get_file_names(dir, &fileNames) == 3);
  assert(strcmp(fileNames[0], "normal") == 0);
  assert(strcmp(fileNames[1], "2k.txt") == 0);
  assert(strcmp(fileNames[2], "othernormal") == 0);
  free_directory(dir);
}

void
test_directory() {
  test_add_file();
  test_get_inode_num();
  test_remove_file();
  test_num_files();
  test_get_file_names();
  test_get_size();
  test_dot_names();
  test_number_ending_names();
  test_can_have_negative_inode_number();
  test_can_use_negative_inode_ids();
  test_get_names_with_negative();
  test_get_multiple_file_names();
  test_has_file();
  test_distinguish_swap_files();
  test_can_have_file_name_start_with_digit();
  test_no_leading_bslash_in_filename();
}

void
test_root_parse() {
  string_array* testArray = parse_path("/");
  assert(testArray->length == 0);
  free_string_array(testArray);
}

void
test_single_parse() {
  string_array* testArray = parse_path("/test");
  assert(testArray->length == 1);
  assert(strcmp(testArray->data[0], "test") == 0);
  free_string_array(testArray);
}

void
test_multi_parse() {
  string_array* testArray = parse_path("/dir/test");
  assert(testArray->length == 2);
  assert(strcmp(testArray->data[0], "dir") == 0);
  assert(strcmp(testArray->data[1], "test") == 0);
  free_string_array(testArray);
}

void
test_can_end_with_slash() {
  string_array* testArray = parse_path("/dir/");
  assert(testArray->length == 1);
  assert(strcmp(testArray->data[0], "dir"));
  free_string_array(testArray);
}

void
test_parser() {
  test_root_parse();
  test_single_parse();
  test_multi_parse();
}
/*
void
test_root() {
  inode* node = get_inode("/");
  directory* dir = read_directory(node);
  assert(strcmp(dir->paths, "/0") == 0);
  free_directory(dir);
}
*/
void
test_storage() {
  //storage_init("test_fs");
  //test_root();
}

int main() {
  test_directory();
  test_parser();
  test_storage();
}
