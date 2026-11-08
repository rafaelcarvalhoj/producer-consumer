
#ifndef SHARED_MEMORY_PARADIGM_H
#define SHARED_MEMORY_PARADIGM_H

#include "structures/sequential.h"
#include "structures/shared_memory_buffer.h"

typedef struct {
  int shmid;
  int sem_empty;
  int sem_full;
  int sem_mutex;
  SharedMemoryBuffer *buffer;
  uint64_t *consumed_count;
} SharedMemoryChannel;

/*
 * Cria e inicializa um canal independente.
 * Retorna 0 em sucesso e -1 em erro.
 */
int shm_channel_create(SharedMemoryChannel *channel);

/*
 * Envia uma mensagem para o buffer.
 * Pode bloquear enquanto não houver espaço livre.
 */
int shm_channel_send(SharedMemoryChannel *channel, SharedMemoryMessage *message,
                     GlobalSequential *sequenciador);

/*
 * Recebe a mensagem mais antiga do buffer.
 * Pode bloquear enquanto não houver mensagens.
 */
int shm_channel_receive(SharedMemoryChannel *channel,
                        SharedMemoryMessage *message, int blocking);
/*
 * Libera os recursos do canal.
 * Deve ser chamada somente depois que os processos que utilizam
 * o canal tiverem terminado.
 */
int shm_channel_destroy(SharedMemoryChannel *channel);

#endif
