#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#include "ipc/ipc_context.h"
#include "ipc/semaphores.h"
#include "ipc/shared.h"

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

static uint16_t alloc_unit_id(ipc_ctx_t *ctx) {
    uint16_t id = 0;

    sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
    if (ctx->S->next_unit_id > MAX_UNITS) {
        sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
        fprintf(stderr, "No more unit IDs available (MAX_UNITS=%d)\n", MAX_UNITS);
        return 0;
    }
    id = ctx->S->next_unit_id++;
    sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);

    return id;
}

static void register_unit(ipc_ctx_t *ctx, uint16_t unit_id, pid_t pid,
                          faction_t faction, unit_type_t type, int x, int y)
{
    sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);

    ctx->S->units[unit_id].pid = pid;
    ctx->S->units[unit_id].faction = (uint8_t)faction;
    ctx->S->units[unit_id].type = (uint8_t)type;
    ctx->S->units[unit_id].alive = 1;
    ctx->S->units[unit_id].x = (uint16_t)x;
    ctx->S->units[unit_id].y = (uint16_t)y;

    if (x>=0 && x<N && y>=0 && y<M) {
        if (ctx->S->grid[x][y] == 0)
            ctx->S->grid[x][y] = (unit_id_t)unit_id;
        else
            fprintf(stderr, "Warning: grid[%d][%d] occupied by unit_id=%d\n",
                    x, y, (int)ctx->S->grid[x][y]);
            }

    ctx->S->unit_count++;

    sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
}

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
    return pid;
}

int main(int argc, char **argv) {
    const char *ftok_path = "./ipc.key";
    const char *battleship = "./battleship";

    const useconds_t tick_us = 200 * 1000;

    for (int i=1; i<argc;i++) {
        if (!strcmp(argv[i], "--ftok") && i+1<argc) ftok_path = argv[++i];
        else if (!strcmp(argv[i], "--battleship") && i+1<argc) battleship = argv[++i];
    }

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

    printf("[CC] shm_id=%d sem_id=%d spawned 4 battleships. Ctrl+C to stop.\n",ctx.shm_id, ctx.sem_id);

    while (!g_stop) {
        usleep(tick_us);

        sem_lock(ctx.sem_id, SEM_GLOBAL_LOCK);
        ctx.S->ticks++;
        uint32_t t =ctx.S->ticks;

        uint16_t alive = 0;
        for (int id=1; id<=MAX_UNITS; id++) if (ctx.S->units[id].alive) alive++;
        
        ctx.S->tick_expected = alive;
        ctx.S->tick_done = 0;

        sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

        // 1) release exactly one "start" permit per alive unit
        for (unsigned i=0; i<alive; i++) {
            if (sem_post_retry(ctx.sem_id, SEM_TICK_START, +1) == -1) {
                perror("sem_post_retry(TICK_START)");
                g_stop = 1;
                break;
            }
        }

        // 2) wait until all alive units report "done"
        for (unsigned i=0; i<alive; i++) {
            if (sem_wait_intr(ctx.sem_id, SEM_TICK_DONE, -1, &g_stop) == -1) {
                break; // Ctrl+C or error
            }
        }

        if ((t % 10) == 0) {
            printf("[CC] ticks=%u alive_units=%u\n", t, alive);
            fflush(stdout);
        }
    }

    printf("[CC] stopping: sending SIGTERM to alive units...\n");

    sem_lock(ctx.sem_id, SEM_GLOBAL_LOCK);
    for (int id=1; id<=MAX_UNITS; id++) {
        if (ctx.S->units[id].alive && ctx.S->units[id].pid > 1) {
            kill(ctx.S->units[id].pid, SIGTERM);
        }
    }
    sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

    // reap children
    while (wait(NULL) > 0) {}

    // detach + destroy IPC
    ipc_detach(&ctx);
    ipc_destroy(&ctx);

    printf("[CC] exit.\n");
    return 0;
}
