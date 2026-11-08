#include "application/shm/shared_memory_context.h"
#include "application/shm/shared_memory_paradigm.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/shm.h>

void test_shm_segment_is_exactly_2048_bytes(void) {
  SharedMemoryChannel channel;
  assert(shm_channel_create(&channel) == 0);

  struct shmid_ds info = {0};

  assert(shmctl(channel.shmid, IPC_STAT, &info) == 0);
  assert(info.shm_segsz == SHM_SEGMENT_SIZE);
  assert(sizeof(*channel.buffer) == SHM_SEGMENT_SIZE);

  assert(shm_channel_destroy(&channel) == 0);
}

void test_shm_channel_transfers_message(void) {
  SharedMemoryChannel channel;
  assert(shm_channel_create(&channel) == 0);

  GlobalSequential sequenciador;
  assert(sequential_init(&sequenciador) == 0);

  uint64_t consumed = 0;
  channel.consumed_count = &consumed;

  SharedMemoryMessage sent = {
      .pid = 1234,
      .sequence = 0,
      .timestamp = 1791251487 * UINT64_C(1000000000) + 123,
      .ack_semid = -1};

  SharedMemoryMessage received = {0};

  assert(shm_channel_send(&channel, &sent, &sequenciador) == 0);
  assert(sent.sequence == 1);
  assert(shm_channel_receive(&channel, &received, false) == 0);

  assert(received.pid == sent.pid);
  assert(received.sequence == 1);
  assert(received.timestamp == sent.timestamp);
  assert(received.ack_semid == -1);
  assert(consumed == 1);

  assert(sequential_destroy(&sequenciador) == 0);
  assert(shm_channel_destroy(&channel) == 0);
}

void test_shm_channel_nonblocking_receive_empty(void) {
  SharedMemoryChannel channel;
  assert(shm_channel_create(&channel) == 0);

  uint64_t consumed = 0;
  channel.consumed_count = &consumed;

  SharedMemoryMessage received = {0};
  errno = 0;
  assert(shm_channel_receive(&channel, &received, false) == -1);
  assert(errno == EAGAIN);
  assert(consumed == 0);

  assert(shm_channel_destroy(&channel) == 0);
}

void test_shm_context_init_and_destroy(void) {
  SharedMemoryContext ctx;
  assert(shm_context_init(&ctx, 4) == 0);
  assert(ctx.n_channels == 4);
  assert(ctx.channels != NULL);
  assert(ctx.consumed_counts != NULL);

  for (int i = 0; i < 4; ++i) {
    struct shmid_ds info = {0};
    assert(shmctl(ctx.channels[i].shmid, IPC_STAT, &info) == 0);
    assert(info.shm_segsz == SHM_SEGMENT_SIZE);
    assert(*ctx.channels[i].consumed_count == 0);
  }

  assert(shm_context_destroy(&ctx) == 0);
  assert(ctx.channels == NULL);
  assert(ctx.n_channels == 0);
  assert(ctx.consumed_counts == NULL);
  assert(ctx.counters_shmid == -1);
}

int main(void) {
  test_shm_segment_is_exactly_2048_bytes();
  test_shm_channel_transfers_message();
  test_shm_channel_nonblocking_receive_empty();
  test_shm_context_init_and_destroy();

  printf("All shared memory IPC tests passed!\n");
  return 0;
}
