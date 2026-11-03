#ifndef WORKER_INFO_H
#define WORKER_INFO_H

#include<sys/types.h>

typedef struct {
    pid_t pid;
    int ack_semid;
} WorkerInfo;


#endif
