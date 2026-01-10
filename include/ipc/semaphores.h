#ifndef IPC_SEMAPHORES_H
#define IPC_SEMAPHORES_H

#include <stddef.h>
#include <sys/sem.h>
#include <signal.h>

/*
 * Lightweight helpers around System V semop/semctl usage.
 *
 * Conventions:
 *  - Functions return 0 on success, -1 on error and set errno like syscalls.
 *  - `semid` is a System V semaphore set id.
 *  - `semnum` is the index of a semaphore within that set.
 *  - `delts` is the sem_op delta (positive to post/increment, negative to wait/decrement).
 */

/* sem_op_retry
 *  - Perform semop(2) with `ops` for `nops` operations.
 *  - Automatically retries if semop fails with EINTR (interrupted by a signal).
 *  - Returns 0 on success, -1 on unrecoverable error.
 */
int sem_op_retry(int semid, struct sembuf * ops, size_t nops);

/* sem_op_intr
 *  - Perform semop(2) but does NOT swallow an EINTR when caller requests cancellation.
 *  - If semop returns EINTR and `stop_flag` is non-NULL and set to non-zero, the call
 *    returns -1 with errno == EINTR so the caller can abort cooperatively.
 *  - If `stop_flag` is NULL the call retries on EINTR (like sem_op_retry).
 *  - Returns 0 on success, -1 on error.
 */
int sem_op_intr(int semid, struct sembuf *ops, size_t nops, volatile sig_atomic_t *stop_flag);

/* sem_lock / sem_unlock
 *  - Convenience wrappers for atomic decrement/increment of a single semaphore
 *    (useful for simple mutex-style locking of shared state).
 *  - sem_lock does a -1 op (wait); sem_unlock does a +1 op (post).
 *  - Return 0 on success, -1 on error.
 */
int sem_lock(int semid, unsigned short semnum);
int sem_lock_intr(int semid, unsigned short semnum, volatile sig_atomic_t *stop_flag);

int sem_unlock(int semid, unsigned short semnum);

/* sem_wait_intr
 *  - Decrement (wait) `semnum` by `delta` with interruption awareness.
 *  - If interrupted by a signal and `stop_flag` is non-NULL and set, the call
 *    returns -1 with errno set to EINTR so the caller can exit cleanly.
 *  - Use delta = -1 for a single wait; positive deltas are permitted per System V.
 *  - Returns 0 on success, -1 on error or cooperative interruption.
 */
int sem_wait_intr(int semid, unsigned short semnum, short delts, volatile sig_atomic_t *stop_flag);

/* sem_post_retry
 *  - Increment (post) `semnum` by `delts`, retrying on EINTR until success.
 *  - Use delts = +1 for a single post.
 *  - Returns 0 on success, -1 on error.
 */
int sem_post_retry(int semid, unsigned short semnum, short delts);

#endif