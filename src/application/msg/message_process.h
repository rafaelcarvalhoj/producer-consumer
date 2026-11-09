#ifndef MESSAGE_PROCESS_H
#define MESSAGE_PROCESS_H

#include "application/msg/message_context.h"
#include "structures/program_command.h"

#include <sys/types.h>

int msg_process(MessageContext *context, const ProgramCommand *cmd,
                pid_t *child_pid, int *ack_semid);

#endif
