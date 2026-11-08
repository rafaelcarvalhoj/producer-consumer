#ifndef SEQUENTIAL_H
#define SEQUENTIAL_H

#include <stdint.h>

typedef struct {
  int idshm;
  int idsem;
  uint32_t *next_seq;
} GlobalSequential;

int sequential_init(GlobalSequential *sequenciador);

int sequential_destroy(GlobalSequential *sequenciador);

int sequential_next(GlobalSequential *sequenciador, uint32_t *sequence);

#endif // SEQUENTIAL_H
