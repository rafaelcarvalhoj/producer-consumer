#ifndef PROGRAM_ARGUMENTS_H
#define PROGRAM_ARGUMENTS_H

#include "unions/paradigm.h"

typedef struct {
    int producers;
    ProducerConsumerParadigm paradigm;
} ProgramArguments;

#endif
