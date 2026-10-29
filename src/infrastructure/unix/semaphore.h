
#ifndef UNIX_SEMAPHORE_H
#define UNIX_SEMAPHORE_H

int semaphore_create(int initial_value);

int semaphore_p(int semaphore_id);

int semaphore_v(int semaphore_id);

int semaphore_destroy(int semaphore_id);

#endif
