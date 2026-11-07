#include "cli/command_parser.h"
#include "cli/startup_args.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "produtor.h"


static void startup_log(const ProgramArguments *args)
{
  puts("Producer-Consumer");
  printf("Producers: %d\n", args->producers);
  printf("Paradigm: %s\n", paradigm_to_str(args->paradigm));
}

int main(int argc, char **argv)
{
  int *msgsIds;
  ProgramArguments *args = load_args(argc, argv);
  if (args == NULL)
  {
    return EXIT_FAILURE;
  }
  startup_log(args);

  ProgramCommand *cmd;
  msgsIds = (int *)malloc(sizeof(int) * args->producers);
  int * begin = &msgsIds[0];
  for (int i = 0; i < args->producers; i++)
  {
    *msgsIds = create_message_exchange(i);
    printf("MsgCriada: %d \n", *msgsIds);
    msgsIds++;
  }
  msgsIds = begin;

  for (;;)
  {

    cmd = next_command();
    if (cmd == NULL)
    {
      free(args);
      return EXIT_FAILURE;
    }

    CommandOperation operation = cmd->op;

    if (operation == SHUTDOWN)
    {
      break;
    }
    if (operation == PRODUCE)
    {
      pid_t pid = fork();
      if (pid == 0)
      {
        if ((cmd->p != 0 && cmd->p <= args->producers))
        {
          for (int i = 0; i < args->producers; i++)
          {
            printf("PID:%d, enviando para a fila %d\n",getpid(),msgsIds[cmd->p -1]);
            send_message(msgsIds[cmd->p -1], getpid(), cmd->n_msgs, cmd->ack);
          }
        }
        else if ((cmd->p == 0))
        {
          for (int i = 0; i < args->producers; i++)
          {
            for(int msgIdsIdx=0;msgIdsIdx< args->producers;msgIdsIdx++){
              printf("processo com PID:%d \n, enviando para a fila %d",getpid(),msgsIds[msgIdsIdx]);
              send_message(msgsIds[msgIdsIdx], getpid(), cmd->n_msgs, cmd->ack);
            }
          }
        }
      }
      else
      {
        continue;
      }
    }
  }
  free(args);
  return EXIT_SUCCESS;
}
