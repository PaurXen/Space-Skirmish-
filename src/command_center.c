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

#include "./ipc/shared.h"

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

static void sem_op(int semid, unsigned short semnum, short delta) {
    struct sembuf op = {.sem_num=semnum, .sem_op=delta, .sem_flg=0};
    if (semop(semid, &op, 1) == -1) die("semop");
}

static int sem_op_intr(int semid, unsigned short semnum, short delta) {
    struct sembuf op = {.sem_num = semnum, .sem_op = delta, .sem_flg = 0};

    while (1) {
        if (semop(semid, &op, 1) == 0) return 0;  // success
        if (errno == EINTR) {
            if (g_stop) return -1;                // interrupted and stopping
            continue;                              // retry
        }
        perror("semop");
        return -1;
    }
}

static void sem_op_retry(int semid, unsigned short semnum, short delta) {
    struct sembuf op = {.sem_num = semnum, .sem_op = delta, .sem_flg = 0};

    while (1) {
        if (semop(semid, &op, 1) == 0) return;    // success
        if (errno == EINTR) continue;             // retry
        perror("semop");
        exit(1);
    }
}



static void lock_global(void){ sem_op(sem_id, SEM_GLOBAL_LOCK, -1); }
static void unlock_global(void){ sem_op(sem_id, SEM_GLOBAL_LOCK, +1); }

static void cleanup_ipc(void) {
    if (S && S != (void*)-1) shmdt(S);
    S = NULL;

    if (shm_id != -1) {
        // Remove SHM
        if (shmctl(shm_id, IPC_RMID, NULL) == -1) perror("shmctl(IPC_RMID)");
        shm_id = -1;
    }
    if (sem_id != -1) {
        // Remove SEM set
        if (semctl(sem_id, 0, IPC_RMID) == -1) perror("semctl(IPC_RMID)");
        sem_id = -1;
    }
}

static key_t make_key(const char *path, int proj_id) {
    key_t k = ftok(path, proj_id);
    if (k == -1) die("ftok");
    return k;
}

static void init_shm_and_sems(const char *ftok_path) {
    key_t shm_key = make_key(ftok_path, 'S');
    key_t sem_key = make_key(ftok_path, 'M');

    // Minimal permissions: owner read/write
    shm_id = shmget(shm_key, sizeof(shm_state_t), IPC_CREAT | IPC_EXCL | 0600);
    if (shm_id == -1) {
        if (errno == EEXIST) {
            // If leftover exists from crash, attach & reuse OR remove and recreate.
            // For now: reuse.
            shm_id = shmget(shm_key, sizeof(shm_state_t), 0600);
            if (shm_id == -1) die("shmget(reuse)");
        } else die("shmget");
    }

    S = (shm_state_t*)shmat(shm_id, NULL, 0);
    if (S == (void*)-1) die("shmat");

    sem_id = semget(sem_key, SEM_COUNT, IPC_CREAT | IPC_EXCL | 0600);
    if (sem_id == -1) {
        if (errno == EEXIST) {
            sem_id = semget(sem_key, SEM_COUNT, 0600);
            if (sem_id == -1) die("semget(reuse)");
        } else die("semget");
    } else {
        // Initialize semaphores only if newly created
        union semun u;
        unsigned short vals[SEM_COUNT];
        vals[SEM_GLOBAL_LOCK] = 1; // unlocked
        vals[SEM_TICK_START] = 0;        // not used yet
        vals[SEM_TICK_DONE] = 0;         // not used yet
        u.array = vals;
        if (semctl(sem_id, 0, SETALL, u) == -1) die("semctl(SETALL)");
    }

    // Initialize shared state once
    lock_global();
    if (S->magic != SHM_MAGIC) {
        memset(S, 0, sizeof(*S));
        S->magic = SHM_MAGIC;
        S->next_unit_id = 1;
        S->ticks = 0;
    }
    unlock_global();
}

static uint16_t alloc_unit_id(void) {
    uint16_t id;
    lock_global();
    if (S->next_unit_id > MAX_UNITS) {
        unlock_global();
        fprintf(stderr, "No more unit IDs available (MAX_UNITS=%d)\n", MAX_UNITS);
        return 0;
    }
    id = S->next_unit_id++;
    unlock_global();
    return id;
}

static void register_unit(uint16_t unit_id, pid_t pid, faction_t faction, unit_type_t type, int x, int y) {
    lock_global();

    S->units[unit_id].pid = pid;
    S->units[unit_id].faction = (uint8_t)faction;
    S->units[unit_id].type = (uint8_t)type;
    S->units[unit_id].alive = 1;
    S->units[unit_id].x = (int16_t)x;
    S->units[unit_id].y = (int16_t)y;

    // place on grid with unit_id (NOT pid)
    if (x >= 0 && x < N && y >= 0 && y < M) {
        if (S->grid[x][y] == 0)
            S->grid[x][y] = (unit_id_t)unit_id;
        else
            fprintf(stderr, "Warning: grid[%d][%d] already occupied by unit_id=%d\n", x, y, (int)S->grid[x][y]);
    }

    S->unit_count++;

    unlock_global();
}

static pid_t spawn_battleship(const char *exe_path, uint16_t unit_id, faction_t faction, unit_type_t type, int x, int y,
                             const char *ftok_path)
{
    pid_t pid = fork();
    if (pid == -1) die("fork");

    if (pid == 0) {
        // child: exec battleship with args
        char unit_id_s[16], faction_s[16], type_s[16], x_s[16], y_s[16];
        snprintf(unit_id_s, sizeof(unit_id_s), "%u", unit_id);
        snprintf(faction_s, sizeof(faction_s), "%u", (unsigned)faction);
        snprintf(type_s, sizeof(type_s), "%u", (unsigned)type);
        snprintf(x_s, sizeof(x_s), "%d", x);
        snprintf(y_s, sizeof(y_s), "%d", y);

        execl(exe_path, exe_path,
              "--ftok", ftok_path,
              "--unit", unit_id_s,
              "--faction", faction_s,
              "--type", type_s,
              "--x", x_s,
              "--y", y_s,
              (char*)NULL);

        perror("execl(battleship)");
        _exit(127);
    }

    // parent
    register_unit(unit_id, pid, faction, type, x, y);
    return pid;
}

int main(int argc, char **argv) {
    const char *ftok_path = "./ipc.key";     // file must exist
    const char *battleship_exe = "./battleship";

    const useconds_t tick_us = 200 * 1000;  // 200ms per tick

    // small CLI override
    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "--ftok") && i+1<argc) ftok_path = argv[++i];
        else if (!strcmp(argv[i], "--battleship") && i+1<argc) battleship_exe = argv[++i];
    }

    // ensure ftok file exists
    FILE *f = fopen(ftok_path, "a");
    if (!f) die("fopen(ftok_path)");
    fclose(f);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    atexit(cleanup_ipc);

    init_shm_and_sems(ftok_path);

    // Spawn a few dummy battleships
    // Republic: destroyer + carrier
    uint16_t u1 = alloc_unit_id();
    uint16_t u2 = alloc_unit_id();
    // CIS: destroyer + carrier
    uint16_t u3 = alloc_unit_id();
    uint16_t u4 = alloc_unit_id();

    if (!u1 || !u2 || !u3 || !u4) {
        fprintf(stderr, "Failed to allocate unit ids\n");
        return 1;
    }

    spawn_battleship(battleship_exe, u1, FACTION_REPUBLIC, TYPE_DESTROYER, 5, 10, ftok_path);
    spawn_battleship(battleship_exe, u2, FACTION_REPUBLIC, TYPE_CARRIER,   8, 12, ftok_path);
    spawn_battleship(battleship_exe, u3, FACTION_CIS,      TYPE_DESTROYER, 30, 60, ftok_path);
    spawn_battleship(battleship_exe, u4, FACTION_CIS,      TYPE_CARRIER,   32, 62, ftok_path);

    printf("[CC] shm_id=%d sem_id=%d spawned 4 battleships. Ctrl+C to stop.\n", shm_id, sem_id);

    // Main loop: just tick + show minimal state (no fancy UI yet)
    while (!g_stop) {
        usleep(tick_us);

        uint32_t t = S->ticks;
        uint16_t alive = 0;

        lock_global();
        S->ticks++;

        for (int id=1; id<=MAX_UNITS; id++) if (S->units[id].alive) alive++;
        S->tick_expected = alive;
        S->tick_done = 0;
        unlock_global();

        // release exctly one permit per alive unit
        for (unsigned i=0; i<alive; i++) {
            sem_op_retry(sem_id, SEM_TICK_START, +1);
        }

        // wait until all alive units finish their step
        for (unsigned i=0; i<alive; i++) {
            if (sem_op_intr(sem_id, SEM_TICK_DONE, -1) == -1) {
                break;  // Ctrl+C / SIGTERM or real error
            }

        }

        if ((t % 10) == 0) {
            printf("[CC] ticks=%u alive_units=%u\n", t, alive);
            fflush(stdout);
        }
    }

    printf("[CC] stopping: sending SIGTERM to alive units...\n");

    lock_global();
    for (int id=1; id<=MAX_UNITS; id++) {
        if (S->units[id].alive && S->units[id].pid > 1) {
            kill(S->units[id].pid, SIGTERM);
        }
    }
    unlock_global();

    // reap children
    int status;
    while (wait(&status) > 0) { /* drain */ }

    printf("[CC] exit.\n");
    return 0;
}
