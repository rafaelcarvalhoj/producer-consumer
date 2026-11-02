
#include "structures/sequential.h"
#include "infrastructure/unix/semaphore.h"
#include "infrastructure/unix/shared_memory.h"
#include <errno.h>
#include <stdio.h>
#include <sys/sem.h>
#include <sys/shm.h>

int sequential_init(GlobalSequential *sequenciador) {
  if (sequenciador == NULL) {
    errno = EINVAL;
    return -1;
  }

  sequenciador->idshm = -1;
  sequenciador->idsem = -1;
  sequenciador->next_seq = NULL;

  sequenciador->idshm = shm_create(sizeof(int));

  if (sequenciador->idshm < 0) {
    perror("shmget sequenciador");
    return -1;
  }

  void *addr = shm_attach(sequenciador->idshm);

  if (addr == NULL) {
    int saved_errno = errno;

    shm_remove(sequenciador->idshm);
    sequenciador->idshm = -1;

    errno = saved_errno;
    return -1;
  }

  sequenciador->next_seq = (uint32_t *)addr;

  *sequenciador->next_seq = 1;

  sequenciador->idsem = semaphore_create(1);

  if (sequenciador->idsem < 0) {
    int saved_errno = errno;

    shmdt(sequenciador->next_seq);
    shm_remove(sequenciador->idshm);

    sequenciador->next_seq = NULL;
    sequenciador->idshm = -1;

    errno = saved_errno;
    return -1;
  }

  return 0;
}

void sequential_destroy(GlobalSequential *sequenciador) {
  if (sequenciador == NULL) {
    return;
  }

  if (sequenciador->next_seq != NULL) {
    shm_detach(sequenciador->next_seq);
    sequenciador->next_seq = NULL;
  }

  if (sequenciador->idsem >= 0) {
    semaphore_destroy(sequenciador->idsem);
    sequenciador->idsem = -1;
  }

  if (sequenciador->idshm >= 0) {
    shm_remove(sequenciador->idshm);
    sequenciador->idshm = -1;
  }
}

int sequential_next(GlobalSequential *sequenciador, uint32_t *sequence) {
  if (sequenciador == NULL || sequenciador->next_seq == NULL ||
      sequenciador->idsem < 0 || sequence == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (semaphore_p(sequenciador->idsem) == -1) {
    return -1;
  }

  *sequence = (*sequenciador->next_seq)++;

  if (semaphore_v(sequenciador->idsem) == -1) {
    return -1;
  }

  return 0;
}
