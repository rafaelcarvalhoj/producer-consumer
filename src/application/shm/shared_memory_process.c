#include "application/shm/shared_memory_context.h"
#include "application/shm/shared_memory_paradigm.h"
#include "structures/program_command.h"
#include "structures/sequential.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int shm_produce(SharedMemoryContext *context,
                       const ProgramCommand *cmd) {
  printf("[PRODUCER %d] Iniciando produção de %d mensagens\n", getpid(),
         cmd->n_msgs);
  fflush(stdout);

  for (int i = 0; i < cmd->n_msgs; i++) {
    uint32_t sequence;

    if (sequential_next(&context->sequential, &sequence) == -1) {
      return -1;
    }

    SharedMemoryMessage msg = {.pid = getpid(), .sequence = sequence};

    if (clock_gettime(CLOCK_REALTIME, &msg.timestamp) == -1) {
      return -1;
    }

    int channel_index = cmd->p == 0 ? i % context->n_channels : cmd->p - 1;

    if (shm_channel_send(&context->channels[channel_index], &msg) == -1) {
      return -1;
    }

    printf("[PRODUCER %d] Mensagem seq=%u -> canal=%d\n", getpid(), sequence,
           channel_index + 1);
    fflush(stdout);
  }

  return 0;
}

static int shm_consume(SharedMemoryContext *context,
                       const ProgramCommand *cmd) {
  for (int i = 0; i < cmd->n_msgs; i++) {
    SharedMemoryMessage msg;

    if (shm_channel_receive(&context->channels[cmd->p - 1], &msg) == -1) {
      return -1;
    }

    printf("[CONSUMER %d] Mensagem seq=%u, produtor=%d\n", getpid(),
           msg.sequence, msg.pid);
    fflush(stdout);
  }

  return 0;
}

int shm_process(SharedMemoryContext *context, ProgramCommand *cmd,
                pid_t *child_pid) {
  if (context == NULL || context->channels == NULL || cmd == NULL ||
      child_pid == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (cmd->p < 0 || cmd->p > context->n_channels || cmd->n_msgs <= 0) {
    errno = EINVAL;
    return -1;
  }

  if (cmd->op != PRODUCE && cmd->op != CONSUME) {
    errno = EINVAL;
    return -1;
  }

  fflush(NULL);

  pid_t pid = fork();

  if (pid == -1) {
    return -1;
  }

  if (pid == 0) {
    int result;

    switch (cmd->op) {
    case PRODUCE:
      result = shm_produce(context, cmd);
      break;

    case CONSUME:
      result = shm_consume(context, cmd);
      _exit(EXIT_FAILURE);

    default:
      _exit(EXIT_FAILURE);
    }

    _exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  *child_pid = pid;

  return 0;
}
