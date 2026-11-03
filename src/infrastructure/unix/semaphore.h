
#ifndef UNIX_SEMAPHORE_H
#define UNIX_SEMAPHORE_H

int semaphore_create(int initial_value);

int semaphore_p(int semaphore_id);

int semaphore_try_p(int semaphore_id);

int semaphore_v(int semaphore_id);

int semaphore_destroy(int semaphore_id);

int ack_semaphore_create(void);

int ack_semaphore_signal(int semid);

int ack_semaphore_wait_n(int semid, unsigned int amount);

int ack_semaphore_destroy(int semid);

#endif
