#include "application/msg/message_channel.h"

#include "infrastructure/unix/message_queue.h"

#include <errno.h>

static int channel_is_valid(const MessageChannel *channel) {
  return channel != NULL && channel->msqid >= 0;
}

int msg_channel_create(MessageChannel *channel) {
  if (channel == NULL) {
    errno = EINVAL;
    return -1;
  }

  channel->msqid = msgq_create();

  return channel->msqid == -1 ? -1 : 0;
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
  if (!channel_is_valid(channel) || message == NULL) {
    errno = EINVAL;
    return -1;
  }

  ssize_t received = msgq_receive(channel->msqid, message,
                                  QUEUED_MESSAGE_PAYLOAD_SIZE, 0, blocking);

  if (received == -1)
    return -1;

  if ((size_t)received != QUEUED_MESSAGE_PAYLOAD_SIZE) {
    errno = EIO;
    return -1;
  }

  return 0;
}

int msg_channel_destroy(MessageChannel *channel) {
  if (channel == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (msgq_remove(channel->msqid) == -1)
    return -1;

  channel->msqid = -1;

  return 0;
}
