#ifndef IPC_CONTEXT_H
#define IPC_CONTEXT_H

#include <sys/types.h>
#include "ipc/shared.h"

/* SysV semctl(2) requires this union on some platforms. */
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

/* IPC runtime context carried by processes using the shared world.
 * - shm_id / sem_id: SysV ids for the shared memory and semaphore set.
 * - S: pointer to the attached shm_state_t (or (void*)-1 if not attached).
 * - owner: 1 if this process created the IPC objects (Command Center), 0 otherwise.
 * - ftok_path: path used with ftok(3) to derive keys.
 */
typedef struct {
    int shm_id;
    int sem_id;
    int q_req;
    int q_rep;
    shm_state_t *S;
    int owner;      /* 1 if created by CC */
    char ftok_path[256];
} ipc_ctx_t;

/* Create (or open and reset) shared IPC objects. Typically used by Command Center.
 * - ctx: out param, must be non-NULL.
 * - ftok_path: path used for ftok(3) key generation; file will be created if needed.
 * - On success ctx is initialized, S attached, and semaphores + SHM reset for a fresh run.
 */
int ipc_create(ipc_ctx_t *ctx, const char *ftok_path);

/* Attach to existing IPC objects created by ipc_create.
 * - Returns 0 on success; on failure errno is set.
 */
int ipc_attach(ipc_ctx_t *ctx, const char *ftok_path);

/* Detach from shared memory without removing IPC objects.
 * - Safe to call from both CC and unit processes.
 */
int ipc_detach(ipc_ctx_t *ctx);

/* Remove IPC objects (shared memory + semaphores).
 * - Only the owner/CC should call this when cleaning up.
 */
int ipc_destroy(ipc_ctx_t *ctx);

#endif