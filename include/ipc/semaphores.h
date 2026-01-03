#ifndef IPC_SEMAPHORES_H
#define IPC_SEMAPHORES_H

#include <stddef.h>
#include <sys/sem.h>
#include <signal.h>

// retrying version of semop
int sem_op_retry(int semid, struct  sembuf * ops, size_t nops);

// non-retrying version (returns -1 if interrupted)
int sem_op_intr(int semid, struct sembuf *ops, size_t nops, volatile sig_atomic_t *stop_flag);

// lock/unlock semaphore
int sem_lock(int semid, unsigned short semnum);
int sem_unlock(int semid, unsigned short semnum);

// wait on semaphore with interrupt checking
int sem_wait_intr(int semid, unsigned short semnum, short delts, volatile sig_atomic_t *stop_flag);
int sem_post_retry(int semid, unsigned short semnum, short delts);

#endif