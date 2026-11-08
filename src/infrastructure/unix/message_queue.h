#ifndef UNIX_MESSAGE_QUEUE_H
#define UNIX_MESSAGE_QUEUE_H

#include <stddef.h>
#include <sys/types.h>

/* Cria uma fila de mensagens System V e retorna seu ID, ou -1 em erro. */
int msgq_create(void);

/*
 * Envia uma mensagem. O primeiro campo de message deve ser um long mtype > 0.
 * payload_size é o tamanho da mensagem sem o campo mtype.
 * Bloqueia enquanto a fila estiver cheia.
 */
int msgq_send(int msqid, const void *message, size_t payload_size);

/*
 * Retira a mensagem mais antiga da fila com o tipo type
 * (type == 0: qualquer tipo).
 * Se blocking == 0 e não houver mensagem, retorna -1 com errno = ENOMSG.
 * Retorna o número de bytes lidos (sem o mtype) ou -1 em erro.
 */
ssize_t msgq_receive(int msqid, void *message, size_t payload_size, long type,
                     int blocking);

/* Remove a fila. */
int msgq_remove(int msqid);

#endif
