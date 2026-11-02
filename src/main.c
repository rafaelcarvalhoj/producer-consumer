#include "application/init.h"
#include "application/shm/shared_memory_context.h"
#include "application/shm/shared_memory_process.h"
#include "cli/command_parser.h"
#include "cli/startup_args.h"
#include "unions/paradigm.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static void startup_log(const ProgramArguments *args) {
  puts("Producer-Consumer");
  printf("Producers: %d\n", args->producers);
  printf("Paradigm: %s\n", paradigm_to_str(args->paradigm));
}

int main(int argc, char **argv) {
  ProgramArguments *args = load_args(argc, argv);
  if (args == NULL) {
    return EXIT_FAILURE;
  }

  startup_log(args);

  ProgramCommand *cmd;

  WorkerInfo *producers_table = setup_producers(args->producers);
  ProducerConsumerParadigm paradigm = args->paradigm;
  SharedMemoryContext shm_context;

  if (paradigm == SHARED_MEMORY) {
    if (shm_context_init(&shm_context, args->producers) == -1) {
      perror("Error initializing shared memory");
      free(args);
      return EXIT_FAILURE;
    }
  }
  if (paradigm == MESSAGE_EXCHANGE) {
    // setup_message_exchange_channel(args->producers);
  }

  for (;;) {
    cmd = next_command();
    if (cmd == NULL) {
      free(args);
      return EXIT_FAILURE;
    }
    printf("[MAIN] Comando recebido: op=%d p=%d n=%d ack=%d\n", cmd->op, cmd->p,
           cmd->n_msgs, cmd->ack);

    CommandOperation operation = cmd->op;
    if (operation == SHUTDOWN) {
      free_command(cmd);
      break;
    }

    int result = 0;
    pid_t child_pid;

    if (paradigm == SHARED_MEMORY) {
      result = shm_process(&shm_context, cmd, &child_pid);

      if (result == 0) {
        printf("[MAIN] Processo filho criado: PID=%d\n", child_pid);
        fflush(stdout);
        // TODO: registrar child_pid numa tabela de processos ativos.
      }
    }

    if (paradigm == MESSAGE_EXCHANGE) {
      // msg_process(producers_table, cmd);
    }

    free_command(cmd);

    if (result == -1) {
      perror("Error processing command");
    }
  }

  int status;
  pid_t pid;

  while ((pid = waitpid(-1, &status, 0)) > 0) {
    if (WIFEXITED(status)) {
      printf("[MAIN] Filho %d terminou com código %d\n", pid,
             WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      printf("[MAIN] Filho %d terminou pelo sinal %d\n", pid, WTERMSIG(status));
    }
  }

  if (pid == -1 && errno != ECHILD) {
    perror("waitpid");
  }

  free_workers(producers_table);
  free(args);
  return EXIT_SUCCESS;
}
