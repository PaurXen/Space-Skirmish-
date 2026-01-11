#define _GNU_SOURCE
#include "ipc/semaphores.h"
#include <errno.h>

/*
 * Small, focused wrappers around System V semop/semctl.
 *
 * Conventions:
 *  - Return 0 on success, -1 on error and set errno (like syscalls).
 *  - semid: System V semaphore set id.
 *  - semnum: index of a semaphore inside the set.
 *  - delta/delts: sem_op change (positive = post, negative = wait).
 *
 * Purpose:
 *  - Centralize EINTR handling and provide a cooperative-cancellation-aware
 *    semop variant used by the tick/barrier logic.
 */

/* sem_op_retry
 *  Perform semop(2) with the provided ops array.
 *  Retries on EINTR until the operation succeeds or fails with a
 *  non-recoverable error. Suitable when the caller does not need
 *  cooperative cancellation.
 */
int sem_op_retry(int semid, struct sembuf * ops, size_t nops) {
    for (;;) {
        if (semop(semid, ops, (unsigned)nops) == 0) return 0;
        if (errno == EINTR) continue;   // interrupted by signal, retry
        return -1;
    }
}

/* sem_op_intr
 *  Perform semop(2) but allow cooperative cancellation via stop_flag.
 *   - If semop returns EINTR and stop_flag != NULL and *stop_flag != 0,
 *     the function returns -1 with errno == EINTR so the caller can abort.
 *   - If stop_flag == NULL this behaves like sem_op_retry (retries on EINTR).
 */
int sem_op_intr(int semid, struct sembuf *ops, size_t nops, volatile sig_atomic_t *stop_flag) {
    for (;;) {
        /* CRITICAL: Check stop_flag BEFORE blocking call! */
        if (stop_flag && *stop_flag) {
            errno = EINTR;
            return -1;
        }
        
        if (semop(semid, ops, (unsigned)nops) == 0) return 0;
        
        if (errno == EINTR) {
            if (stop_flag && *stop_flag) {
                errno = EINTR;
                return -1;
            }
            continue;   // interrupted by signal, retry
        }
        return -1;
    }
}

/* sem_lock / sem_unlock
 *  Convenience single-semaphore helpers:
 *   - sem_lock: decrement (wait) semaphore[semnum] by 1.
 *   - sem_unlock: increment (post) semaphore[semnum] by 1.
 *  These use sem_op_retry (uninterruptible from caller's POV).
 */
int sem_lock(int semid, unsigned short semnum) {
    struct sembuf op = {.sem_num=semnum, .sem_op=-1, .sem_flg=0};
    return sem_op_retry(semid, &op, 1);
}

int sem_lock_intr(int semid, unsigned short semnum, volatile sig_atomic_t *stop_flag) {
    struct sembuf op = {.sem_num=semnum, .sem_op=-1, .sem_flg=0};
    return sem_op_intr(semid, &op, 1, stop_flag);
}


int sem_unlock(int semid , unsigned short semnum) {
    struct sembuf op = {.sem_num=semnum, .sem_op=+1, .sem_flg=0};
    return sem_op_retry(semid, &op, 1);
}

/* sem_wait_intr
 *  Wait/decrement a single semaphore with cooperative-interrupt support.
 *  delta: sem_op value (use -1 for a single wait).
 *  stop_flag: if non-NULL and set, interruption causes return with errno==EINTR.
 */
int sem_wait_intr(int semid, unsigned short semnum, short delta, volatile sig_atomic_t *stop_flag) {
    struct sembuf op = {.sem_num=semnum, .sem_op=delta, .sem_flg=0};
    return sem_op_intr(semid, &op, 1, stop_flag);
}

/* sem_post_retry
 *  Post/increment a single semaphore by delts, retrying on EINTR until success.
 */
int sem_post_retry(int semid, unsigned short semnum, short delta) {
    struct sembuf op = {.sem_num=semnum, .sem_op=delta, .sem_flg=0};
    return sem_op_retry(semid, &op, 1);
}