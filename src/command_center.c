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
#include <errno.h>

#include "ipc/ipc_context.h"
#include "ipc/semaphores.h"
#include "ipc/shared.h"
#include "ipc/ipc_mesq.h"
#include "unit_ipc.h"
#include "unit_logic.h"
#include "unit_stats.h"
#include "unit_size.h"
#include "log.h"
#include "terminal_tee.h"


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
static void on_term(int sig) {
    (void)sig;
    LOGD("g_stop flag raised. (g_stop = 1)");
    g_stop = 1;
}

static void exit_handler(void) {
    g_stop = 1;
    printf("[CC] Exit handler called, setting g_stop=1\n");
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
    unit_id_t id = 0;

    // sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);

    for (uint16_t i = 1; i <= MAX_UNITS; i++) {
        if (ctx->S->units[i].alive == 0 &&
            ctx->S->units[i].pid == 0) {
            ctx->S->units[i].alive = -1;
            id = i;
            break;
        }
    }

    // sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
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
static void register_unit(ipc_ctx_t *ctx, unit_id_t unit_id, pid_t pid,
                          faction_t faction, unit_type_t type, point_t pos)
{
    // sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);

    ctx->S->units[unit_id].pid = pid;
    ctx->S->units[unit_id].faction = (uint8_t)faction;
    ctx->S->units[unit_id].type = (uint8_t)type;
    ctx->S->units[unit_id].alive = 1;
    ctx->S->units[unit_id].position = pos;

    // Place unit on grid using size mechanic
    unit_stats_t stats = unit_stats_for_type(type);
    place_unit_on_grid(ctx, unit_id, pos, stats.si);

    ctx->S->unit_count++;

    // sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
}

/* Spawn a battleship process:
 *  - fork + execl; child execs the battleship binary with args.
 *  - parent registers the unit and returns child pid (or -1 on error).
 */
static pid_t spawn_unit(ipc_ctx_t *ctx, const char *exe_path,
                              unit_id_t unit_id, faction_t faction,
                              unit_type_t type, point_t pos,
                              const char *ftok_path, unit_id_t commander_id)
{
    pid_t pid = fork();
    if (pid == -1) {
        LOGE("[CC] fork failed for unit_id=%u: %s", unit_id, strerror(errno));
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        char unit_id_s[16], faction_s[16], types_s[16], x_s[16], y_s[16], commander_s[16];
        snprintf(unit_id_s, 16, "%u", unit_id);
        snprintf(faction_s, 16, "%u", faction);
        snprintf(types_s, 16, "%u", type);
        snprintf(x_s, 16, "%d", pos.x);
        snprintf(y_s, 16, "%d", pos.y);
        snprintf(commander_s, 16, "%u", commander_id);

        execl(exe_path, exe_path,
              "--ftok", ftok_path,
              "--unit", unit_id_s,
              "--faction", faction_s,
              "--type", types_s,
              "--x", x_s,
              "--y", y_s,
              "--commander", commander_s,
            NULL);
        /* execl only returns on error */
        fprintf(stderr, "[CC child] execl(%s) failed: %s\n", exe_path, strerror(errno));
        perror("execl");
        _exit(1);
    }
    register_unit(ctx, unit_id, pid, faction, type, pos);
    LOGD("[CC] spawned unit_id=%u pid=%d type=%u faction=%u at (%d,%d)",
            unit_id, (int)pid, (unsigned)type, (unsigned)faction, pos.x, pos.y);
    return pid;
}


static pid_t spawn_squadron(ipc_ctx_t *ctx,
    const char *exe_path,
    unit_type_t u_type,
    faction_t faction,
    point_t pos,
    const char *ftok_path,
    unit_id_t commander_id,
    unit_id_t *out_unit_id
){
    uint16_t unit_id = alloc_unit_id(ctx);
    if (unit_id == 0) {
        LOGE("[CC] Failed to allocate unit ID for new squadron");
        fprintf(stderr, "[CC] Failed to allocate unit ID for new squadron\n");
        return -1;
    }

    pid_t pid = spawn_unit(
        ctx,
        exe_path,
        unit_id,
        faction,
        u_type,
        pos,
        ftok_path,
        commander_id
    );
    if (pid == -1) {
        LOGE("[CC] Failed to spawn squadron process for unit %u", unit_id);
        fprintf(stderr, "[CC] Failed to spawn squadron process for unit %u\n", unit_id);
        /* Free the allocated unit_id since spawn failed */
        ctx->S->units[unit_id].alive = 0;
        ctx->S->units[unit_id].pid = 0;
        if (ctx->S->unit_count > 0) ctx->S->unit_count--;
        return -1;
    }

    if (out_unit_id) *out_unit_id = unit_id;
    return pid;
}

static void cleanup_dead_units(ipc_ctx_t *ctx) {
    pid_t killed[MAX_UNITS];
    int killed_n = 0;

    sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);

    for (unit_id_t id = 1; id <= MAX_UNITS; id++) {
        if (ctx->S->units[id].alive == 0 && ctx->S->units[id].pid > 0) {
            pid_t pid = ctx->S->units[id].pid;

            printf("[CC] unit %u marked dead, terminating pid %d\n", id, pid);
            fflush(stdout);

            (void)kill(pid, SIGTERM);

            if (killed_n < MAX_UNITS) {
                killed[killed_n++] = pid;
            }

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

    // wait/reap ONLY those we just SIGTERM'ed (bounded wait)
    for (int i = 0; i < killed_n; i++) {
        int status = 0;

        // wait up to ~500ms total for this pid to exit
        for (int tries = 0; tries < 50; tries++) {
            pid_t r = waitpid(killed[i], &status, WNOHANG);

            if (r == killed[i]) {
                // reaped successfully
                break;
            }
            if (r == 0) {
                // still running
                usleep(10 * 1000);
                continue;
            }
            if (r == -1) {
                if (errno == EINTR) continue;   // retry
                if (errno == ECHILD) break;     // already reaped / not our child
                break;
            }
        }
    }
}

void print_grid(ipc_ctx_t *ctx) {
    // Print grid
        sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
        printf("\n\t");
        for (int i = 0; i < M; i++) printf("%d", i % 10);
        printf("\n");
        for (int i = 0; i < N; i++) {
            printf("%d\t", i);
            for (int j = 0; j < M; j++) {
                int16_t t = ctx->S->grid[j][i];
                if (t == 0) {
                    printf(".");
                } else if (0 < t && t < MAX_UNITS)  {
                    uint8_t faction = ctx->S->units[t].faction;
                    const char *color = "\x1b[0m"; // default
                    if (faction == FACTION_REPUBLIC) color = "\x1b[34m"; // blue
                    else if (faction == FACTION_CIS) color = "\x1b[31m"; // red
                    printf("%s%d\x1b[0m", color, t);
                } else {
                    printf(".");
                }
            }
            printf("\n");
        }
        fflush(stdout);
        sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
}


int main(int argc, char **argv) {
    // atexit(exit_handler);
    setpgid(0, 0);
    const char *ftok_path = "./ipc.key";
    const char *battleship = "./battleship";
    const char *squadron = "./squadron";

    const useconds_t tick_us = 1000 * 1000; /* tick interval */

    for (int i=1; i<argc;i++) {
        if (!strcmp(argv[i], "--ftok") && i+1<argc) ftok_path = argv[++i];
        else if (!strcmp(argv[i], "--battleship") && i+1<argc) battleship = argv[++i];
        else if (!strcmp(argv[i], "--squadron") && i+1<argc) squadron = argv[++i];
    }

    /* Setup signal handlers for clean shutdown */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_term;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    ipc_ctx_t ctx;
    if (ipc_create(&ctx, ftok_path) == -1) {
        fprintf(stderr, "[CC] ipc_create failed: %s\n", strerror(errno));
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

    if (log_init("CC", 0) == -1) {
        fprintf(stderr, "[CC] log_init failed, continuing without logs\n");
    }
    atexit(log_close);

    
    if (sem_lock(ctx.sem_id, SEM_GLOBAL_LOCK) == -1) {
        fprintf(stderr, "[CC] failed to acquire initial lock: %s\n", strerror(errno));
        perror("sem_lock");
        ipc_detach(&ctx);
        ipc_destroy(&ctx);
        return 1;
    }
    LOGD("TESTU #1 - allocating units");
    /* Allocate a few unit ids and spawn battleship processes */
    uint16_t u1 = alloc_unit_id(&ctx);
    uint16_t u2 = alloc_unit_id(&ctx);
    uint16_t u3 = alloc_unit_id(&ctx);
    uint16_t u4 = alloc_unit_id(&ctx);
    if (!u1 || !u2 || !u3 || !u4) {
        // if (!u1 || !u3) {
            LOGE("[CC] Failed to allocate units: u1=%u u2=%u u3=%u u4=%u", u1, u2, u3, u4);
            fprintf(stderr, "[CC] Failed to allocate units\n");
            sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);
            ipc_detach(&ctx);
            ipc_destroy(&ctx);
            return 1;
        }
    LOGD("TESTU #2 - spawening units");
    
    spawn_unit(&ctx, battleship, u1, FACTION_REPUBLIC, TYPE_CARRIER, (point_t){5, 5}, ftok_path, 0);
    spawn_unit(&ctx, battleship, u2, FACTION_REPUBLIC, TYPE_CARRIER,   (point_t){5, N-5}, ftok_path, 0);
    spawn_unit(&ctx, battleship, u3, FACTION_CIS,      TYPE_CARRIER, (point_t){M-5, 5}, ftok_path, 0);
    spawn_unit(&ctx, battleship, u4, FACTION_CIS,      TYPE_CARRIER,   (point_t){M-5, N-5}, ftok_path, 0);

    sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

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
        /* Use select/poll with timeout instead of usleep to check g_stop more often */
        struct timeval tv = {0, tick_us};
        fd_set rfds;
        FD_ZERO(&rfds);
        
        /* This allows us to sleep but still be interruptible */
        int ret = select(0, NULL, NULL, NULL, &tv);
        if (ret == -1 && errno == EINTR) {
            if (g_stop) break;
        }
        
        if (g_stop) break;  // Check immediately after sleep

        if (sem_lock_intr(ctx.sem_id, SEM_GLOBAL_LOCK, &g_stop) == -1) break;

        mq_spawn_req_t r;
        while (mq_try_recv_spawn(ctx.q_req, &r) == 1) {

            LOGD("[CC] received spawn request from BS %u at (%d,%d) for type %d",
                    r.sender_id, r.pos.x, r.pos.y, r.utype);
            int16_t status;
            pid_t child_pid = -1;
            unit_id_t child_unit_id = 0;
            
            // Check if the unit can fit at the requested position
            unit_stats_t spawn_stats = unit_stats_for_type((unit_type_t)r.utype);
            if (can_fit_at_position(&ctx, r.pos, spawn_stats.si, 0) && 
                in_bounds(r.pos.x, r.pos.y, M, N)) {
                child_pid = spawn_squadron(
                    &ctx,
                    squadron,
                    r.utype,
                    ctx.S->units[r.sender_id].faction,
                    r.pos,
                    ftok_path,
                    r.commander_id,
                    &child_unit_id
                );
                status = 0;
            } else {
                LOGI("[CC] spawn request at (%d,%d) rejected: insufficient space or OOB",
                        r.pos.x, r.pos.y);
                fprintf(stderr, "[CC] spawn request at (%d,%d) rejected: insufficient space or OOB\n",
                        r.pos.x, r.pos.y);
                status = -1;
            }


            mq_spawn_rep_t rep = {
                .mtype = r.sender,
                .req_id = r.req_id,
                .status = status,
                .child_pid = (status==0 ? child_pid : -1),
                .child_unit_id = (status==0 ? child_unit_id : 0)
            };
            mq_send_reply(ctx.q_rep, &rep);

        }

        ctx.S->ticks++;
        uint32_t t =ctx.S->ticks;

        uint8_t alive = 0;
        for (int id=1; id<=MAX_UNITS; id++) if (ctx.S->units[id].alive) alive++;
        
        ctx.S->tick_expected = alive;
        ctx.S->tick_done = 0;

        sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);
                /* release exactly one start permit per alive unit */
        for (unsigned i=0; i<alive; i++) {
            if (sem_post_retry(ctx.sem_id, SEM_TICK_START, +1) == -1) {
                LOGE("[CC] sem_post_retry(TICK_START) failed: %s", strerror(errno));
                perror("sem_post_retry(TICK_START)");
                g_stop = 1;
                break;
            }
        }
        /* wait for all alive units to report done (cooperative interrupt via g_stop) */
        for (unsigned i=0; i<alive; i++) {
            if (sem_wait_intr(ctx.sem_id, SEM_TICK_DONE, -1, &g_stop) == -1) {
                if (g_stop) {
                    LOGW("[CC] sem_wait_intr interrupted by stop signal");
                } else {
                    LOGE("[CC] sem_wait_intr failed: %s", strerror(errno));
                }
                break; // Ctrl+C or error
            }
            // printf("[CC] got\n");
            // fflush(stdout);
        }
        // printf("[CC] got all\n");
        //             fflush(stdout);

        print_grid(&ctx);

        cleanup_dead_units(&ctx);

        
        
        if ((t % 1) == 0) {
            LOGI("ticks=%u alive_units=%u", t, alive);
            printf("[CC] ticks=%u alive_units=%u\n", t, alive);
            fflush(stdout);
            printf("[ ");
            for (int id=1; id<=MAX_UNITS; id++) {
                printf("%d, ", ctx.S->units[id].pid);
            } printf(" ]\n");
            fflush(stdout);

        }
        int c_r = 0, c_s = 0;
        for (int id=1; id<=MAX_UNITS; id++) {
            if (ctx.S->units[id].faction == FACTION_REPUBLIC) c_r++;
            else if (ctx.S->units[id].faction == FACTION_CIS) c_s++;
        }
        if (c_r == 0 || c_s == 0) {
            LOGI("Faction elimination detected: Republic=%d CIS=%d", c_r, c_s);
            printf("[CC] Faction elimination detected: Republic=%d CIS=%d\n", c_r, c_s);
            g_stop = 1;
        }

    }

    /* Shutdown sequence:
     *  - signal all alive unit processes with SIGTERM
     *  - reap child processes
     *  - detach and destroy IPC objects
     */
    LOGW("stopping: sending SIGTERM to alive units...");
    printf("[CC] stopping: sending SIGTERM to alive units...\n");
    fflush(stdout);

    /* Try to get lock with interruptible version */
    if (sem_lock_intr(ctx.sem_id, SEM_GLOBAL_LOCK, &g_stop) == -1) {
        /* Couldn't get lock, just send signals without it */
        LOGW("[CC] Could not acquire lock for shutdown, sending SIGTERM anyway");
        for (int id = 1; id <= MAX_UNITS; id++) {
            pid_t pid = ctx.S->units[id].pid;
            if (pid > 1) {
                LOGD("[CC] Sending SIGTERM to unit %d (pid %d)", id, pid);
                kill(pid, SIGTERM);
            }
        }
    } else {
        /* We got the lock */
        for (int id = 1; id <= MAX_UNITS; id++) {
            pid_t pid = ctx.S->units[id].pid;
            if (pid > 1) {
                LOGD("[CC] Sending SIGTERM to unit %d (pid %d)", id, pid);
                kill(pid, SIGTERM);
            }
        }
        sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);
    }

    int status;
    pid_t pid;
    int waited = 0;
    int timeout_count = 0;
    const int MAX_WAIT_ATTEMPTS = 100; /* Prevent infinite loop */

    for (;;) {
        pid = waitpid(-1, &status, 0);   // BLOCK until a child exits
        if (pid > 0) {
            waited++;
            if (WIFEXITED(status)) {
                LOGD("[CC] reaped child %d, exit status %d", pid, WEXITSTATUS(status));
                printf("[CC] reaped child %d, exit status %d\n", pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                LOGD("[CC] reaped child %d, killed by signal %d", pid, WTERMSIG(status));
                printf("[CC] reaped child %d, killed by signal %d\n", pid, WTERMSIG(status));
            } else {
                printf("[CC] reaped child %d\n", pid);
            }
            continue;
        }

        if (pid == -1) {
            if (errno == EINTR) {
                timeout_count++;
                if (timeout_count > MAX_WAIT_ATTEMPTS) {
                    LOGW("[CC] waitpid interrupted too many times, giving up");
                    break;
                }
                continue;               // interrupted by signal -> retry
            }
            if (errno == ECHILD) {
                LOGD("[CC] no more children to reap");
                break;                  // no more children
            }
            LOGE("[CC] waitpid failed: %s", strerror(errno));
            perror("[CC] waitpid");
            break;
        }
    }

    LOGI("[CC] reaped %d children total", waited);
    printf("[CC] reaped %d children total\n", waited);
    fflush(stdout);

    /* cleanup IPC and exit */
    LOGD("[CC] Detaching and destroying IPC objects");
    if (ipc_detach(&ctx) == -1) {
        LOGE("[CC] ipc_detach failed: %s", strerror(errno));
    }
    if (ipc_destroy(&ctx) == -1) {
        LOGE("[CC] ipc_destroy failed: %s", strerror(errno));
    }

    printf("[CC] exit.\n");
    return 0;
}
