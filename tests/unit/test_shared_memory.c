
#include "infrastructure/unix/shared_memory.h"

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/*
 * Mocks das chamadas ao sistema.
 * O linker redirecionará as chamadas feitas por shared_memory.c
 * para estas funções.
 */

extern int __real_shmget(key_t key, size_t size, int shmflg);
extern void *__real_shmat(int shmid, const void *shmaddr, int shmflg);
extern int __real_shmdt(const void *shmaddr);
extern int __real_shmctl(int shmid, int cmd, struct shmid_ds *buf);

static int mocks_active = 0;

static int shmget_calls;
static key_t shmget_key;
static size_t shmget_size;
static int shmget_flags;
static int shmget_result;
static int shmget_error;

static int shmat_calls;
static int shmat_shmid;
static const void *shmat_address;
static int shmat_flags;
static void *shmat_result;
static int shmat_error;

static int shmdt_calls;
static const void *shmdt_address;
static int shmdt_result;
static int shmdt_error;

static int shmctl_calls;
static int shmctl_shmid;
static int shmctl_command;
static struct shmid_ds *shmctl_buffer;
static int shmctl_result;
static int shmctl_error;

static unsigned char fake_segment[2048];

static void reset_mocks(void) {
  mocks_active = 1;
  shmget_calls = 0;
  shmget_key = 0;
  shmget_size = 0;
  shmget_flags = 0;
  shmget_result = 42;
  shmget_error = 0;

  shmat_calls = 0;
  shmat_shmid = -1;
  shmat_address = NULL;
  shmat_flags = 0;
  shmat_result = fake_segment;
  shmat_error = 0;

  shmdt_calls = 0;
  shmdt_address = NULL;
  shmdt_result = 0;
  shmdt_error = 0;

  shmctl_calls = 0;
  shmctl_shmid = -1;
  shmctl_command = -1;
  shmctl_buffer = NULL;
  shmctl_result = 0;
  shmctl_error = 0;

  errno = 0;
}

/* Substitui shmget(). */
int __wrap_shmget(key_t key, size_t size, int shmflg) {
  if (!mocks_active) {
    return __real_shmget(key, size, shmflg);
  }

  ++shmget_calls;

  shmget_key = key;
  shmget_size = size;
  shmget_flags = shmflg;

  if (shmget_result == -1) {
    errno = shmget_error;
  }

  return shmget_result;
}

/* Substitui shmat(). */
void *__wrap_shmat(int shmid, const void *shmaddr, int shmflg) {
  if (!mocks_active) {
    return __real_shmat(shmid, shmaddr, shmflg);
  }

  ++shmat_calls;

  shmat_shmid = shmid;
  shmat_address = shmaddr;
  shmat_flags = shmflg;

  if (shmat_result == (void *)-1) {
    errno = shmat_error;
  }

  return shmat_result;
}

/* Substitui shmdt(). */
int __wrap_shmdt(const void *shmaddr) {
  if (!mocks_active) {
    return __real_shmdt(shmaddr);
  }

  ++shmdt_calls;
  shmdt_address = shmaddr;

  if (shmdt_result == -1) {
    errno = shmdt_error;
  }

  return shmdt_result;
}

/* Substitui shmctl(). */
int __wrap_shmctl(int shmid, int cmd, struct shmid_ds *buf) {
  if (!mocks_active) {
    return __real_shmctl(shmid, cmd, buf);
  }

  ++shmctl_calls;

  shmctl_shmid = shmid;
  shmctl_command = cmd;
  shmctl_buffer = buf;

  if (shmctl_result == -1) {
    errno = shmctl_error;
  }

  return shmctl_result;
}

/* ---------------------------------------------------------- */
/* Testes de unix_shm_create()                                */
/* ---------------------------------------------------------- */

static void test_create_passes_size_and_permissions(void) {
  reset_mocks();

  int shmid = shm_create(2048);

  assert(shmid == 42);
  assert(shmget_calls == 1);
  assert(shmget_key == IPC_PRIVATE);
  assert(shmget_size == 2048);
  assert(shmget_flags == (0600));
}

static void test_create_rejects_zero_size(void) {
  reset_mocks();

  int shmid = shm_create(0);

  assert(shmid == -1);
  assert(errno == EINVAL);
  assert(shmget_calls == 0);
}

static void test_create_propagates_system_error(void) {
  reset_mocks();

  shmget_result = -1;
  shmget_error = ENOSPC;

  int shmid = shm_create(2048);

  assert(shmid == -1);
  assert(shmget_calls == 1);
  assert(errno == ENOSPC);
}

/* ---------------------------------------------------------- */
/* Testes de unix_shm_attach()                                */
/* ---------------------------------------------------------- */

static void test_attach_returns_address(void) {
  reset_mocks();

  void *address = shm_attach(42);

  assert(address == fake_segment);
  assert(shmat_calls == 1);
  assert(shmat_shmid == 42);
  assert(shmat_address == NULL);
  assert(shmat_flags == 0);
}

static void test_attach_rejects_invalid_id(void) {
  reset_mocks();

  void *address = shm_attach(-1);

  assert(address == NULL);
  assert(errno == EINVAL);
  assert(shmat_calls == 0);
}

static void test_attach_converts_system_error_to_null(void) {
  reset_mocks();

  shmat_result = (void *)-1;
  shmat_error = ENOMEM;

  void *address = shm_attach(42);

  assert(address == NULL);
  assert(shmat_calls == 1);
  assert(errno == ENOMEM);
}

/* ---------------------------------------------------------- */
/* Testes de unix_shm_detach()                                */
/* ---------------------------------------------------------- */

static void test_detach_passes_address(void) {
  reset_mocks();

  const void *address = fake_segment;

  int result = shm_detach(address);

  assert(result == 0);
  assert(shmdt_calls == 1);
  assert(shmdt_address == address);
}

static void test_detach_rejects_null(void) {
  reset_mocks();

  int result = shm_detach(NULL);

  assert(result == -1);
  assert(errno == EINVAL);
  assert(shmdt_calls == 0);
}

static void test_detach_propagates_system_error(void) {
  reset_mocks();

  shmdt_result = -1;
  shmdt_error = EINVAL;

  int result = shm_detach(fake_segment);

  assert(result == -1);
  assert(shmdt_calls == 1);
  assert(errno == EINVAL);
}

/* ---------------------------------------------------------- */
/* Testes de unix_shm_remove()                                */
/* ---------------------------------------------------------- */

static void test_remove_marks_segment_for_removal(void) {
  reset_mocks();

  int result = shm_remove(42);

  assert(result == 0);
  assert(shmctl_calls == 1);
  assert(shmctl_shmid == 42);
  assert(shmctl_command == IPC_RMID);
  assert(shmctl_buffer == NULL);
}

static void test_remove_rejects_invalid_id(void) {
  reset_mocks();

  int result = shm_remove(-1);

  assert(result == -1);
  assert(errno == EINVAL);
  assert(shmctl_calls == 0);
}

static void test_remove_propagates_system_error(void) {
  reset_mocks();

  shmctl_result = -1;
  shmctl_error = EIDRM;

  int result = shm_remove(42);

  assert(result == -1);
  assert(shmctl_calls == 1);
  assert(errno == EIDRM);
}

void test_shared_memory(void) {
  test_create_passes_size_and_permissions();
  test_create_rejects_zero_size();
  test_create_propagates_system_error();

  test_attach_returns_address();
  test_attach_rejects_invalid_id();
  test_attach_converts_system_error_to_null();

  test_detach_passes_address();
  test_detach_rejects_null();
  test_detach_propagates_system_error();

  test_remove_marks_segment_for_removal();
  test_remove_rejects_invalid_id();
  test_remove_propagates_system_error();

  mocks_active = 0;
}
