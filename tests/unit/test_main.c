#include <stdio.h>

void test_str_to_int(void);
void test_startup_args(void);
void test_paradigm(void);
void test_next_command(void);
void test_shared_memory(void);
void test_shared_memory_buffer(void);
void test_semaphore(void);
void test_sequential(void);
void test_worker_manager(void);

int main(void) {
  test_str_to_int();
  test_startup_args();
  test_paradigm();
  test_next_command();
  test_shared_memory();
  test_shared_memory_buffer();
  test_semaphore();
  test_sequential();
  test_worker_manager();
  puts("Unit tests passed.");
  return 0;
}
