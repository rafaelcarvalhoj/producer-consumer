#include "application/runtime.h"
#include "cli/startup_args.h"

#include <stdlib.h>

int main(int argc, char **argv) {
  ProgramArguments *args = load_args(argc, argv);

  if (args == NULL) {
    return EXIT_FAILURE;
  }

  int result = application_run(args);

  free(args);

  return result;
}
