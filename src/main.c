#include "application/init.h"
#include "application/shared_memory_paradigm.h"
#include "cli/command_parser.h"
#include "cli/startup_args.h"
#include "unions/paradigm.h"
#include <stdio.h>
#include <stdlib.h>

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

  for (;;) {
    cmd = next_command();
    if (cmd == NULL) {
      free(args);
      return EXIT_FAILURE;
    }

    CommandOperation operation = cmd->op;
    free_command(cmd);
    if (operation == SHUTDOWN) {
      break;
    } else if (operation == PRODUCE) {
      // produz(producers_table, paradigm, cmd);
    } else if (operation == CONSUME) {
      // consome(producers_table, paradigm, cmd);
    }
  }
  free_workers(producers_table);
  free(args);
  return EXIT_SUCCESS;
}
