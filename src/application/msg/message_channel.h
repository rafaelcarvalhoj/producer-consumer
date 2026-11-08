#ifndef MESSAGE_CHANNEL_H
#define MESSAGE_CHANNEL_H

#include <stdint.h>
#include <sys/types.h>

/*
 * Mensagem trafegada na fila. Pela especificação, contém o pid do emissor,
 * o número sequencial da mensagem para o emissor e o tempo do envio.
 * O System V exige que o primeiro campo seja um long mtype > 0.
 */
typedef struct {
  long mtype;
  pid_t pid;
  uint32_t sequence;   // sequencial para o processo emissor (1, 2, ...)
  uint64_t timestamp;  // timestamp em nanossegundos
  int ack_semid;       // -1: sem ack; >= 0: id do semáforo de ack
} QueuedMessage;

#define QUEUED_MESSAGE_TYPE 1L
#define QUEUED_MESSAGE_PAYLOAD_SIZE (sizeof(QueuedMessage) - sizeof(long))

typedef struct {
  int msqid;
} MessageChannel;

/*
 * Cria e inicializa um canal (uma fila de mensagens).
 * Retorna 0 em sucesso e -1 em erro.
 */
int msg_channel_create(MessageChannel *channel);

/*
 * Envia uma mensagem para a fila.
 * Pode bloquear enquanto a fila estiver cheia.
 */
int msg_channel_send(MessageChannel *channel, QueuedMessage *message);

/*
 * Recebe a mensagem mais antiga da fila.
 * Se blocking == 0 e a fila estiver vazia, retorna -1 com errno = ENOMSG.
 */
int msg_channel_receive(MessageChannel *channel, QueuedMessage *message,
                        int blocking);

/*
 * Libera os recursos do canal.
 * Deve ser chamada somente depois que os processos que utilizam
 * o canal tiverem terminado.
 */
int msg_channel_destroy(MessageChannel *channel);

#endif
