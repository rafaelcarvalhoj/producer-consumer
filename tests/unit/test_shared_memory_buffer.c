
#include "structures/shared_memory_buffer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static SharedMemoryMessage make_message(pid_t pid, uint32_t sequence,
                                        time_t seconds) {
  SharedMemoryMessage message = {.pid = pid,
                                 .sequence = sequence,
                                 .timestamp = seconds * UINT64_C(1000000000)};

  return message;
}

void test_buffer_starts_empty(void) {
  SharedMemoryBuffer buffer;
  shm_buffer_init(&buffer);

  assert(shm_buffer_is_empty(&buffer));
  assert(!shm_buffer_is_full(&buffer));
  assert(shm_buffer_size(&buffer) == 0);
}

void test_buffer_push_and_pop(void) {
  SharedMemoryBuffer buffer;
  shm_buffer_init(&buffer);

  SharedMemoryMessage original = make_message(1234, 1, 1000);
  SharedMemoryMessage received = {0};

  assert(shm_buffer_push(&buffer, &original) == 0);
  assert(shm_buffer_size(&buffer) == 1);
  assert(!shm_buffer_is_empty(&buffer));

  assert(shm_buffer_pop(&buffer, &received) == 0);
  assert(received.pid == original.pid);
  assert(received.sequence == original.sequence);
  assert(received.timestamp == original.timestamp);
  assert(shm_buffer_is_empty(&buffer));
}

void test_buffer_preserves_fifo_order(void) {
  SharedMemoryBuffer buffer;
  shm_buffer_init(&buffer);

  SharedMemoryMessage first = make_message(100, 1, 1000);
  SharedMemoryMessage second = make_message(100, 2, 1001);
  SharedMemoryMessage third = make_message(100, 3, 1002);
  SharedMemoryMessage received = {0};

  assert(shm_buffer_push(&buffer, &first) == 0);
  assert(shm_buffer_push(&buffer, &second) == 0);
  assert(shm_buffer_push(&buffer, &third) == 0);

  assert(shm_buffer_pop(&buffer, &received) == 0);
  assert(received.sequence == 1);

  assert(shm_buffer_pop(&buffer, &received) == 0);
  assert(received.sequence == 2);

  assert(shm_buffer_pop(&buffer, &received) == 0);
  assert(received.sequence == 3);

  assert(shm_buffer_is_empty(&buffer));
}

void test_buffer_rejects_pop_when_empty(void) {
  SharedMemoryBuffer buffer;
  SharedMemoryMessage received = {0};

  shm_buffer_init(&buffer);

  assert(shm_buffer_pop(&buffer, &received) == -1);
  assert(shm_buffer_is_empty(&buffer));
}

void test_buffer_rejects_push_when_full(void) {
  SharedMemoryBuffer buffer;
  shm_buffer_init(&buffer);

  for (uint32_t i = 1; i <= SHM_BUFFER_CAPACITY; ++i) {
    SharedMemoryMessage message = make_message(100, i, i);

    assert(shm_buffer_push(&buffer, &message) == 0);
  }

  assert(shm_buffer_is_full(&buffer));
  assert(shm_buffer_size(&buffer) == SHM_BUFFER_CAPACITY);

  SharedMemoryMessage extra = make_message(100, 999, 999);
  assert(shm_buffer_push(&buffer, &extra) == -1);
}

void test_buffer_wraps_around(void) {
  SharedMemoryBuffer buffer;
  shm_buffer_init(&buffer);

  SharedMemoryMessage received = {0};

  /* Preenche o buffer. */
  for (uint32_t i = 1; i <= SHM_BUFFER_CAPACITY; ++i) {
    SharedMemoryMessage message = make_message(200, i, i);
    assert(shm_buffer_push(&buffer, &message) == 0);
  }

  /* Libera algumas posições no início lógico do buffer. */
  for (uint32_t i = 1; i <= 10; ++i) {
    assert(shm_buffer_pop(&buffer, &received) == 0);
    assert(received.sequence == i);
  }

  /* As novas mensagens devem ocupar as posições liberadas. */
  for (uint32_t i = 86; i <= 95; ++i) {
    SharedMemoryMessage message = make_message(200, i, i);
    assert(shm_buffer_push(&buffer, &message) == 0);
  }

  /* As mensagens restantes devem continuar na ordem correta. */
  for (uint32_t i = 11; i <= SHM_BUFFER_CAPACITY; ++i) {
    assert(shm_buffer_pop(&buffer, &received) == 0);
    assert(received.sequence == i);
  }

  for (uint32_t i = 86; i <= 95; ++i) {
    assert(shm_buffer_pop(&buffer, &received) == 0);
    assert(received.sequence == i);
  }

  assert(shm_buffer_is_empty(&buffer));
}

void test_buffer_rejects_null_arguments(void) {
  SharedMemoryBuffer buffer;
  SharedMemoryMessage message = make_message(100, 1, 1000);

  shm_buffer_init(&buffer);

  assert(shm_buffer_push(NULL, &message) == -1);
  assert(shm_buffer_push(&buffer, NULL) == -1);
  assert(shm_buffer_pop(NULL, &message) == -1);
  assert(shm_buffer_pop(&buffer, NULL) == -1);
  assert(shm_buffer_size(NULL) == 0);
  assert(shm_buffer_is_empty(NULL));
  assert(!shm_buffer_is_full(NULL));
}

void test_shared_memory_buffer(void) {
  test_buffer_starts_empty();
  test_buffer_push_and_pop();
  test_buffer_preserves_fifo_order();
  test_buffer_rejects_pop_when_empty();
  test_buffer_rejects_push_when_full();
  test_buffer_wraps_around();
  test_buffer_rejects_null_arguments();

  printf("All shared memory buffer tests passed.\n");
}
