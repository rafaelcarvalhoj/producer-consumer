#include "application/shm/shared_memory_context.h"
#include "structures/sequential.h"
#include <errno.h>
#include <stdlib.h>

static SharedMemoryChannel *setup_shared_memory_channels(const int n_workers) {
  if (n_workers <= 0) {
    errno = EINVAL;
    return NULL;
  }

  SharedMemoryChannel *channels = calloc((size_t)n_workers, sizeof(*channels));

  if (channels == NULL) {
    return NULL;
  }

  for (int i = 0; i < n_workers; i++) {
    if (shm_channel_create(&channels[i]) == -1) {
      int saved_errno = errno;

      for (int j = 0; j < i; j++) {
        shm_channel_destroy(&channels[j]);
      }

      free(channels);
      errno = saved_errno;
      return NULL;
    }
  }

  return channels;
}

int shm_context_init(SharedMemoryContext *context, int producers) {
  if (context == NULL || producers <= 0) {
    errno = EINVAL;
    return -1;
  }

  context->n_channels = 0;
  context->channels = NULL;

  context->sequential.idshm = -1;
  context->sequential.idsem = -1;
  context->sequential.next_seq = NULL;

  context->channels = setup_shared_memory_channels(producers);

  if (context->channels == NULL) {
    return -1;
  }

  context->n_channels = producers;

  if (sequential_init(&context->sequential) == -1) {
    int saved_errno = errno;

    shm_context_destroy(context);

    errno = saved_errno;
    return -1;
  }

  return 0;
}

int shm_context_destroy(SharedMemoryContext *context) {
  if (context == NULL) {
    errno = EINVAL;
    return -1;
  }

  int first_errno = 0;

  if (context->channels != NULL) {
    for (int i = 0; i < context->n_channels; i++) {
      if (shm_channel_destroy(&context->channels[i]) == -1 &&
          first_errno == 0) {
        first_errno = errno;
      }
    }

    if (first_errno == 0) {
      free(context->channels);
      context->channels = NULL;
      context->n_channels = 0;
    }
  }

  sequential_destroy(&context->sequential);

  if (first_errno != 0) {
    errno = first_errno;
    return -1;
  }

  return 0;
}
