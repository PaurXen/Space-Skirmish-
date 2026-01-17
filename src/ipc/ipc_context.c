#define _GNU_SOURCE
#include "ipc/ipc_context.h"
#include "ipc/semaphores.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

/*
 * IPC helper: create/attach/destroy SysV shared memory + semaphore set
 *
 * Overview:
 *  - ipc_create(): create (or open) and initialize a fresh shared-state run.
 *    Creates the ftok file if missing, obtains keys via ftok, creates semaphores
 *    and shared memory, attaches, and resets shared state under SEM_GLOBAL_LOCK.
 *
 *  - ipc_attach(): attach to existing objects created by ipc_create. Performs
 *    basic sanity checking (magic) and returns -1 with errno set on failure.
 *
 *  - ipc_detach(): detach the shared memory mapping for this process.
 *
 *  - ipc_destroy(): remove the SysV objects (shmctl IPC_RMID, semctl IPC_RMID).
 *
 * Notes / Conventions:
 *  - All functions return 0 on success, -1 on failure and set errno like syscalls.
 *  - Caller is responsible for calling ipc_destroy only if it is the owner/CC.
 *  - Shared state is protected by SEM_GLOBAL_LOCK where required; ipc_create
 *    resets the shared memory contents under that lock so a fresh run starts
 *    with predictable values.
 *  - ftok project ids are single characters: 'S' for shared memory, 'M' for semaphores.
 */

static key_t make_key(const char *path, int proj_id) {
    key_t k = ftok(path, proj_id);
    return k;   // caller checks -1
}

/* Ensure the ftok key file exists (create if needed). Returns 0 on success. */
static int esure_ftok_file(const char *path) {
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    fclose(f);
    return 0;
}

/* Initialize shm contents if they are not already valid.
 * This is called by the creator after attaching; it acquires SEM_GLOBAL_LOCK
 * to perform a safe reset. On first-run we set magic, next_unit_id and zero
 * sensible counters. Returns 0 on success, -1 on error (errno set).
 */
// static int init_if_needed(ipc_ctx_t *ctx) {
//     /* acquire global lock to safely inspect/modify shared state */
//     if (sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK) == -1) return -1;

//     if (ctx->S->magic != SHM_MAGIC) {
//         memset(ctx->S, 0, sizeof(*(ctx->S)));
//         ctx->S->magic = SHM_MAGIC;
//         ctx->S->next_unit_id = 1;
//         ctx->S->ticks = 0;
//         ctx->S->unit_count = 0;
//     }

//     if (sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK) == -1) return -1;
//     return 0;
// }

/* ipc_create
 *  - Prepare ctx and create/reset IPC objects for a fresh run.
 *  - Ensures the ftok file exists, obtains keys, creates semaphores and SHM.
 *  - Resets semaphore values (SETALL) and clears the shared segment under lock.
 *  - On success ctx is initialized, ctx->S attached and ready.
 */
int ipc_create(ipc_ctx_t *ctx, const char *ftok_path) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->shm_id = -1;
    ctx->sem_id = -1;
    ctx->S = (void*)-1;
    ctx->owner = 1;
    strncpy(ctx->ftok_path, ftok_path, sizeof(ctx->ftok_path)-1);

    if (esure_ftok_file(ftok_path) == -1) return -1;

    key_t shm_key = make_key(ftok_path, 'S');
    key_t sem_key = make_key(ftok_path, 'M');
    if (shm_key == -1 || sem_key == -1) return -1;

    // 1) Semaphores: create-or-open, then RESET ALWAYS for fresh run
    ctx->sem_id = semget(sem_key, SEM_COUNT, IPC_CREAT | 0600);
    if (ctx->sem_id == -1) return -1;

    union semun u;
    unsigned short vals[SEM_COUNT];
    vals[SEM_GLOBAL_LOCK] = 1;
    vals[SEM_TICK_START]  = 0;
    vals[SEM_TICK_DONE]   = 0;
    u.array = vals;
    if (semctl(ctx->sem_id, 0, SETALL, u) == -1) return -1;

    // 2) SHM: create-or-open, attach, RESET ALWAYS for fresh run
    ctx->shm_id = shmget(shm_key, sizeof(shm_state_t), IPC_CREAT | 0600);
    if (ctx->shm_id == -1) return -1;

    ctx->S = (shm_state_t*)shmat(ctx->shm_id, NULL, 0);
    if (ctx->S == (void*)-1) return -1;

    // safe reset under lock (now sem exists)
    sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
    memset(ctx->S, 0, sizeof(*ctx->S));
    ctx->S->magic = SHM_MAGIC;
    ctx->S->next_unit_id = 1;
    sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);

    return 0;
}

/* ipc_attach
 *  - Attach to existing IPC objects using ftok-derived keys.
 *  - Does not modify shared memory; verifies magic and returns -1 with errno
 *    set if the segment is not initialized correctly.
 */
int ipc_attach(ipc_ctx_t *ctx, const char *ftok_path) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->shm_id = -1;
    ctx->sem_id = -1;
    ctx->S = (void*)-1;
    ctx->owner = 0;

    strncpy(ctx->ftok_path, ftok_path, sizeof(ctx->ftok_path)-1);

    key_t shm_key = make_key(ftok_path, 'S');
    key_t sem_key = make_key(ftok_path, 'M');
    if (shm_key == -1 || sem_key == -1) return -1;

    ctx->shm_id = shmget(shm_key, sizeof(shm_state_t), 0600);
    if (ctx->shm_id == -1) return -1;

    ctx->S = (shm_state_t*)shmat(ctx->shm_id, NULL, 0);
    if (ctx->S == (void*)-1) return -1;

    ctx->sem_id = semget(sem_key, SEM_COUNT, 0600);
    if (ctx->sem_id == -1) return -1;

    // sanity check: ensure the creator initialized the segment
    if (ctx->S->magic != SHM_MAGIC) {
        errno = EPROTO;
        return -1;
    }

    return 0;
}

/* ipc_detach
 *  - Detach the shared memory mapping for this process.
 *  - Returns 0 on success, -1 on failure (errno set by shmdt).
 */
int ipc_detach(ipc_ctx_t *ctx) {
    int ok = 0;
    if (ctx->S && ctx->S != (void*)-1) {
        if (shmdt(ctx->S) == -1) ok = -1;
        ctx->S = (void*)-1;
    }
    return ok;
}

/* ipc_destroy
 *  - Remove the SysV shared memory and semaphore objects (IPC_RMID).
 *  - Intended to be called by the owner (Command Center) during cleanup.
 *  - Returns 0 on success, -1 if any removal failed.
 */
int ipc_destroy(ipc_ctx_t* ctx) {
    int ok =0;

    if (ctx->shm_id != -1) {
            if (shmctl(ctx->shm_id, IPC_RMID, NULL) == -1) ok = -1;
            ctx->shm_id = -1;
    }
    if (ctx->sem_id != -1) {
            if (semctl(ctx->sem_id, 0, IPC_RMID) == -1) ok = -1;
            ctx->sem_id = -1;
    }
    return ok;
}
