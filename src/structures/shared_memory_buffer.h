
#ifndef SHARED_MEMORY_BUFFER_H
#define SHARED_MEMORY_BUFFER_H

#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#define SHM_SEGMENT_SIZE 2048u
#define SHM_BUFFER_CAPACITY 85u

typedef struct {
  pid_t pid;
  uint32_t sequence;
  struct timespec timestamp;
} SharedMemoryMessage;

/*
 * O cabeçalho ocupa 8 bytes.
 * Cada mensagem ocupa 24 bytes.
 * O buffer inteiro ocupa exatamente 2048 bytes.
 */
typedef struct {
  uint16_t head;
  uint16_t tail;
  uint16_t count;
  uint16_t reserved;
  SharedMemoryMessage messages[SHM_BUFFER_CAPACITY]; // 85 * 24 = 2040 bytes
} SharedMemoryBuffer;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(SharedMemoryMessage) == 24,
               "SharedMemoryMessage deve ocupar 24 bytes");

_Static_assert(sizeof(SharedMemoryBuffer) == SHM_SEGMENT_SIZE,
               "SharedMemoryBuffer deve ocupar exatamente 2048 bytes");
#endif

/* Inicializa um buffer vazio. */
void shm_buffer_init(SharedMemoryBuffer *buffer);

/*
 * Insere uma mensagem.
 * Retorna 0 em caso de sucesso e -1 se o buffer estiver cheio
 * ou os argumentos forem inválidos.
 */
int shm_buffer_push(SharedMemoryBuffer *buffer,
                    const SharedMemoryMessage *message);

/*
 * Retira a mensagem mais antiga.
 * Retorna 0 em caso de sucesso e -1 se o buffer estiver vazio
 * ou os argumentos forem inválidos.
 */
int shm_buffer_pop(SharedMemoryBuffer *buffer,
                   SharedMemoryMessage *message_out);

/* Consultas do estado do buffer. */
uint16_t shm_buffer_size(const SharedMemoryBuffer *buffer);
int shm_buffer_is_empty(const SharedMemoryBuffer *buffer);
int shm_buffer_is_full(const SharedMemoryBuffer *buffer);

#endif
