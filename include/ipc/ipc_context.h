#ifndef IPC_CONTEXT_H
#define IPC_CONTEXT_H

#include <sys/types.h>
#include "ipc/shared.h"

// SysV semctl union
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

typedef struct {
    int shm_id;
    int sem_id;
    shm_state_t *S;
    int owner;      // 1 if created by CC
    char ftok_path[256];
} ipc_ctx_t;

// comand center (CC): creates or reuse
int ipc_create(ipc_ctx_t *ctx, const char *ftok_path);

// unit processes: attach to existing
int ipc_attach(ipc_ctx_t *ctx, const char *ftok_path);

// detach from SHM (does not remove IPC objects)
int ipc_detach(ipc_ctx_t *ctx);

// remove IPC objects (only by CC)
int ipc_destroy(ipc_ctx_t *ctx);

#endif