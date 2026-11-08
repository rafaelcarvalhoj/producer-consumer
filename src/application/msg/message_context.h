#ifndef MESSAGE_CONTEXT_H
#define MESSAGE_CONTEXT_H

#include "application/msg/message_channel.h"
#include <stdint.h>

typedef struct {
  int n_channels;
  MessageChannel *channels;

  int counters_shmid;
  uint64_t *consumed_counts;
} MessageContext;

int msg_context_init(MessageContext *context, int producers);
int msg_context_destroy(MessageContext *context);

#endif
