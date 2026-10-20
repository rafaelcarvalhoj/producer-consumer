#ifndef CLI_COMMAND_PARSER_H
#define CLI_COMMAND_PARSER_H

#include "structures/program_command.h"

/* Returns NULL for an invalid line, end of input, or an allocation failure. */
ProgramCommand *next_command(void);

void free_command(ProgramCommand *command);

#endif
