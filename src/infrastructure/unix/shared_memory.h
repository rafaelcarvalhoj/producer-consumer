
#ifndef UNIX_SHARED_MEMORY_H
#define UNIX_SHARED_MEMORY_H

#include <stddef.h>

/* Cria um segmento System V e retorna seu ID, ou -1 em caso de erro. */
int shm_create(size_t size);

/* Retorna o endereço anexado ou NULL em caso de erro. */
void *shm_attach(int shmid);

/* Retorna 0 em sucesso ou -1 em caso de erro. */
int shm_detach(const void *address);

/* Marca o segmento para remoção. */
int shm_remove(int shmid);

#endif
