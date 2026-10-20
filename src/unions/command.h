#ifndef COMMAND_UNIONS_H
#define COMMAND_UNIONS_H

typedef enum {
    PRODUCE,
    CONSUME,
    SHUTDOWN
} CommandOperation;

typedef enum {
    YES = 1,
    NO = 2
} Ack;

#endif
