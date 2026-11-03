
#include "application/shm/shared_memory_paradigm.h"
#include "infrastructure/unix/semaphore.h"
#include "infrastructure/unix/shared_memory.h"

#include <errno.h>

static int channel_is_valid(const SharedMemoryChannel *channel) {
  return channel != NULL && channel->shmid >= 0 && channel->sem_empty >= 0 &&
         channel->sem_full >= 0 && channel->sem_mutex >= 0 &&
         channel->buffer != NULL;
}

int shm_channel_create(SharedMemoryChannel *channel) {
  if (channel == NULL) {
    errno = EINVAL;
    return -1;
  }

  channel->shmid = -1;
  channel->sem_empty = -1;
  channel->sem_full = -1;
  channel->sem_mutex = -1;
  channel->buffer = NULL;

  channel->shmid = shm_create(SHM_SEGMENT_SIZE);
  if (channel->shmid == -1) {
    goto failure;
  }

  channel->buffer = shm_attach(channel->shmid);
  if (channel->buffer == NULL) {
    goto failure;
  }

  shm_buffer_init(channel->buffer);

  channel->sem_empty = semaphore_create((int)SHM_BUFFER_CAPACITY);
  if (channel->sem_empty == -1) {
    goto failure;
  }

  channel->sem_full = semaphore_create(0);
  if (channel->sem_full == -1) {
    goto failure;
  }

  channel->sem_mutex = semaphore_create(1);
  if (channel->sem_mutex == -1) {
    goto failure;
  }

  return 0;

failure:
  {
    int saved_errno = errno;
    (void)shm_channel_destroy(channel);
    errno = saved_errno;
  }

  return -1;
}

int shm_channel_send(SharedMemoryChannel *channel,
                     const SharedMemoryMessage *message) {
  if (!channel_is_valid(channel) || message == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (semaphore_p(channel->sem_empty) == -1) {
    return -1;
  }

  if (semaphore_p(channel->sem_mutex) == -1) {
    int saved_errno = errno;
    (void)semaphore_v(channel->sem_empty);
    errno = saved_errno;
    return -1;
  }

  if (shm_buffer_push(channel->buffer, message) == -1) {
    (void)semaphore_v(channel->sem_mutex);
    (void)semaphore_v(channel->sem_empty);
    errno = EIO;
    return -1;
  }

  int mutex_result = semaphore_v(channel->sem_mutex);
  int mutex_errno = errno;

  int full_result = semaphore_v(channel->sem_full);
  int full_errno = errno;

  if (mutex_result == -1) {
    errno = mutex_errno;
    return -1;
  }

  if (full_result == -1) {
    errno = full_errno;
    return -1;
  }

  return 0;
}

int shm_channel_receive(SharedMemoryChannel *channel,
                        SharedMemoryMessage *message, int blocking) {
  if (!channel_is_valid(channel) || message == NULL) {
    errno = EINVAL;
    return -1;
  }

  int result = blocking ? semaphore_p(channel->sem_full)
                        : semaphore_try_p(channel->sem_full);

  if (result == -1) {
    return -1;
  }

  if (semaphore_p(channel->sem_mutex) == -1) {
    int saved_errno = errno;
    (void)semaphore_v(channel->sem_full);
    errno = saved_errno;
    return -1;
  }

  if (shm_buffer_pop(channel->buffer, message) == -1) {
    (void)semaphore_v(channel->sem_mutex);
    (void)semaphore_v(channel->sem_full);
    errno = EIO;
    return -1;
  }

  int mutex_result = semaphore_v(channel->sem_mutex);
  int mutex_errno = errno;

  int empty_result = semaphore_v(channel->sem_empty);
  int empty_errno = errno;

  if (mutex_result == -1) {
    errno = mutex_errno;
    return -1;
  }

  if (empty_result == -1) {
    errno = empty_errno;
    return -1;
  }

  return 0;
}

int shm_channel_destroy(SharedMemoryChannel *channel) {
  if (channel == NULL) {
    errno = EINVAL;
    return -1;
  }

  int first_errno = 0;

  if (channel->buffer != NULL) {
    if (shm_detach(channel->buffer) == -1) {
      if (first_errno == 0) {
        first_errno = errno;
      }
    } else {
      channel->buffer = NULL;
    }
  }

  if (channel->shmid >= 0) {
    if (shm_remove(channel->shmid) == -1) {
      if (first_errno == 0) {
        first_errno = errno;
      }
    } else {
      channel->shmid = -1;
    }
  }

  if (channel->sem_empty >= 0) {
    if (semaphore_destroy(channel->sem_empty) == -1) {
      if (first_errno == 0) {
        first_errno = errno;
      }
    } else {
      channel->sem_empty = -1;
    }
  }

  if (channel->sem_full >= 0) {
    if (semaphore_destroy(channel->sem_full) == -1) {
      if (first_errno == 0) {
        first_errno = errno;
      }
    } else {
      channel->sem_full = -1;
    }
  }

  if (channel->sem_mutex >= 0) {
    if (semaphore_destroy(channel->sem_mutex) == -1) {
      if (first_errno == 0) {
        first_errno = errno;
      }
    } else {
      channel->sem_mutex = -1;
    }
  }

  if (first_errno != 0) {
    errno = first_errno;
    return -1;
  }

  return 0;
}
