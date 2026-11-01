#include "application/init.h"
#include <stdlib.h>
#include <stdio.h>

WorkerInfo * setup_producers(int n_workers) {

    WorkerInfo * producers = malloc(sizeof(WorkerInfo)*n_workers);

    if (producers == NULL) {
        fputs("Error: Failed to alloc producers table.\n", stderr);
        return NULL;
    }

    for (int i = 0; i < n_workers; i++){
        producers[i].pid = i;
        printf("[TODO] Implementar função que inicia os produtores\n");
    }

    return producers;
}

void free_workers(WorkerInfo *workers) {
    free(workers);
}
