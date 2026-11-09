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

typedef struct {
  int n_channels;
  MessageChannel *channels;

  int stats_msqid;            // fila de relatórios de consumo
  uint64_t *consumed_counts;  // preenchido por msg_context_collect_stats
} MessageContext;

int msg_context_init(MessageContext *context, int producers);

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
