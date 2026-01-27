#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include "ipc/ipc_context.h"
#include "ipc/semaphores.h"
#include "ipc/shared.h"
#include "log.h"

static volatile sig_atomic_t g_stop = 0;

static void on_term(int sig) {
    (void)sig;
    g_stop = 1;
}

static void mark_dead_min(ipc_ctx_t *ctx, unit_id_t id) {
    sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);

    if (id > 0 && id <= MAX_UNITS) {
        point_t p = ctx->S->units[id].position;
        if (p.x >= 0 && p.x < M && p.y >= 0 && p.y < N) {
            if (ctx->S->grid[p.x][p.y] == id) ctx->S->grid[p.x][p.y] = 0;
        }
        ctx->S->units[id].alive = 0;
        // keep pid for CC cleanup_dead_units() to SIGTERM/reap, then it clears pid
    }

    sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
}

int main(int argc, char **argv) {
    setpgid(getpid(), 0);

    const char *ftok_path = "./ipc.key";
    int faction = 0, type_i = 0, x = -1, y = -1;
    unit_id_t unit_id = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--ftok") && i + 1 < argc) ftok_path = argv[++i];
        else if (!strcmp(argv[i], "--unit") && i + 1 < argc) unit_id = (unit_id_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--faction") && i + 1 < argc) faction = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--type") && i + 1 < argc) type_i = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--x") && i + 1 < argc) x = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--y") && i + 1 < argc) y = atoi(argv[++i]);
    }

    if (unit_id <= 0 || unit_id > MAX_UNITS) {
        fprintf(stderr, "[SQ] invalid unit_id\n");
        return 1;
    }

    // signals
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_term;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &(struct sigaction){ .sa_handler = SIG_IGN }, NULL);

    ipc_ctx_t ctx;
    if (ipc_attach(&ctx, ftok_path) == -1) {
        perror("ipc_attach");
        return 1;
    }

    log_init("SQ", unit_id);
    atexit(log_close);

    // ensure registry entry is correct
    sem_lock(ctx.sem_id, SEM_GLOBAL_LOCK);
    ctx.S->units[unit_id].pid = getpid();
    ctx.S->units[unit_id].faction = (uint8_t)faction;
    ctx.S->units[unit_id].type = (uint8_t)type_i;
    ctx.S->units[unit_id].alive = 1;
    ctx.S->units[unit_id].position.x = (int16_t)x;
    ctx.S->units[unit_id].position.y = (int16_t)y;
    ctx.S->units[unit_id].dmg_payload = 0;
    if (x >= 0 && x < M && y >= 0 && y < N && ctx.S->grid[x][y] == 0) ctx.S->grid[x][y] = unit_id;
    sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

    LOGI("pid=%d faction=%d type=%d pos=(%d,%d)", (int)getpid(), faction, type_i, x, y);
    printf("[SQ %u] pid=%d faction=%d type=%d pos=(%d,%d)\n",
           unit_id, (int)getpid(), faction, type_i, x, y);
    fflush(stdout);

    while (!g_stop) {
        if (sem_wait_intr(ctx.sem_id, SEM_TICK_START, -1, &g_stop) == -1) {
            if (g_stop) break;
            continue;
        }

        if (sem_lock_intr(ctx.sem_id, SEM_GLOBAL_LOCK, &g_stop) == -1) break;

        uint32_t t = ctx.S->ticks;
        uint8_t alive = ctx.S->units[unit_id].alive;

        if (!alive) {
            sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);
            (void)sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1);
            break;
        }

        if (ctx.S->last_step_tick[unit_id] == t) {
            sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);
            (void)sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1);
            continue;
        }
        ctx.S->last_step_tick[unit_id] = t;

        // dummy behavior: do nothing this tick
        sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

        if (sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1) == -1) break;
    }

    LOGW("[SQ %u] terminating", unit_id);
    printf("[SQ %u] terminating\n", unit_id);
    fflush(stdout);

    mark_dead_min(&ctx, unit_id);
    ipc_detach(&ctx);
    return 0;
}
