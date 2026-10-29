
#include "structures/shared_memory_buffer.h"

#include <string.h>

/*
 * Verifica os invariantes internos do buffer circular.
 *
 * head e tail devem apontar para posições válidas.
 * count não pode exceder a capacidade.
 */
static int buffer_is_valid(const SharedMemoryBuffer *buffer) {
  return buffer != NULL && buffer->head < SHM_BUFFER_CAPACITY &&
         buffer->tail < SHM_BUFFER_CAPACITY &&
         buffer->count <= SHM_BUFFER_CAPACITY;
}

void shm_buffer_init(SharedMemoryBuffer *buffer) {
  if (buffer == NULL) {
    return;
  }

  memset(buffer, 0, sizeof(*buffer));
}

uint16_t shm_buffer_size(const SharedMemoryBuffer *buffer) {
  if (!buffer_is_valid(buffer)) {
    return 0;
  }

  return buffer->count;
}

int shm_buffer_is_empty(const SharedMemoryBuffer *buffer) {
  if (!buffer_is_valid(buffer)) {
    return 1;
  }

  return buffer->count == 0;
}

int shm_buffer_is_full(const SharedMemoryBuffer *buffer) {
  if (buffer == NULL) {
    return 0;
  }

  /*
   * Se o estado interno estiver inconsistente, não devemos
   * permitir novas inserções.
   */
  if (!buffer_is_valid(buffer)) {
    return 1;
  }

  return buffer->count == SHM_BUFFER_CAPACITY;
}

int shm_buffer_push(SharedMemoryBuffer *buffer,
                    const SharedMemoryMessage *message) {
  if (message == NULL || !buffer_is_valid(buffer)) {
    return -1;
  }

  if (buffer->count == SHM_BUFFER_CAPACITY) {
    return -1;
  }

  /*
   * Insere no final lógico da fila.
   */
  buffer->messages[buffer->tail] = *message;

  /*
   * Avança circularmente:
   * 0, 1, ..., 84, 0, 1, ...
   */
  buffer->tail = (uint16_t)((buffer->tail + 1) % SHM_BUFFER_CAPACITY);

  ++buffer->count;

  return 0;
}

int shm_buffer_pop(SharedMemoryBuffer *buffer,
                   SharedMemoryMessage *message_out) {
  if (message_out == NULL || !buffer_is_valid(buffer)) {
    return -1;
  }

  if (buffer->count == 0) {
    return -1;
  }

  /*
   * Retira a mensagem mais antiga.
   */
  *message_out = buffer->messages[buffer->head];

  /*
   * Avança o início lógico da fila.
   */
  buffer->head = (uint16_t)((buffer->head + 1) % SHM_BUFFER_CAPACITY);

  --buffer->count;

  return 0;
}
