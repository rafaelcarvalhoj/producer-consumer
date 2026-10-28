#ifndef INIT_H
#define INIT_H

#include "structures/worker_info.h"

WorkerInfo * setup_producers(int n_workers);

void free_workers(WorkerInfo *workers);

#endif
