#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ipc/ipc_context.h"
#include "ipc/semaphores.h"
#include "ipc/shared.h"
#include "log.h"

/* Command Center (CC)
 *
 * Responsibilities:
 *  - Create/reset IPC (shared memory + semaphores).
 *  - Spawn battleship worker processes and register them in shared state.
 *  - Drive a periodic "tick" barrier: on each tick CC posts SEM_TICK_START
 *    once per alive unit, then waits for SEM_TICK_DONE from each unit.
 *  - Handle shutdown: notify alive units with SIGTERM, reap children, and
 *    cleanup IPC objects and logs.
 */

static volatile sig_atomic_t g_stop = 0;

/* Signal handler: set cooperative stop flag */
static void on_signal(int sig) {
    (void)sig;
    LOGD("g_stop flag raised. (g_stop = 1)");
    g_stop = 1;
}

/* Create a per-run directory name under ./logs with timestamp+pid and ensure it exists. */
static void make_run_dir(char *out, size_t out_sz) {
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    pid_t pid = getpid();

    /* ensure base logs/ exists */
    (void)mkdir("logs", 0755);

    snprintf(out, out_sz,
             "logs/run_%04d-%02d-%02d_%02d-%02d-%02d_pid%d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec,
             (int)pid);

    (void)mkdir(out, 0755);
}

/* Allocate the next unit id from shared state under the global lock.
 * Returns 0 on failure (no ids left) or the allocated id. */
static uint16_t alloc_unit_id(ipc_ctx_t *ctx) {
    uint16_t id = 0;

    sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);

    for (uint16_t i = 1; i <= MAX_UNITS; i++) {
        if (ctx->S->units[i].alive == 0 &&
            ctx->S->units[i].pid == 0) {
            ctx->S->units[i].alive = -1;
            id = i;
            break;
        }
    }

    sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
    if (id == 0){
        LOGD("No more unit IDs available (MAX_UNITS=%d)", MAX_UNITS);
        fprintf(stderr, "[CC] No more unit IDs available (MAX_UNITS=%d)\n", MAX_UNITS);
    }
    return id;   // 0 means "no free slot"
}


/* Register a unit in shared memory:
 *  - sets PID, faction, type, alive flag and position
 *  - attempts to place unit in grid if cell empty (warns if occupied)
 *  - increments global unit_count
 * Protected by SEM_GLOBAL_LOCK by caller. */
static void register_unit(ipc_ctx_t *ctx, uint16_t unit_id, pid_t pid,
                          faction_t faction, unit_type_t type, int x, int y)
{
    sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);

    ctx->S->units[unit_id].pid = pid;
    ctx->S->units[unit_id].faction = (uint8_t)faction;
    ctx->S->units[unit_id].type = (uint8_t)type;
    ctx->S->units[unit_id].alive = 1;
    ctx->S->units[unit_id].position.x = (uint16_t)x;
    ctx->S->units[unit_id].position.y = (uint16_t)y;

    if (x>=0 && x<N && y>=0 && y<M) {
        if (ctx->S->grid[x][y] == 0)
            ctx->S->grid[x][y] = (unit_id_t)unit_id;
        else {
            LOGD("Warning: grid[%d][%d] occupied by unit_id=%d",
                    x, y, (int)ctx->S->grid[x][y]);
            fprintf(stderr, "[CC] Warning: grid[%d][%d] occupied by unit_id=%d\n",
                    x, y, (int)ctx->S->grid[x][y]);
            }
    }

    ctx->S->unit_count++;

    sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
}

/* Spawn a battleship process:
 *  - fork + execl; child execs the battleship binary with args.
 *  - parent registers the unit and returns child pid (or -1 on error).
 */
static pid_t spawn_battleship(ipc_ctx_t *ctx, const char *exe_path,
                              uint16_t unit_id, faction_t faction,
                              unit_type_t type, int x, int y,
                              const char *ftok_path)
{
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        char unit_id_s[16], faction_s[16], types_s[16], x_s[16], y_s[16];
        snprintf(unit_id_s, 16, "%u", unit_id);
        snprintf(faction_s, 16, "%u", faction);
        snprintf(types_s, 16, "%u", type);
        snprintf(x_s, 16, "%d", x);
        snprintf(y_s, 16, "%d", y);

        execl(exe_path, exe_path,
              "--ftok", ftok_path,
              "--unit", unit_id_s,
              "--faction", faction_s,
              "--type", types_s,
              "--x", x_s,
              "--y", y_s,
            NULL);
        _exit(1);
    }
    register_unit(ctx, unit_id, pid, faction, type, x, y);
    LOGD("battleship pid=%d id=%d spawned", unit_id, pid);
    return pid;
}

static void cleanup_dead_units(ipc_ctx_t *ctx) {
    sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);

    for (uint16_t id = 1; id <= MAX_UNITS; id++) {
        if (ctx->S->units[id].alive == 0 &&
            ctx->S->units[id].pid > 0) {

            pid_t pid = ctx->S->units[id].pid;

            printf("[CC] unit %u marked dead, terminating pid %d\n", id, pid);
            fflush(stdout);

            kill(pid, SIGTERM);

            // mark slot reusable
            ctx->S->units[id].pid = 0;
            ctx->S->units[id].type = 0;
            ctx->S->units[id].faction = 0;
            ctx->S->units[id].position.x = -1;
            ctx->S->units[id].position.y = -1;
            ctx->S->units[id].flags = -1;
        }
    }

    sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);

    // reap zombies (non-blocking)
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

static void process_dmg_payload(ipc_ctx_t *ctx) {
    sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
    for (uint16_t id = 1; id <= MAX_UNITS; id++) {
        if (ctx->S->units[id].alive == 1
            && ctx->S->units[id].pid > 0
            && ctx->S->units[id].dmg_payload != 0) {
            
                pid_t pid = ctx->S->units[id].pid;

                kill(pid, SIGRTMAX);
        }
    }
    sem_unlock(ctx->sem_id,SEM_GLOBAL_LOCK);
}


/* Start a small 'tee' process that mirrors the program's stdout/stderr into a file:
 *  - creates a pipe, forks a child that reads from pipe and writes to both
 *    the real terminal (stdout) and a run-specific ALL.term.log file.
 *  - parent redirects its stdout/stderr to the pipe so all output is captured.
 * Returns pid of tee child or -1 on error.
 */
static pid_t start_terminal_tee(const char *run_dir) {
    int pfd[2];
    if (pipe(pfd) == -1) {
        perror("pipe");
        return -1;
    }

    /* open output file (exact terminal transcript) */
    char out_path[600];
    snprintf(out_path, sizeof(out_path), "%s/ALL.term.log", run_dir);
    int out_fd = open(out_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (out_fd == -1) {
        perror("open ALL.term.log");
        close(pfd[0]); close(pfd[1]);
        return -1;
    }

    pid_t tee_pid = fork();
    if (tee_pid == -1) {
        perror("fork tee");
        close(out_fd);
        close(pfd[0]); close(pfd[1]);
        return -1;
    }

    if (tee_pid == 0) {
        /* child: read from pipe and write to both terminal and file */
        close(pfd[1]); // close write end

        char buf[4096];
        for (;;) {
            ssize_t n = read(pfd[0], buf, sizeof(buf));
            if (n == 0) break;
            if (n < 0) _exit(0);

            (void)write(STDOUT_FILENO, buf, (size_t)n);
            (void)write(out_fd, buf, (size_t)n);
        }

        close(out_fd);
        close(pfd[0]);
        _exit(0);
    }

    /* parent: redirect stdout/stderr to pipe */
    close(pfd[0]); // close read end in parent
    close(out_fd);

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    if (dup2(pfd[1], STDOUT_FILENO) == -1) perror("dup2 stdout");
    if (dup2(pfd[1], STDERR_FILENO) == -1) perror("dup2 stderr");
    close(pfd[1]);

    return tee_pid;
}

int main(int argc, char **argv) {
    const char *ftok_path = "./ipc.key";
    const char *battleship = "./battleship";

    const useconds_t tick_us = 200 * 1000; /* tick interval */

    for (int i=1; i<argc;i++) {
        if (!strcmp(argv[i], "--ftok") && i+1<argc) ftok_path = argv[++i];
        else if (!strcmp(argv[i], "--battleship") && i+1<argc) battleship = argv[++i];
    }

    /* Setup signal handlers for clean shutdown */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    ipc_ctx_t ctx;
    if (ipc_create(&ctx, ftok_path) == -1) {
        perror("ipc_create");
        return 1;
    }

    /* Prepare run directory and tee to capture terminal output into run logs */
    char run_dir[512];
    make_run_dir(run_dir, sizeof(run_dir));
    setenv("SKIRMISH_RUN_DIR", run_dir, 1);
    
    pid_t tee_pid = start_terminal_tee(run_dir);
    if (tee_pid == -1) {
        fprintf(stderr, "Failed to start terminal tee\n");
    }

    log_init("CC", 0);
    atexit(log_close);

    /* Allocate a few unit ids and spawn battleship processes */
    uint16_t u1 = alloc_unit_id(&ctx);
    uint16_t u2 = alloc_unit_id(&ctx);
    uint16_t u3 = alloc_unit_id(&ctx);
    uint16_t u4 = alloc_unit_id(&ctx);
    if (!u1 || !u2 || !u3 || !u4) {
        ipc_detach(&ctx);
        ipc_destroy(&ctx);
        return 1;
    }

    spawn_battleship(&ctx, battleship, u1, FACTION_REPUBLIC, TYPE_DESTROYER, 5, 10, ftok_path);
    spawn_battleship(&ctx, battleship, u2, FACTION_REPUBLIC, TYPE_CARRIER,   8, 12, ftok_path);
    spawn_battleship(&ctx, battleship, u3, FACTION_CIS,      TYPE_DESTROYER, 30, 60, ftok_path);
    spawn_battleship(&ctx, battleship, u4, FACTION_CIS,      TYPE_CARRIER,   32, 62, ftok_path);

    LOGI("[CC] shm_id=%d sem_id=%d spawned 4 battleships. Ctrl+C to stop.",ctx.shm_id, ctx.sem_id);
    printf("[CC] shm_id=%d sem_id=%d spawned 4 battleships. Ctrl+C to stop.\n",ctx.shm_id, ctx.sem_id);


    /* Main tick loop:
     *  - sleep for tick interval
     *  - increment global tick counter and compute alive units
     *  - set tick_expected and reset tick_done under global lock
     *  - post SEM_TICK_START once per alive unit
     *  - wait for SEM_TICK_DONE alive times (interruptible)
     */
    while (!g_stop) {
        usleep(tick_us);
                if (sem_lock_intr(ctx.sem_id, SEM_GLOBAL_LOCK, &g_stop) == -1) break;
        ctx.S->ticks++;
        uint32_t t =ctx.S->ticks;

        uint16_t alive = 0;
        for (int id=1; id<=MAX_UNITS; id++) if (ctx.S->units[id].alive) alive++;
        
        ctx.S->tick_expected = alive;
        ctx.S->tick_done = 0;

        sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);
                /* release exactly one start permit per alive unit */
        for (unsigned i=0; i<alive; i++) {
            if (sem_post_retry(ctx.sem_id, SEM_TICK_START, +1) == -1) {
                perror("sem_post_retry(TICK_START)");
                g_stop = 1;
                break;
            }
        }
                /* wait for all alive units to report done (cooperative interrupt via g_stop) */
        for (unsigned i=0; i<alive; i++) {
            if (sem_wait_intr(ctx.sem_id, SEM_TICK_DONE, -1, &g_stop) == -1) {
                break; // Ctrl+C or error
            }
        }
                process_dmg_payload(&ctx);
        cleanup_dead_units(&ctx);
        
        if ((t % 25) == 0) {
            LOGI("ticks=%u alive_units=%u", t, alive);
            printf("[CC] ticks=%u alive_units=%u\n", t, alive);
            fflush(stdout);
            printf("[ ");
            for (int id=1; id<=MAX_UNITS; id++) {
                printf("%d, ", ctx.S->units[id].pid);
            } printf(" ]");
            fflush(stdout);
        }
    }

    /* Shutdown sequence:
     *  - signal all alive unit processes with SIGTERM
     *  - reap child processes
     *  - detach and destroy IPC objects
     */
    LOGW("stopping: sending SIGTERM to alive units...");
    printf("[CC] stopping: sending SIGTERM to alive units...\n");

    sem_lock(ctx.sem_id, SEM_GLOBAL_LOCK);
    for (int id=1; id<=MAX_UNITS; id++) {
        if (ctx.S->units[id].pid > 1) {
            kill(ctx.S->units[id].pid, SIGTERM);
        }
    }
    sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

    /* reap children */
    while (wait(NULL) > 0) {}

    /* cleanup IPC and exit */
    ipc_detach(&ctx);
    ipc_destroy(&ctx);

    printf("[CC] exit.\n");
    return 0;
}
