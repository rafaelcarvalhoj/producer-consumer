
#include "infrastructure/unix/shared_memory.h"

#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int shm_create(size_t size) {
  if (size == 0) {
    errno = EINVAL;
    return -1;
  }

  /*
   * IPC_PRIVATE garante a criação de um novo segmento.
   * O ID retornado pode ser utilizado pelos processos filhos.
   *
   * 0600: leitura e escrita apenas pelo owner.
   */
  return shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);
}

void *shm_attach(int shmid) {
  if (shmid < 0) {
    errno = EINVAL;
    return NULL;
  }

  void *address = shmat(shmid, NULL, 0);

  /*
   * A API shmat() retorna (void *) -1 em caso de erro.
   * Esta interface converte esse retorno para NULL,
   * preservando errno para diagnóstico.
   */
  if (address == (void *)-1) {
    return NULL;
  }

  return address;
}

int shm_detach(const void *address) {
  if (address == NULL) {
    errno = EINVAL;
    return -1;
  }

  return shmdt(address);
}

int shm_remove(int shmid) {
  if (shmid < 0) {
    errno = EINVAL;
    return -1;
  }

  return shmctl(shmid, IPC_RMID, NULL);
}
