#include "cli/command_parser.h"
#include "cli/startup_args.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int fd_pipes[][2];

static void startup_log(const ProgramArguments *args)
{
  puts("Producer-Consumer");
  printf("Producers: %d\n", args->producers);
  printf("Paradigm: %s\n", paradigm_to_str(args->paradigm));
}

static void alloc_pipes(int n, int fd_pipes[][2])
{
  for (int i = 0; i < n; i++)
  {
    pipe(fd_pipes[i]);
  }
}

int main(int argc, char **argv)
{
  ProgramArguments *args = load_args(argc, argv);
  if (args == NULL)
  {
    return EXIT_FAILURE;
  }
  startup_log(args);

  ProgramCommand *cmd;
  int (*fd_pipes)[2] = malloc(sizeof *fd_pipes * args->producers);
  alloc_pipes(args->producers, fd_pipes);

  for (int i = 0; i < args->producers; i++)
  {
    int pid = fork();
    // Aqui é se o fork for um filho.
    if (pid == 0)
    {
      int msg;
      for (;;)
      {
        close(fd_pipes[i][1]);
        int n = read(fd_pipes[i][0], &msg, sizeof(int));
        printf("eu recebi msg no produtor %d %d\n", i+1, msg);
      }
    }
  }

  int count = 0;
  for (;;)
  {

    cmd = next_command();
    if (cmd == NULL)
    {
      printf("dei problema aqui");
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
      int aba = 1;
      if ((int)cmd->p == 0)
      {
        for (int i = 0; i < args->producers; i++)
        {
          printf("estou indo escrever no %d\n",i+1);
          write(fd_pipes[i][1], &aba, sizeof(aba));
        }
      }
      else
      {
        printf("to aqui escrevendo no correto? %d\n", cmd->p);
        write(fd_pipes[cmd->p-1][1], &aba, sizeof(aba));
      }
    }
  }
  // free(args);
  // return EXIT_SUCCESS;
}
