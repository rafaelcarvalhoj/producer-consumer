#include "cli/command_parser.h"
#include "cli/startup_args.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static void startup_log(const ProgramArguments *args)
{
  puts("Producer-Consumer");
  printf("Producers: %d\n", args->producers);
  printf("Paradigm: %s\n", paradigm_to_str(args->paradigm));
}

int main(int argc, char **argv)
{
  ProgramArguments *args = load_args(argc, argv);
  if (args == NULL)
  {
    return EXIT_FAILURE;
  }

  startup_log(args);
  printf("esse aqui é o escolhido %d", args->paradigm);
  ProgramCommand *cmd;
  __pid_t pid = fork();
  if (pid > 0)
  {
    if (args->paradigm == MESSAGE_EXCHANGE)
    {
      printf("fazendo um fork pra executar fila de msgs\n");
      execl("./build/ipc/message_queue/message_queue",
            "message_queue",
            (char *)NULL);
    }
    if (args->paradigm == MESSAGE_EXCHANGE)
    {
      printf("fazendo um fork pra executar memória compartilhada\n");
      // execl("./build/ipc/message_queue/message_queue",
      //       "message_queue",
      //       (char *)NULL);
    }
  }
  
  // for (;;) {
  //   cmd = next_command();
  //   if (cmd == NULL) {
  //     free(args);
  //     return EXIT_FAILURE;
  //   }

  //   CommandOperation operation = cmd->op;
  //   free_command(cmd);
  //   if (operation == SHUTDOWN) {
  //     break;
  //   }
  // }
  // free(args);
  return EXIT_SUCCESS;
}
