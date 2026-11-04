#ifndef SHARED_MEMORY_CONTEXT_H
#define SHARED_MEMORY_CONTEXT_H

#include "application/shm/shared_memory_paradigm.h"
#include "structures/sequential.h"
#include <stdint.h>

typedef struct {
  int n_channels;
  SharedMemoryChannel *channels;

  GlobalSequential sequential;
  int counters_shmid;
  uint64_t *consumed_counts;
} SharedMemoryContext;

int shm_context_init(SharedMemoryContext *context, int producers);
int shm_context_destroy(SharedMemoryContext *context);

#endif
