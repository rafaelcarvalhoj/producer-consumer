#include "structures/sequential.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static void test_sequential_validation(void) {
  uint32_t seq = 0;

  errno = 0;
  assert(sequential_init(NULL) == -1);
  assert(errno == EINVAL);

  errno = 0;
  assert(sequential_destroy(NULL) == -1);
  assert(errno == EINVAL);

  errno = 0;
  assert(sequential_next(NULL, &seq) == -1);
  assert(errno == EINVAL);

  GlobalSequential uninitialized = {.idshm = -1, .idsem = -1, .next_seq = NULL};
  errno = 0;
  assert(sequential_next(&uninitialized, &seq) == -1);
  assert(errno == EINVAL);
}

static void test_sequential_monotonic_single_process(void) {
  GlobalSequential seq_gen;
  assert(sequential_init(&seq_gen) == 0);
  assert(seq_gen.idshm >= 0);
  assert(seq_gen.idsem >= 0);
  assert(seq_gen.next_seq != NULL);
  assert(*seq_gen.next_seq == 1);

  for (uint32_t expected = 1; expected <= 25; ++expected) {
    uint32_t value = 0;
    assert(sequential_next(&seq_gen, &value) == 0);
    assert(value == expected);
  }

  assert(*seq_gen.next_seq == 26);
  assert(sequential_destroy(&seq_gen) == 0);
  assert(seq_gen.idshm == -1);
  assert(seq_gen.idsem == -1);
  assert(seq_gen.next_seq == NULL);
}

static void test_sequential_concurrent_processes(void) {
  GlobalSequential seq_gen;
  assert(sequential_init(&seq_gen) == 0);

  const int workers = 4;
  const int increments_per_worker = 50;
  pid_t pids[4];

  for (int w = 0; w < workers; ++w) {
    pid_t pid = fork();
    assert(pid != -1);

    if (pid == 0) {
      uint32_t prev = 0;
      for (int i = 0; i < increments_per_worker; ++i) {
        uint32_t current = 0;
        if (sequential_next(&seq_gen, &current) == -1) {
          _exit(10);
        }
        if (current <= prev) {
          _exit(11);
        }
        prev = current;
      }
      _exit(0);
    }

    pids[w] = pid;
  }

  for (int w = 0; w < workers; ++w) {
    int status = 0;
    assert(waitpid(pids[w], &status, 0) == pids[w]);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
  }

  assert(*seq_gen.next_seq == (uint32_t)(workers * increments_per_worker + 1));
  assert(sequential_destroy(&seq_gen) == 0);
}

void test_sequential(void) {
  test_sequential_validation();
  test_sequential_monotonic_single_process();
  test_sequential_concurrent_processes();
  puts("All sequential generator tests passed.");
}
