#ifndef PROGRAM_COMMAND_H
#define PROGRAM_COMMAND_H

#include "unions/command.h"

typedef struct {
    CommandOperation op;
    int p;
    int n_msgs;
    Ack ack;
} ProgramCommand;

#endif
