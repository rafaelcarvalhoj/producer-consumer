
#include "infrastructure/unix/semaphore.h"

#include <errno.h>
#include <limits.h>
#include <sys/ipc.h>
#include <sys/sem.h>

/*
 * Em algumas implementações, union semun não é declarada
 * automaticamente pelos headers do sistema.
 */
#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
#else
union semun {
  int val;
  struct semid_ds *buf;
  unsigned short *array;
  struct seminfo *__buf;
};
#endif

int semaphore_create(int initial_value) {
  if (initial_value < 0) {
    errno = EINVAL;
    return -1;
  }

  int id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
  if (id == -1) {
    return -1;
  }

  union semun argument;
  argument.val = initial_value;

  if (semctl(id, 0, SETVAL, argument) == -1) {
    int saved_errno = errno;
    semctl(id, 0, IPC_RMID);
    errno = saved_errno;
    return -1;
  }

  return id;
}

static int semaphore_operate(int semaphore_id, short operation) {
  struct sembuf op = {.sem_num = 0, .sem_op = operation, .sem_flg = 0};

  while (semop(semaphore_id, &op, 1) == -1) {
    if (errno == EINTR) {
      continue;
    }

    return -1;
  }

  return 0;
}

int semaphore_p(int semaphore_id) {
  return semaphore_operate(semaphore_id, -1);
}

int semaphore_v(int semaphore_id) { return semaphore_operate(semaphore_id, 1); }

int semaphore_destroy(int semaphore_id) {
  return semctl(semaphore_id, 0, IPC_RMID);
}
