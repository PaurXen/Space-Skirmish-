#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "ipc/ipc_context.h"
#include "ipc/semaphores.h"
#include "ipc/shared.h"

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

static void mark_dead(ipc_ctx_t *ctx, uint16_t unit_id) {
    sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);

    if (unit_id <= MAX_UNITS) {
        ctx->S->units[unit_id].alive = 0;

        int x = ctx->S->units[unit_id].x;
        int y = ctx->S->units[unit_id].y;
        if (x >= 0 && x < N && y >= 0 && y < M) {
            if (ctx->S->grid[x][y] == (unit_id_t)unit_id)
                ctx->S->grid[x][y] = 0;
        }
    }

    sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
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

    ipc_ctx_t ctx;
    if (ipc_attach(&ctx, ftok_path) == -1) {
        perror("ipc_attach");
        return 1;
    }

    // ensure registry PID is correct (independent consistency)
    sem_lock(ctx.sem_id, SEM_GLOBAL_LOCK);
    ctx.S->units[unit_id].pid = getpid();
    ctx.S->units[unit_id].faction = (uint8_t)faction;
    ctx.S->units[unit_id].type = (uint8_t)type;
    ctx.S->units[unit_id].alive = 1;
    ctx.S->units[unit_id].x = (int16_t)x;
    ctx.S->units[unit_id].y = (int16_t)y;
    sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

    printf("[BS %u] pid=%d faction=%d type=%d pos=(%d,%d)\n",
           unit_id, (int)getpid(), faction, type, x, y);

    while (!g_stop) {
        // wait for CC permit (interruptible)
        if (sem_wait_intr(ctx.sem_id, SEM_TICK_START, -1, &g_stop) == -1) {
            if (g_stop) break;
            continue;
        }

        sem_lock(ctx.sem_id, SEM_GLOBAL_LOCK);
        uint32_t t = ctx.S->ticks;

        // ensure max 1 action per tick
        if (ctx.S->last_step_tick[unit_id] != t) {
            ctx.S->last_step_tick[unit_id] = t;

            // TODO: real per-tick logic here (move/shoot/detect)
            if ((t % 25) == 0) {
                printf("[BS %u] tick=%u step done (random order)\n", unit_id, t);
                fflush(stdout);
            }
        }

        if (ctx.S->tick_done < 65535) ctx.S->tick_done++;
        sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

        // notify CC: done
        if (sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1) == -1) {
            perror("sem_post_retry(TICK_DONE)");
            break;
        }
    }

    printf("[BS %u] terminating, cleaning registry/grid\n", unit_id);
    mark_dead(&ctx, unit_id);

    ipc_detach(&ctx);
    return 0;
}
