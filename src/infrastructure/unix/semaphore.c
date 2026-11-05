#include "infrastructure/unix/semaphore.h"

#include <errno.h>
#include <limits.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#if !defined(__GNU_LIBRARY__) || defined(_SEM_SEMUN_UNDEFINED)
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

  int id = semget(IPC_PRIVATE, 1, 0600);
  if (id == -1) {
    return -1;
  }

  union semun argument = {.val = initial_value};

  if (semctl(id, 0, SETVAL, argument) == -1) {
    int saved_errno = errno;
    (void)semctl(id, 0, IPC_RMID);
    errno = saved_errno;
    return -1;
  }

  return id;
}

static int semaphore_operate(int semaphore_id, short operation, short flags) {
  if (semaphore_id < 0) {
    errno = EINVAL;
    return -1;
  }

  struct sembuf op = {.sem_num = 0, .sem_op = operation, .sem_flg = flags};

  int result;

  do {
    result = semop(semaphore_id, &op, 1);
  } while (result == -1 && errno == EINTR);

  return result;
}

int semaphore_p(int semaphore_id) {
  return semaphore_operate(semaphore_id, -1, 0);
}

int semaphore_try_p(int semaphore_id) {
  return semaphore_operate(semaphore_id, -1, IPC_NOWAIT);
}

int semaphore_v(int semaphore_id) {
  return semaphore_operate(semaphore_id, 1, 0);
}

int semaphore_destroy(int semaphore_id) {
  if (semaphore_id < 0) {
    return 0;
  }

  return semctl(semaphore_id, 0, IPC_RMID);
}

int ack_semaphore_create(void) { return semaphore_create(0); }

int ack_semaphore_signal(int semid) {
  if (semid == -1) {
    return 0;
  }

  return semaphore_v(semid);
}

int ack_semaphore_wait_n(int semid, unsigned int amount) {
  if (semid < 0 || amount > SHRT_MAX) {
    errno = EINVAL;
    return -1;
  }

  if (amount == 0) {
    return 0;
  }

  return semaphore_operate(semid, -(short)amount, 0);
}

int ack_semaphore_destroy(int semid) { return semaphore_destroy(semid); }
