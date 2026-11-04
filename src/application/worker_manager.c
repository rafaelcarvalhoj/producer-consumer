#include "application/worker_manager.h"

#include "infrastructure/unix/semaphore.h"
#include "structures/worker_info.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

static double elapsed_seconds(struct timespec start, struct timespec end) {
  return (double)(end.tv_sec - start.tv_sec) +
         (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

static void report_worker_exit(const WorkerInfo *worker, int status) {
  struct timespec finished_at;

  if (clock_gettime(CLOCK_MONOTONIC, &finished_at) == -1) {
    perror("[MAIN] Erro obtendo tempo de encerramento");
    return;
  }

  double duration = elapsed_seconds(worker->started_at, finished_at);

  const char *operation = worker->operation == 0   ? "PRODUTOR"
                          : worker->operation == 1 ? "CONSUMIDOR"
                                                   : "WORKER";

  printf("[MAIN] %s PID=%d: %.6f segundos; ", operation, worker->pid, duration);

  if (WIFEXITED(status)) {
    printf("código de saída=%d\n", WEXITSTATUS(status));
  } else if (WIFSIGNALED(status)) {
    printf("interrompido pelo sinal %d\n", WTERMSIG(status));
  } else {
    printf("status não convencional\n");
  }
}

void worker_manager_init(WorkerManager *manager) {
  if (manager == NULL) {
    return;
  }

  manager->workers = NULL;
  manager->count = 0;
  manager->capacity = 0;
}

int worker_manager_register(WorkerManager *manager, pid_t pid, int ack_semid,
                            int operation) {
  if (manager == NULL || pid <= 0 || ack_semid < -1) {
    errno = EINVAL;
    return -1;
  }

  struct timespec started_at;
  if (clock_gettime(CLOCK_MONOTONIC, &started_at) == -1) {
    return -1;
  }

  if (manager->count == manager->capacity) {
    size_t new_capacity = manager->capacity == 0 ? 16 : manager->capacity * 2;

    WorkerInfo *new_workers =
        realloc(manager->workers, new_capacity * sizeof(*new_workers));

    if (new_workers == NULL) {
      return -1;
    }

    manager->workers = new_workers;
    manager->capacity = new_capacity;
  }

  manager->workers[manager->count++] = (WorkerInfo){.pid = pid,
                                                    .ack_semid = ack_semid,
                                                    .operation = operation,
                                                    .started_at = started_at};

  return 0;
}

int worker_manager_shutdown(WorkerManager *manager, bool *all_stopped) {
  if (manager == NULL || all_stopped == NULL) {
    errno = EINVAL;
    return -1;
  }

  *all_stopped = false;

  int first_errno = 0;

  for (size_t i = 0; i < manager->count; i++) {
    if (kill(manager->workers[i].pid, SIGTERM) == -1 && errno != ESRCH) {
      if (first_errno == 0) {
        first_errno = errno;
      }
    }
  }

  size_t remaining = 0;

  for (size_t i = 0; i < manager->count; i++) {
    WorkerInfo worker = manager->workers[i];
    int status;
    pid_t pid;

    do {
      pid = waitpid(worker.pid, &status, 0);
    } while (pid == -1 && errno == EINTR);

    if (pid == -1) {
      if (errno == ECHILD) {
        continue;
      }

      if (first_errno == 0) {
        first_errno = errno;
      }

      // Não conseguimos confirmar o encerramento.
      manager->workers[remaining++] = worker;
      continue;
    }

    if (pid > 0) {
      report_worker_exit(&worker, status);
    }

    if (worker.ack_semid >= 0 &&
        ack_semaphore_destroy(worker.ack_semid) == -1 && first_errno == 0) {
      first_errno = errno;
    }
  }

  manager->count = remaining;
  *all_stopped = (remaining == 0);

  if (first_errno != 0) {
    errno = first_errno;
    return -1;
  }

  return 0;
}

void worker_manager_destroy(WorkerManager *manager) {
  if (manager == NULL) {
    return;
  }

  free(manager->workers);

  manager->workers = NULL;
  manager->count = 0;
  manager->capacity = 0;
}

int worker_manager_zombie_kill(WorkerManager *manager) {
  WorkerInfo *workers = manager->workers;

  if (manager == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (manager->count == 0) {
    return 0;
  }

  if (manager->workers == NULL) {
    errno = EINVAL;
    return -1;
  }

  int killed = 0;

  for (size_t i = 0; i < manager->count;) {
    int status;
    pid_t pid;

    do {
      pid = waitpid(workers[i].pid, &status, WNOHANG);
    } while (pid == -1 && errno == EINTR);

    if (pid == 0) {
      i++;
      continue;
    }

    if (pid == -1) {
      if (errno == ECHILD) {
        i++;
        continue;
      }

      return -1;
    }

    printf("[MAIN] Processo filho encerrado: PID=%d", pid);

    if (pid > 0) {
      report_worker_exit(&workers[i], status);
    }

    if (workers[i].ack_semid >= 0) {
      ack_semaphore_destroy(workers[i].ack_semid);
    }

    workers[i] = workers[manager->count - 1];
    manager->count--;

    killed++;
  }

  return killed;
}
