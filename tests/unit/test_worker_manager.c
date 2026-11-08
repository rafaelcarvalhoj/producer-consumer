#define _POSIX_C_SOURCE 200809L

#include "application/worker_manager.h"
#include "infrastructure/unix/semaphore.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static void test_worker_manager_validation(void) {
  WorkerManager manager;
  worker_manager_init(&manager);
  assert(manager.workers == NULL);
  assert(manager.count == 0);
  assert(manager.capacity == 0);

  errno = 0;
  assert(worker_manager_register(NULL, 123, -1, 0) == -1);
  assert(errno == EINVAL);

  errno = 0;
  assert(worker_manager_register(&manager, 0, -1, 0) == -1);
  assert(errno == EINVAL);

  errno = 0;
  assert(worker_manager_register(&manager, -5, -1, 0) == -1);
  assert(errno == EINVAL);

  errno = 0;
  assert(worker_manager_register(&manager, 123, -2, 0) == -1);
  assert(errno == EINVAL);

  errno = 0;
  assert(worker_manager_zombie_kill(NULL) == -1);
  assert(errno == EINVAL);

  assert(worker_manager_zombie_kill(&manager) == 0);

  bool all_stopped = false;
  errno = 0;
  assert(worker_manager_shutdown(NULL, &all_stopped) == -1);
  assert(errno == EINVAL);

  errno = 0;
  assert(worker_manager_shutdown(&manager, NULL) == -1);
  assert(errno == EINVAL);

  worker_manager_destroy(&manager);
}

static void test_worker_manager_dynamic_capacity_and_zombie_reap(void) {
  WorkerManager manager;
  worker_manager_init(&manager);

  const int total_workers = 20;
  for (int i = 0; i < total_workers; ++i) {
    int ack_sem = (i % 2 == 0) ? ack_semaphore_create() : -1;
    if (i % 2 == 0) {
      assert(ack_sem >= 0);
    }

    pid_t pid = fork();
    assert(pid != -1);

    if (pid == 0) {
      _exit(0);
    }

    assert(worker_manager_register(&manager, pid, ack_sem, i % 2) == 0);
  }

  assert(manager.count == (size_t)total_workers);
  assert(manager.capacity >= 32);

  /* Wait until all short-lived children have exited so zombie_kill reaps all */
  int total_reaped = 0;
  for (int attempt = 0; attempt < 50 && total_reaped < total_workers; ++attempt) {
    int reaped = worker_manager_zombie_kill(&manager);
    assert(reaped >= 0);
    total_reaped += reaped;
    if (total_reaped < total_workers) {
      struct timespec ts = {.tv_sec = 0, .tv_nsec = 20 * 1000 * 1000};
      nanosleep(&ts, NULL);
    }
  }

  assert(total_reaped == total_workers);
  assert(manager.count == 0);

  worker_manager_destroy(&manager);
}

static void test_worker_manager_shutdown_terminates_blocked_children(void) {
  WorkerManager manager;
  worker_manager_init(&manager);

  int ack_sem = ack_semaphore_create();
  assert(ack_sem >= 0);

  for (int i = 0; i < 2; ++i) {
    pid_t pid = fork();
    assert(pid != -1);

    if (pid == 0) {
      for (;;) {
        pause();
      }
      _exit(0);
    }

    int sem_for_worker = (i == 0) ? ack_sem : -1;
    assert(worker_manager_register(&manager, pid, sem_for_worker, i) == 0);
  }

  assert(manager.count == 2);

  bool all_stopped = false;
  assert(worker_manager_shutdown(&manager, &all_stopped) == 0);
  assert(all_stopped);
  assert(manager.count == 0);

  /* Verify the ack semaphore was destroyed during shutdown */
  errno = 0;
  assert(semaphore_p(ack_sem) == -1);
  assert(errno == EINVAL || errno == EIDRM);

  worker_manager_destroy(&manager);
}

void test_worker_manager(void) {
  test_worker_manager_validation();
  test_worker_manager_dynamic_capacity_and_zombie_reap();
  test_worker_manager_shutdown_terminates_blocked_children();
  puts("All worker manager tests passed.");
}
