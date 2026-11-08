#ifndef MESSAGE_CONTEXT_H
#define MESSAGE_CONTEXT_H

#include "application/msg/message_channel.h"
#include <stdint.h>

/*
 * Relatório de consumo enviado por cada consumidor à fila de estatísticas
 * quando termina (normalmente ou por SIGTERM no shutdown).
 * mtype = número do canal (1..n_channels).
 */
typedef struct {
  long mtype;
  int consumed;
} ConsumptionReport;

#define CONSUMPTION_REPORT_PAYLOAD_SIZE                                        \
  (sizeof(ConsumptionReport) - sizeof(long))

/*
 * Confirmação de consumo (ACK). Não tem conteúdo: o mtype é o pid do
 * produtor que pediu a confirmação, assim cada produtor recebe só as suas.
 */
typedef struct {
  long mtype;
} AckMessage;

typedef struct {
  int n_channels;
  MessageChannel *channels;

  int ack_msqid;              // fila de confirmações de consumo
  int stats_msqid;            // fila de relatórios de consumo
  uint64_t *consumed_counts;  // preenchido por msg_context_collect_stats
} MessageContext;

int msg_context_init(MessageContext *context, int producers);

/* Confirma ao produtor producer_pid o consumo de uma mensagem dele. */
int msg_context_send_ack(MessageContext *context, pid_t producer_pid);

/*
 * Bloqueia o produtor producer_pid até receber amount confirmações,
 * ou seja, até todas as suas mensagens terem sido consumidas.
 */
int msg_context_wait_acks(MessageContext *context, pid_t producer_pid,
                          int amount);

/*
 * Envia à fila de estatísticas quantas mensagens foram consumidas do canal
 * channel (1..n_channels). Usa apenas msgsnd, para poder ser chamada também
 * a partir do handler de SIGTERM do consumidor.
 */
int msg_context_report_consumption(int stats_msqid, int channel, int consumed);

/*
 * Esvazia a fila de estatísticas e soma os relatórios em consumed_counts.
 * Deve ser chamada depois que todos os consumidores tiverem terminado.
 */
int msg_context_collect_stats(MessageContext *context);

int msg_context_destroy(MessageContext *context);

#endif
