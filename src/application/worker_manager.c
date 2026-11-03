#include "application/worker_manager.h"

#include "infrastructure/unix/semaphore.h"
#include "structures/worker_info.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

void worker_manager_init(WorkerManager *manager) {
  if (manager == NULL) {
    return;
  }

  manager->workers = NULL;
  manager->count = 0;
  manager->capacity = 0;
}

int worker_manager_register(WorkerManager *manager, pid_t pid, int ack_semid) {
  if (manager == NULL || pid <= 0 || ack_semid < -1) {
    errno = EINVAL;
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

  manager->workers[manager->count++] =
      (WorkerInfo){.pid = pid, .ack_semid = ack_semid};

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
    pid_t pid = manager->workers[i].pid;

    if (kill(pid, SIGTERM) == -1 && errno != ESRCH) {
      if (first_errno == 0) {
        first_errno = errno;
      }
    }
  }

  /*
   * Depois, aguarda todos os filhos.
   */
  for (size_t i = 0; i < manager->count; i++) {
    int status;
    pid_t pid;

    do {
      pid = waitpid(manager->workers[i].pid, &status, 0);
    } while (pid == -1 && errno == EINTR);

    if (pid == -1) {
      if (errno == ECHILD) {
        continue;
      }

      if (first_errno == 0) {
        first_errno = errno;
      }

      continue;
    }

    if (WIFEXITED(status)) {
      printf("[MAIN] Filho %d terminou com código %d\n", pid,
             WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      printf("[MAIN] Filho %d terminou pelo sinal %d\n", pid, WTERMSIG(status));
    }
  }

  if (first_errno != 0) {
    errno = first_errno;
    return -1;
  }

  *all_stopped = true;

  for (size_t i = 0; i < manager->count; i++) {
    int semid = manager->workers[i].ack_semid;

    if (semid < 0) {
      continue;
    }

    if (ack_semaphore_destroy(semid) == -1) {
      if (first_errno == 0) {
        first_errno = errno;
      }

      perror("[MAIN] Erro removendo semáforo de ACK");
    }
  }

  if (first_errno != 0) {
    errno = first_errno;
    return -1;
  }

  manager->count = 0;

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

  if (workers == NULL || manager->workers == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (manager->count == 0) {
    return 0;
  }

  int killed = 0;

  for (size_t i = 0; i < manager->count;) {
    int status;
    pid_t result;

    do {
      result = waitpid(workers[i].pid, &status, WNOHANG);
    } while (result == -1 && errno == EINTR);

    if (result == 0) {
      i++;
      continue;
    }

    if (result == -1) {
      if (errno == ECHILD) {
        i++;
        continue;
      }

      return -1;
    }

    printf("[MAIN] Processo filho encerrado: PID=%d", result);

    if (WIFEXITED(status)) {
      printf(", código de saída=%d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      printf(", sinal=%d\n", WTERMSIG(status));
    } else {
      printf("\n");
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
