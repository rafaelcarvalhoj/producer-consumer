#include "application/msg/message_context.h"

#include "infrastructure/unix/message_queue.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

static MessageChannel *setup_message_channels(int n_channels) {
  MessageChannel *channels = calloc((size_t)n_channels, sizeof(*channels));

  if (channels == NULL)
    return NULL;

  for (int i = 0; i < n_channels; i++) {
    if (msg_channel_create(&channels[i]) == -1) {
      int saved_errno = errno;

      for (int j = 0; j < i; j++)
        msg_channel_destroy(&channels[j]);

      free(channels);
      errno = saved_errno;
      return NULL;
    }
  }

  return channels;
}

int msg_context_init(MessageContext *context, int producers) {
  if (context == NULL || producers <= 0) {
    errno = EINVAL;
    return -1;
  }

  *context = (MessageContext){.ack_msqid = -1, .stats_msqid = -1};

  context->channels = setup_message_channels(producers);

  if (context->channels == NULL)
    return -1;

  context->n_channels = producers;

  context->consumed_counts =
      calloc((size_t)producers, sizeof(*context->consumed_counts));

  if (context->consumed_counts == NULL)
    goto failure;

  context->ack_msqid = msgq_create();

  if (context->ack_msqid == -1)
    goto failure;

  context->stats_msqid = msgq_create();

  if (context->stats_msqid == -1)
    goto failure;

  return 0;

failure:
  {
    int saved_errno = errno;
    msg_context_destroy(context);
    errno = saved_errno;
    return -1;
  }
}

int msg_context_send_ack(MessageContext *context, pid_t producer_pid) {
  if (context == NULL || producer_pid <= 0) {
    errno = EINVAL;
    return -1;
  }

  AckMessage ack = {.mtype = producer_pid};

  return msgq_send(context->ack_msqid, &ack, 0);
}

int msg_context_wait_acks(MessageContext *context, pid_t producer_pid,
                          int amount) {
  if (context == NULL || producer_pid <= 0 || amount < 0) {
    errno = EINVAL;
    return -1;
  }

  for (int i = 0; i < amount; i++) {
    AckMessage ack;

    if (msgq_receive(context->ack_msqid, &ack, 0, producer_pid, 1) == -1)
      return -1;
  }

  return 0;
}

int msg_context_report_consumption(int stats_msqid, int channel, int consumed) {
  if (channel <= 0 || consumed < 0) {
    errno = EINVAL;
    return -1;
  }

  ConsumptionReport report = {.mtype = channel, .consumed = consumed};

  return msgq_send(stats_msqid, &report, CONSUMPTION_REPORT_PAYLOAD_SIZE);
}

int msg_context_collect_stats(MessageContext *context) {
  if (context == NULL || context->consumed_counts == NULL) {
    errno = EINVAL;
    return -1;
  }

  for (;;) {
    ConsumptionReport report;

    ssize_t received = msgq_receive(context->stats_msqid, &report,
                                    CONSUMPTION_REPORT_PAYLOAD_SIZE, 0, 0);

    if (received == -1) {
      if (errno == ENOMSG)
        return 0;

      return -1;
    }

    if (report.mtype < 1 || report.mtype > context->n_channels ||
        report.consumed < 0) {
      errno = EIO;
      return -1;
    }

    context->consumed_counts[report.mtype - 1] += (uint64_t)report.consumed;
  }
}

int msg_context_destroy(MessageContext *context) {
  if (context == NULL) {
    errno = EINVAL;
    return -1;
  }

  int first_errno = 0;
  bool channels_destroyed = true;

  if (context->channels != NULL) {
    for (int i = 0; i < context->n_channels; i++) {
      if (msg_channel_destroy(&context->channels[i]) == -1) {
        channels_destroyed = false;

        if (first_errno == 0)
          first_errno = errno;
      }
    }

    if (channels_destroyed) {
      free(context->channels);
      context->channels = NULL;
      context->n_channels = 0;
    }
  }

  if (context->ack_msqid >= 0) {
    if (msgq_remove(context->ack_msqid) == -1) {
      if (first_errno == 0)
        first_errno = errno;
    } else {
      context->ack_msqid = -1;
    }
  }

  if (context->stats_msqid >= 0) {
    if (msgq_remove(context->stats_msqid) == -1) {
      if (first_errno == 0)
        first_errno = errno;
    } else {
      context->stats_msqid = -1;
    }
  }

  free(context->consumed_counts);
  context->consumed_counts = NULL;

  if (first_errno != 0) {
    errno = first_errno;
    return -1;
  }

  return 0;
}
