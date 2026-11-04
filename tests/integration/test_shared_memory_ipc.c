
#include "application/shm/shared_memory_paradigm.h"

#include <assert.h>
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

  SharedMemoryMessage sent = {
      .pid = 1234, .sequence = 42, .timestamp = 1791251487 * UINT64_C(1000000000) + 123};

  SharedMemoryMessage received = {0};

  assert(shm_channel_send(&channel, &sent, NULL) == 0);
  assert(shm_channel_receive(&channel, &received, false) == 0);

  assert(received.pid == sent.pid);
  assert(received.sequence == sent.sequence);
  assert(received.timestamp == sent.timestamp);

  assert(shm_channel_destroy(&channel) == 0);
}

int main(void) {
  test_shm_segment_is_exactly_2048_bytes();
  test_shm_channel_transfers_message();

  printf("All shared memory IPC tests passed!\n");
}
