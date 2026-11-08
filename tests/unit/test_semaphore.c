#define _POSIX_C_SOURCE 200809L

#include "infrastructure/unix/semaphore.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void test_create_and_basic_operations(void)
{
    int sem = semaphore_create(1);
    assert(sem != -1);

    assert(semaphore_p(sem) == 0);
    assert(semaphore_v(sem) == 0);

    assert(semaphore_destroy(sem) == 0);
}

static void test_zero_initial_value(void)
{
    int sem = semaphore_create(0);
    assert(sem != -1);

    assert(semaphore_v(sem) == 0);
    assert(semaphore_p(sem) == 0);

    assert(semaphore_destroy(sem) == 0);
}

static void test_negative_initial_value(void)
{
    errno = 0;

    int sem = semaphore_create(-1);

    assert(sem == -1);
    assert(errno == EINVAL);
}

static void test_p_blocks_until_v(void)
{
    int sem = semaphore_create(0);
    assert(sem != -1);

    int pipefd[2];
    assert(pipe(pipefd) == 0);

    pid_t pid = fork();
    assert(pid != -1);

    if (pid == 0) {
        close(pipefd[0]);

        /*
         * Informa ao pai que o filho está prestes a executar P.
         */
        char ready = 'R';
        if (write(pipefd[1], &ready, 1) != 1) {
            _exit(10);
        }

        if (semaphore_p(sem) == -1) {
            _exit(11);
        }                                                                                       

        char completed = 'D';
        if (write(pipefd[1], &completed, 1) != 1) {
            _exit(12);
        }

        _exit(0);
    }

    close(pipefd[1]);

    char signal_byte;
    assert(read(pipefd[0], &signal_byte, 1) == 1);
    assert(signal_byte == 'R');

    /*
     * Dá ao filho tempo para chegar a P e bloquear.
     */
    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 100 * 1000 * 1000
    };

    while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {
    }

    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);

    /*
     * O filho não deve ter terminado: o semáforo começou em zero.
     */
    assert(result == 0);

    /*
     * V incrementa o semáforo e permite que P prossiga.
     */
    assert(semaphore_v(sem) == 0);

    assert(read(pipefd[0], &signal_byte, 1) == 1);
    assert(signal_byte == 'D');

    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);

    close(pipefd[0]);

    assert(semaphore_destroy(sem) == 0);
}

static void test_destroy_invalidates_semaphore(void)
{
    int sem = semaphore_create(1);
    assert(sem != -1);

    assert(semaphore_destroy(sem) == 0);

    errno = 0;
    assert(semaphore_p(sem) == -1);
    assert(errno == EIDRM || errno == EINVAL);
}

static void test_try_p_nonblocking(void)
{
    errno = 0;
    assert(semaphore_try_p(-1) == -1);
    assert(errno == EINVAL);

    int sem = semaphore_create(1);
    assert(sem != -1);

    assert(semaphore_try_p(sem) == 0);

    errno = 0;
    assert(semaphore_try_p(sem) == -1);
    assert(errno == EAGAIN);

    assert(semaphore_v(sem) == 0);
    assert(semaphore_try_p(sem) == 0);

    assert(semaphore_destroy(sem) == 0);
}

static void test_ack_semaphore_lifecycle_and_wait_n(void)
{
    assert(ack_semaphore_signal(-1) == 0);
    assert(ack_semaphore_destroy(-1) == 0);

    errno = 0;
    assert(ack_semaphore_wait_n(-1, 1) == -1);
    assert(errno == EINVAL);

    int sem = ack_semaphore_create();
    assert(sem != -1);

    assert(ack_semaphore_wait_n(sem, 0) == 0);

    errno = 0;
    assert(ack_semaphore_wait_n(sem, (unsigned int)SHRT_MAX + 1u) == -1);
    assert(errno == EINVAL);

    int pipefd[2];
    assert(pipe(pipefd) == 0);

    pid_t pid = fork();
    assert(pid != -1);

    if (pid == 0) {
        close(pipefd[0]);
        char ready = 'R';
        if (write(pipefd[1], &ready, 1) != 1) {
            _exit(10);
        }
        if (ack_semaphore_wait_n(sem, 3) == -1) {
            _exit(11);
        }
        char done = 'D';
        if (write(pipefd[1], &done, 1) != 1) {
            _exit(12);
        }
        _exit(0);
    }

    close(pipefd[1]);

    char byte = 0;
    assert(read(pipefd[0], &byte, 1) == 1);
    assert(byte == 'R');

    struct timespec delay = {.tv_sec = 0, .tv_nsec = 50 * 1000 * 1000};
    assert(ack_semaphore_signal(sem) == 0);
    assert(ack_semaphore_signal(sem) == 0);
    while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {
    }

    int status = 0;
    assert(waitpid(pid, &status, WNOHANG) == 0);

    assert(ack_semaphore_signal(sem) == 0);
    assert(read(pipefd[0], &byte, 1) == 1);
    assert(byte == 'D');

    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    close(pipefd[0]);
    assert(ack_semaphore_destroy(sem) == 0);
}

void test_semaphore(void)
{
    test_create_and_basic_operations();
    puts("[OK] Criacao e operacoes basicas");

    test_zero_initial_value();
    puts("[OK] Semaforo iniciado em zero");

    test_negative_initial_value();
    puts("[OK] Valor inicial negativo rejeitado");

    test_p_blocks_until_v();
    puts("[OK] P bloqueia ate V");

    test_destroy_invalidates_semaphore();
    puts("[OK] Remocao do semaforo");

    test_try_p_nonblocking();
    puts("[OK] semaphore_try_p nao bloqueante");

    test_ack_semaphore_lifecycle_and_wait_n();
    puts("[OK] Semaforo de ACK e wait_n");

    puts("Todos os testes de semaforo passaram.");
}
