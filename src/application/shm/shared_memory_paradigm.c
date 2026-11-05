
#include "application/shm/shared_memory_paradigm.h"
#include "infrastructure/unix/semaphore.h"
#include "infrastructure/unix/shared_memory.h"
#include "structures/sequential.h"

#include <errno.h>
#include <string.h>

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

  *channel = (SharedMemoryChannel){.shmid = -1,
                                   .sem_empty = -1,
                                   .sem_full = -1,
                                   .sem_mutex = -1,
                                   .buffer = NULL,
                                   .consumed_count = NULL};

  channel->shmid = shm_create(SHM_SEGMENT_SIZE);
  if (channel->shmid == -1)
    goto failure;

  channel->buffer = shm_attach(channel->shmid);
  if (channel->buffer == NULL)
    goto failure;

  shm_buffer_init(channel->buffer);

  channel->sem_empty = semaphore_create(SHM_BUFFER_CAPACITY);
  if (channel->sem_empty == -1)
    goto failure;

  channel->sem_full = semaphore_create(0);
  if (channel->sem_full == -1)
    goto failure;

  channel->sem_mutex = semaphore_create(1);
  if (channel->sem_mutex == -1)
    goto failure;

  return 0;

failure:
  {
    int saved_errno = errno;
    shm_channel_destroy(channel);
    errno = saved_errno;
    return -1;
  }
}

int shm_channel_send(SharedMemoryChannel *channel, SharedMemoryMessage *message,
                     GlobalSequential *sequenciador) {
  if (!channel_is_valid(channel) || message == NULL || sequenciador == NULL ||
      sequenciador->next_seq == NULL || sequenciador->idsem < 0) {
    errno = EINVAL;
    return -1;
  }

  if (semaphore_p(channel->sem_empty) == -1)
    return -1;

  if (semaphore_p(sequenciador->idsem) == -1) {
    int saved_errno = errno;
    semaphore_v(channel->sem_empty);
    errno = saved_errno;
    return -1;
  }

  if (semaphore_p(channel->sem_mutex) == -1) {
    int saved_errno = errno;
    semaphore_v(sequenciador->idsem);
    semaphore_v(channel->sem_empty);
    errno = saved_errno;
    return -1;
  }

  int inserted = 0;
  int error = 0;

  uint32_t sequence = *sequenciador->next_seq;

  if (sequence == 0) {
    error = EOVERFLOW;
  } else {
    SharedMemoryMessage queued_message = *message;
    queued_message.sequence = sequence;

    if (shm_buffer_push(channel->buffer, &queued_message) == -1) {
      error = EIO;
    } else {
      *sequenciador->next_seq = sequence + 1;
      message->sequence = sequence;
      inserted = 1;
    }
  }

  if (semaphore_v(channel->sem_mutex) == -1 && error == 0)
    error = errno;

  if (semaphore_v(sequenciador->idsem) == -1 && error == 0)
    error = errno;

  int sem_result = inserted ? semaphore_v(channel->sem_full)
                            : semaphore_v(channel->sem_empty);

  if (sem_result == -1 && error == 0)
    error = errno;

  if (error != 0) {
    errno = error;
    return -1;
  }

  return 0;
}

int shm_channel_receive(SharedMemoryChannel *channel,
                        SharedMemoryMessage *message, int blocking) {
  if (!channel_is_valid(channel) || message == NULL ||
      channel->consumed_count == NULL) {
    errno = EINVAL;
    return -1;
  }

  int result = blocking ? semaphore_p(channel->sem_full)
                        : semaphore_try_p(channel->sem_full);

  if (result == -1)
    return -1;

  if (semaphore_p(channel->sem_mutex) == -1) {
    int saved_errno = errno;
    semaphore_v(channel->sem_full);
    errno = saved_errno;
    return -1;
  }

  if (shm_buffer_pop(channel->buffer, message) == -1) {
    semaphore_v(channel->sem_mutex);
    semaphore_v(channel->sem_full);
    errno = EIO;
    return -1;
  }

  (*channel->consumed_count)++;

  if (semaphore_v(channel->sem_mutex) == -1) {
    int saved_errno = errno;
    semaphore_v(channel->sem_empty);
    errno = saved_errno;
    return -1;
  }

  return semaphore_v(channel->sem_empty);
}

int shm_channel_destroy(SharedMemoryChannel *channel) {
  if (channel == NULL) {
    errno = EINVAL;
    return -1;
  }

  channel->consumed_count = NULL;
  int first_errno = 0;

  if (channel->buffer != NULL) {
    if (shm_detach(channel->buffer) == -1)
      first_errno = errno;
    else
      channel->buffer = NULL;
  }

  if (channel->shmid >= 0) {
    if (shm_remove(channel->shmid) == -1) {
      if (first_errno == 0)
        first_errno = errno;
    } else {
      channel->shmid = -1;
    }
  }

  if (channel->sem_empty >= 0) {
    if (semaphore_destroy(channel->sem_empty) == -1) {
      if (first_errno == 0)
        first_errno = errno;
    } else {
      channel->sem_empty = -1;
    }
  }

  if (channel->sem_full >= 0) {
    if (semaphore_destroy(channel->sem_full) == -1) {
      if (first_errno == 0)
        first_errno = errno;
    } else {
      channel->sem_full = -1;
    }
  }

  if (channel->sem_mutex >= 0) {
    if (semaphore_destroy(channel->sem_mutex) == -1) {
      if (first_errno == 0)
        first_errno = errno;
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
