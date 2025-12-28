#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>

#include "ipc/shared.h"

static int shm_id = -1;
static int sem_id = -1;
static shm_state_t *S = NULL;
static volatile sig_atomic_t g_stop = 0;

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

// SysV semctl union
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// sem_op and global lock/unlock
static void sem_op(int semid, unsigned short semnum, short delta) {
    struct sembuf op = {
        .sem_num = semnum,
        .sem_op = delta,
        .sem_flg = 0
    };
    if (semop(semid, &op, 1) == -1) die("semop");
}
static void lock_global(void){ sem_op(sem_id, SEM_GLOBAL_LOCK, -1); }
static void unlock_global(void){ sem_op(sem_id,  SEM_GLOBAL_LOCK, +1); }

// Cleanup IPC resources
static void cleanup_ipc(void) {
    if (S && S != (void*)-1) shmdt(S);
    S = NULL;

    if (shm_id != -1) {
        shmctl(shm_id, IPC_RMID, NULL);
        shm_id = -1;
    }
    if (sem_id != -1) {
        semctl(sem_id, 0, IPC_RMID);
        sem_id = -1;
    }
}

// Generate IPC key
static key_t make_key(const char *path, int proj_id) {
    key_t key = ftok(path, proj_id);
    if (key == -1) die("ftok");
    return key;
}

static void init_shm_and_sems(const char *ftok_path) {
    
}
