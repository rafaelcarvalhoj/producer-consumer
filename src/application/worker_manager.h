#ifndef WORKER_MANAGER_H
#define WORKER_MANAGER_H

#include "structures/worker_info.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  WorkerInfo *workers;
  size_t count;
  size_t capacity;
} WorkerManager;

void worker_manager_init(WorkerManager *manager);

int worker_manager_register(WorkerManager *manager, pid_t pid, int ack_semid);

int worker_manager_shutdown(WorkerManager *manager, bool *all_stopped);

void worker_manager_destroy(WorkerManager *manager);

int worker_manager_reap(WorkerInfo *workers, size_t count);

#endif
