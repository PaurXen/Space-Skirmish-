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

#include "ipc/shared.h"

static int shm_id = -1;
static int sem_id = -1;
static shm_state_t *S = NULL;
static volatile sig_atomic_t g_stop = 0;

static void die(const char *msg) { perror(msg); exit(EXIT_FAILURE); }

union semun { int val; struct semid_ds *buf; unsigned short *array; };

static void sem_op(int semid, unsigned short semnum, short delta) {
    struct sembuf op = {.sem_num=semnum, .sem_op=delta, .sem_flg=0};
    if (semop(semid, &op, 1) == -1) die("semop");
}

static int sem_op_intr(int semid, unsigned short semnum, short delta) {
    struct sembuf op = {.sem_num = semnum, .sem_op = delta, .sem_flg = 0};

    while (1) {
        if (semop(semid, &op, 1) == 0) return 0;     // success
        if (errno == EINTR) {
            if (g_stop) return -1;                   // interrupted and we want to exit
            continue;                                 // interrupted but keep waiting
        }
        return -1;                                    // real error
    }
}

static void sem_op_retry(int semid, unsigned short semnum, short delta) {
    struct sembuf op = {.sem_num = semnum, .sem_op = delta, .sem_flg = 0};

    while (1) {
        if (semop(semid, &op, 1) == 0)
            return;                 // OK: void return

        if (errno == EINTR)
            continue;

        perror("semop");
        exit(1);
    }
}


static void lock_global(void){ sem_op(sem_id, SEM_GLOBAL_LOCK, -1); }
static void unlock_global(void){ sem_op(sem_id, SEM_GLOBAL_LOCK, +1); }

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

static key_t make_key(const char *path, int proj_id) {
    key_t k = ftok(path, proj_id);
    if (k == -1) die("ftok");
    return k;
}

static void attach_ipc(const char *ftok_path) {
    key_t shm_key = make_key(ftok_path, 'S');
    key_t sem_key = make_key(ftok_path, 'M');

    shm_id = shmget(shm_key, sizeof(shm_state_t), 0600);
    if (shm_id == -1) die("shmget");

    S = (shm_state_t*)shmat(shm_id, NULL, 0);
    if (S == (void*)-1) die("shmat");

    sem_id = semget(sem_key, SEM_COUNT, 0600);
    if (sem_id == -1) die("semget");

    if (S->magic != SHM_MAGIC) {
        fprintf(stderr, "[BS] SHM magic mismatch\n");
        exit(2);
    }
}

static void mark_dead(uint16_t unit_id) {
    lock_global();
    if (unit_id <= MAX_UNITS) {
        S->units[unit_id].alive = 0;

        int x = S->units[unit_id].x;
        int y = S->units[unit_id].y;
        if (x >= 0 && x < N && y >= 0 && y < M) {
            if (S->grid[x][y] == (unit_id_t)unit_id) S->grid[x][y] = 0;
        }
    }
    unlock_global();
}

int main(int argc, char **argv) {
    const char *ftok_path = "./ipc.key";
    uint16_t unit_id = 0;
    int faction = 0, type = 0, x = -1, y = -1;

    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "--ftok") && i+1<argc) ftok_path = argv[++i];
        else if (!strcmp(argv[i], "--unit") && i+1<argc) unit_id = (uint16_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--faction") && i+1<argc) faction = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--type") && i+1<argc) type = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--x") && i+1<argc) x = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--y") && i+1<argc) y = atoi(argv[++i]);
    }

    if (unit_id == 0 || unit_id > MAX_UNITS) {
        fprintf(stderr, "[BS] invalid unit_id\n");
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    attach_ipc(ftok_path);

    // ensure registry PID is correct (independent consistency)
    lock_global();
    S->units[unit_id].pid = getpid();
    S->units[unit_id].faction = (uint8_t)faction;
    S->units[unit_id].type = (uint8_t)type;
    S->units[unit_id].alive = 1;
    S->units[unit_id].x = (int16_t)x;
    S->units[unit_id].y = (int16_t)y;
    unlock_global();

    printf("[BS %u] pid=%d faction=%d type=%d pos=(%d,%d)\n", unit_id, (int)getpid(), faction, type, x, y);

    // Dummy behavior: just “live” and occasionally print ticks
    while (!g_stop) {
    // wait for CC permit (interruptible)
    if (sem_op_intr(sem_id, SEM_TICK_START, -1) == -1) {
        if (g_stop) break;
        continue;
    }

    lock_global();
    uint32_t t = S->ticks;

    // ensure max 1 action per tick
    if (S->last_step_tick[unit_id] != t) {
        S->last_step_tick[unit_id] = t;

        // TODO: real per-tick logic here (move/shoot/detect)
        if ((t % 25) == 0) {
            printf("[BS %u] tick=%u step done (random order)\n", unit_id, t);
            fflush(stdout);
        }
    }

    // optional bookkeeping
    if (S->tick_done < 65535) S->tick_done++;

    unlock_global();

    // notify CC: done
    sem_op_retry(sem_id, SEM_TICK_DONE, +1);
}

    printf("[BS %u] terminating, cleaning registry/grid\n", unit_id);
    mark_dead(unit_id);

    shmdt(S);
    return 0;
}
