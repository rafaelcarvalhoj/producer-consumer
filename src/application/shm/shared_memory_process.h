
#ifndef SHARED_MEMORY_PROCESS_H
#define SHARED_MEMORY_PROCESS_H

#include "application/shm/shared_memory_context.h"
#include "structures/program_command.h"

int shm_process(SharedMemoryContext *context, const ProgramCommand *cmd,
                pid_t *child_pid);

#endif
