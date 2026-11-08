#include "application/msg/message_process.h"

#include "application/msg/message_channel.h"
#include "application/msg/message_context.h"
#include "infrastructure/unix/semaphore.h"
#include "structures/program_command.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static int msg_produce(MessageContext *context, const ProgramCommand *cmd,
                       int ack_semid) {
  printf("[PRODUCER %d] Iniciando produção de %d mensagens\n", getpid(),
         cmd->n_msgs);
  fflush(stdout);

  int published = 0;
  int production_error = 0;

  for (int i = 0; i < cmd->n_msgs; i++) {
    QueuedMessage msg = {.pid = getpid(),
                         .sequence = (uint32_t)(i + 1),
                         .ack_semid = ack_semid};

    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
      production_error = errno;
      break;
    }

    msg.timestamp =
        (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;

    // p == 0: padrão striped (1, 2, ..., p, 1, 2, ..., p)
    int channel_index = cmd->p == 0 ? i % context->n_channels : cmd->p - 1;

    if (msg_channel_send(&context->channels[channel_index], &msg) == -1) {
      production_error = errno;
      break;
    }

    published++;

    printf("[PRODUCER %d] Mensagem seq=%u -> canal=%d\n", getpid(),
           msg.sequence, channel_index + 1);
    fflush(stdout);
  }

  if (ack_semid >= 0 && published > 0) {
    printf("[PRODUCER %d] Aguardando confirmação de %d mensagens\n", getpid(),
           published);
    fflush(stdout);

    if (ack_semaphore_wait_n(ack_semid, published) == -1)
      return -1;

    printf("[PRODUCER %d] Todas as mensagens foram consumidas\n", getpid());
    fflush(stdout);
  }

  if (production_error != 0) {
    errno = production_error;
    return -1;
  }

  return 0;
}

static int msg_consume(MessageContext *context, const ProgramCommand *cmd) {
  int consumed = 0;

  printf("[CONSUMER %d] Solicitadas %d mensagens (ACK=%d)\n", getpid(),
         cmd->n_msgs, cmd->ack);
  fflush(stdout);

  while (consumed < cmd->n_msgs) {
    QueuedMessage msg;

    if (msg_channel_receive(&context->channels[cmd->p - 1], &msg,
                            cmd->ack == 1) == -1) {

      // ack=2: fila vazia, para de consumir e reporta o erro abaixo
      if (cmd->ack == 2 && errno == ENOMSG)
        break;

      return -1;
    }

    consumed++;

    if (msg.ack_semid >= 0 && ack_semaphore_signal(msg.ack_semid) == -1)
      return -1;

    printf("[CONSUMER %d] Mensagem seq=%u, produtor=%d\n", getpid(),
           msg.sequence, msg.pid);
    fflush(stdout);
  }

  if (consumed < cmd->n_msgs) {
    fprintf(stderr,
            "[CONSUMER %d] Erro: solicitadas %d mensagens, "
            "mas apenas %d foram consumidas.\n",
            getpid(), cmd->n_msgs, consumed);
    errno = ENOMSG;
    return -1;
  }

  printf("[CONSUMER %d] Consumo concluído: %d mensagens\n", getpid(), consumed);
  fflush(stdout);

  return 0;
}

int msg_process(MessageContext *context, const ProgramCommand *cmd,
                pid_t *child_pid, int *ack_semid_out) {
  if (context == NULL || context->channels == NULL ||
      context->n_channels <= 0 || cmd == NULL || child_pid == NULL ||
      ack_semid_out == NULL) {
    errno = EINVAL;
    return -1;
  }

  *child_pid = -1;
  *ack_semid_out = -1;

  if (cmd->n_msgs <= 0 || cmd->n_msgs > 1024 ||
      (cmd->ack != 1 && cmd->ack != 2)) {
    errno = EINVAL;
    return -1;
  }

  if (cmd->op == PRODUCE) {
    if (cmd->p < 0 || cmd->p > context->n_channels) {
      errno = EINVAL;
      return -1;
    }
  } else if (cmd->op == CONSUME) {
    if (cmd->p < 1 || cmd->p > context->n_channels) {
      errno = EINVAL;
      return -1;
    }
  } else {
    errno = EINVAL;
    return -1;
  }

  int ack_semid = -1;

  if (cmd->op == PRODUCE && cmd->ack == 1) {
    ack_semid = ack_semaphore_create();

    if (ack_semid == -1)
      return -1;
  }

  fflush(NULL);

  pid_t pid = fork();

  if (pid == -1) {
    int saved_errno = errno;
    ack_semaphore_destroy(ack_semid);
    errno = saved_errno;
    return -1;
  }

  if (pid == 0) {
    struct sigaction action = {0};

    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) == -1)
      _exit(EXIT_FAILURE);

    int result = cmd->op == PRODUCE ? msg_produce(context, cmd, ack_semid)
                                    : msg_consume(context, cmd);

    _exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  *child_pid = pid;
  *ack_semid_out = ack_semid;

  return 0;
}
