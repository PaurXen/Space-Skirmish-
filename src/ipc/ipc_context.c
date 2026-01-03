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


static key_t make_key(const char *path, int proj_id) {
    key_t k = ftok(path, proj_id);
    return k;   // caller checks -1
}

static int esure_ftok_file(const char *path) {
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    fclose(f);
    return 0;
}

static int init_if_needed(ipc_ctx_t *ctx) {
    // called by craetor after attach: initialize SHM
    if (sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK) == -1) return -1;

    if (ctx->S->magic != SHM_MAGIC) {
        memset(ctx->S, 0, sizeof(*(ctx->S)));
        ctx->S->magic = SHM_MAGIC;
        ctx->S->next_unit_id = 1;
        ctx->S->ticks = 0;
        ctx->S->unit_count = 0;
    }

    if (sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK) == -1) return -1;
    return 0;
}

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

    // sanity check
    if (ctx->S->magic != SHM_MAGIC) {
        errno = EPROTO;
        return -1;
    }

    return 0;
}

int ipc_detach(ipc_ctx_t *ctx) {
    int ok = 0;
    if (ctx->S && ctx->S != (void*)-1) {
        if (shmdt(ctx->S) == -1) ok = -1;
        ctx->S = (void*)-1;
    }
    return ok;
}

int ipc_destroy(ipc_ctx_t* ctx) {
    // remove objects (CC only)
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
