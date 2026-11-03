#include "application/runtime.h"

#include "application/shm/shared_memory_context.h"
#include "application/shm/shared_memory_process.h"
#include "application/worker_manager.h"

#include "cli/command_parser.h"

#include "infrastructure/unix/semaphore.h"

#include "unions/paradigm.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t shutdown_requested = 0;

static void handle_sigint(int signum) {
  (void)signum;
  shutdown_requested = 1;
}

static int install_signal_handlers(void) {
  struct sigaction action = {0};

  action.sa_handler = handle_sigint;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  return sigaction(SIGINT, &action, NULL);
}

static void startup_log(const ProgramArguments *args) {
  puts("Producer-Consumer");
  printf("Producers: %d\n", args->producers);
  printf("Paradigm: %s\n", paradigm_to_str(args->paradigm));
}

static void terminate_unregistered_child(pid_t pid, int ack_semid) {
  int saved_errno = errno;

  kill(pid, SIGTERM);

  while (waitpid(pid, NULL, 0) == -1 && errno == EINTR) {
  }

  if (ack_semid >= 0) {
    ack_semaphore_destroy(ack_semid);
  }

  errno = saved_errno;
}

static int process_command(void *context, WorkerManager *manager,
                           ProducerConsumerParadigm paradigm,
                           const ProgramCommand *cmd) {
  pid_t child_pid;
  int ack_semid = -1;

  switch (paradigm) {
  case SHARED_MEMORY:
    if (shm_process((SharedMemoryContext *)context, cmd, &child_pid,
                    &ack_semid) == -1) {
      return -1;
    }
    break;

  case MESSAGE_EXCHANGE:
    /*
     * if (msg_process((MessageContext *)context, cmd, &child_pid,
     *                 &ack_semid) == -1) {
     *   return -1;
     * }
     */
    errno = ENOSYS;
    return -1;

  default:
    errno = EINVAL;
    return -1;
  }

  if (worker_manager_register(manager, child_pid, ack_semid) == -1) {
    terminate_unregistered_child(child_pid, ack_semid);
    return -1;
  }

  printf("[MAIN] Processo filho criado: PID=%d, ACK=%d\n", child_pid,
         ack_semid);
  fflush(stdout);

  return 0;
}

static int run_command_loop(void *context, WorkerManager *manager,
                            ProducerConsumerParadigm paradigm) {
  int exit_status = EXIT_SUCCESS;

  while (!shutdown_requested) {
    errno = 0;

    if (worker_manager_zombie_kill(manager) == -1) {
      perror("[MAIN] Erro recolhendo processos filhos");
    }

    ProgramCommand *cmd = next_command();

    if (shutdown_requested) {
      if (cmd != NULL) {
        free_command(cmd);
      }

      clearerr(stdin);
      break;
    }

    if (cmd == NULL) {
      if (errno == EINTR) {
        clearerr(stdin);
        continue;
      }

      if (feof(stdin)) {
        break;
      }

      perror("[MAIN] Erro lendo comando");
      exit_status = EXIT_FAILURE;
      break;
    }

    printf("[MAIN] Comando recebido: op=%d p=%d n=%d ack=%d\n", cmd->op, cmd->p,
           cmd->n_msgs, cmd->ack);

    if (cmd->op == SHUTDOWN) {
      free_command(cmd);
      break;
    }

    if (process_command(context, manager, paradigm, cmd) == -1) {
      perror("[MAIN] Erro processando comando");
    }

    free_command(cmd);
  }

  return exit_status;
}

int application_run(const ProgramArguments *args) {
  if (args == NULL) {
    errno = EINVAL;
    return EXIT_FAILURE;
  }

  if (install_signal_handlers() == -1) {
    perror("[MAIN] Erro instalando handlers");
    return EXIT_FAILURE;
  }

  startup_log(args);

  SharedMemoryContext shm_context = {0};
  // MessageContext msg_context = {0};

  void *context = NULL;
  bool context_initialized = false;

  switch (args->paradigm) {
  case SHARED_MEMORY:
    if (shm_context_init(&shm_context, args->producers) == -1) {
      perror("[MAIN] Erro inicializando memória compartilhada");
      return EXIT_FAILURE;
    }

    context = &shm_context;
    context_initialized = true;
    break;

  case MESSAGE_EXCHANGE:
    /*
     * if (msg_context_init(&msg_context, args->producers) == -1) {
     *   perror("[MAIN] Erro inicializando troca de mensagens");
     *   return EXIT_FAILURE;
     * }
     *
     * context = &msg_context;
     * context_initialized = true;
     */
    errno = ENOSYS;
    perror("[MAIN] Paradigma ainda não implementado");
    return EXIT_FAILURE;

  default:
    errno = EINVAL;
    perror("[MAIN] Paradigma inválido");
    return EXIT_FAILURE;
  }

  WorkerManager manager;
  worker_manager_init(&manager);

  int exit_status = run_command_loop(context, &manager, args->paradigm);

  if (shutdown_requested) {
    printf("\n[MAIN] SIGINT recebido. Iniciando encerramento...\n");
  }

  bool all_workers_stopped = false;

  if (worker_manager_shutdown(&manager, &all_workers_stopped) == -1) {
    perror("[MAIN] Erro durante shutdown");
    exit_status = EXIT_FAILURE;
  }

  if (all_workers_stopped && context_initialized) {
    switch (args->paradigm) {
    case SHARED_MEMORY:
      if (shm_context_destroy(&shm_context) == -1) {
        perror("[MAIN] Erro destruindo contexto SHM");
        exit_status = EXIT_FAILURE;
      }
      break;

    case MESSAGE_EXCHANGE:
      /*
       * msg_context_destroy(&msg_context);
       */
      break;

    default:
      break;
    }
  }

  worker_manager_destroy(&manager);

  if (!all_workers_stopped) {
    return EXIT_FAILURE;
  }

  return exit_status;
}
