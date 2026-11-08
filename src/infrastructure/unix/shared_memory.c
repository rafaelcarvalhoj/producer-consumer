#include "infrastructure/unix/shared_memory.h"

#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int shm_create(size_t size) {
  if (size == 0) {
    errno = EINVAL;
    return -1;
  }

  return shmget(IPC_PRIVATE, size, 0600);
}

void *shm_attach(int shmid) {
  if (shmid < 0) {
    errno = EINVAL;
    return NULL;
  }

  void *address = shmat(shmid, NULL, 0);

  if (address == (void *)-1) {
    return NULL;
  }

  return address;
}

int shm_detach(const void *address) {
  if (address == NULL) {
    errno = EINVAL;
    return -1;
  }

  return shmdt(address);
}

int shm_remove(int shmid) {
  if (shmid < 0) {
    errno = EINVAL;
    return -1;
  }

  return shmctl(shmid, IPC_RMID, NULL);
}
