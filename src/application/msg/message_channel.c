#include "application/msg/message_channel.h"

#include "infrastructure/unix/message_queue.h"
#include "infrastructure/unix/semaphore.h"

#include <errno.h>

static int channel_is_valid(const MessageChannel *channel) {
  return channel != NULL && channel->msqid >= 0 && channel->sem_mutex >= 0;
}

int msg_channel_create(MessageChannel *channel) {
  if (channel == NULL) {
    errno = EINVAL;
    return -1;
  }

  *channel = (MessageChannel){
      .msqid = -1, .sem_mutex = -1, .consumed_count = NULL};

  channel->msqid = msgq_create();
  if (channel->msqid == -1)
    goto failure;

  channel->sem_mutex = semaphore_create(1);
  if (channel->sem_mutex == -1)
    goto failure;

  return 0;

failure:
  {
    int saved_errno = errno;
    msg_channel_destroy(channel);
    errno = saved_errno;
    return -1;
  }
}

int msg_channel_send(MessageChannel *channel, QueuedMessage *message) {
  if (!channel_is_valid(channel) || message == NULL) {
    errno = EINVAL;
    return -1;
  }

  message->mtype = QUEUED_MESSAGE_TYPE;

  return msgq_send(channel->msqid, message, QUEUED_MESSAGE_PAYLOAD_SIZE);
}

int msg_channel_receive(MessageChannel *channel, QueuedMessage *message,
                        int blocking) {
  if (!channel_is_valid(channel) || message == NULL ||
      channel->consumed_count == NULL) {
    errno = EINVAL;
    return -1;
  }

  ssize_t received = msgq_receive(channel->msqid, message,
                                  QUEUED_MESSAGE_PAYLOAD_SIZE, blocking);

  if (received == -1)
    return -1;

  if ((size_t)received != QUEUED_MESSAGE_PAYLOAD_SIZE) {
    errno = EIO;
    return -1;
  }

  if (semaphore_p(channel->sem_mutex) == -1)
    return -1;

  (*channel->consumed_count)++;

  return semaphore_v(channel->sem_mutex);
}

int msg_channel_destroy(MessageChannel *channel) {
  if (channel == NULL) {
    errno = EINVAL;
    return -1;
  }

  channel->consumed_count = NULL;
  int first_errno = 0;

  if (channel->msqid >= 0) {
    if (msgq_remove(channel->msqid) == -1)
      first_errno = errno;
    else
      channel->msqid = -1;
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
