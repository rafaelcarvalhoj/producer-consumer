#include "infrastructure/unix/message_queue.h"

#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>

int msgq_create(void) { return msgget(IPC_PRIVATE, IPC_CREAT | 0600); }

int msgq_send(int msqid, const void *message, size_t payload_size) {
  if (msqid < 0 || message == NULL) {
    errno = EINVAL;
    return -1;
  }

  int result;

  do {
    result = msgsnd(msqid, message, payload_size, 0);
  } while (result == -1 && errno == EINTR);

  return result;
}

ssize_t msgq_receive(int msqid, void *message, size_t payload_size, long type,
                     int blocking) {
  if (msqid < 0 || message == NULL) {
    errno = EINVAL;
    return -1;
  }

  int flags = blocking ? 0 : IPC_NOWAIT;
  ssize_t result;

  do {
    result = msgrcv(msqid, message, payload_size, type, flags);
  } while (result == -1 && errno == EINTR);

  return result;
}

int msgq_remove(int msqid) {
  if (msqid < 0) {
    return 0;
  }

  return msgctl(msqid, IPC_RMID, NULL);
}
