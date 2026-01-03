#define _GNU_SOURCE
#include "ipc/semaphores.h"
#include <errno.h>

int sem_op_retry(int semid, struct sembuf * ops, size_t nops) {
    for (;;) {
        if (semop(semid, ops, (unsigned)nops) == 0) return 0;
        if (errno == EINTR) continue;   // iterupted by signal, retry
        return -1;
    }
}

int sem_op_intr(int semid, struct sembuf *ops, size_t nops, volatile sig_atomic_t *stop_flag) {

    for (;;) {
        if (semop(semid, ops, (unsigned)nops) == 0) return 0;
        if (errno == EINTR) {
            if (stop_flag && *stop_flag) {
                errno = EINTR;
                return -1;
            }
            continue;   // iterupted by signal, retry
        }
        return -1;
    }
}

int sem_lock(int semid, unsigned short semnum) {
    struct sembuf op = {.sem_num=semnum, .sem_op=-1, .sem_flg=0};
    return sem_op_retry(semid, &op, 1);
}

int sem_unlock(int semid , unsigned short semnum) {
    struct sembuf op = {.sem_num=semnum, .sem_op=+1, .sem_flg=0};
    return sem_op_retry(semid, &op, 1);
}

int sem_wait_intr(int semid, unsigned short semnum, short delta, volatile sig_atomic_t *stop_flag) {
    struct sembuf op = {.sem_num=semnum, .sem_op=delta, .sem_flg=0};
    return sem_op_intr(semid, &op, 1, stop_flag);
}

int sem_post_retry(int semid, unsigned short semnum, short delta) {
    struct sembuf op = {.sem_num=semnum, .sem_op=delta, .sem_flg=0};
    return sem_op_retry(semid, &op, 1);
}