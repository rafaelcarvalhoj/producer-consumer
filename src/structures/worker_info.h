#ifndef WORKER_INFO_H
#define WORKER_INFO_H

#include<sys/types.h>
#include <time.h>

typedef struct {
    pid_t pid;
    int ack_semid;
    int operation;
    struct timespec started_at;
} WorkerInfo;


#endif
