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
#include "ipc/ipc_mesq.h"

#include "weapon_stats.h"
#include "unit_stats.h"
#include "unit_logic.h"
#include "unit_ipc.h"
#include "log.h"

static volatile unit_order_t order = PATROL;

static volatile sig_atomic_t g_stop = 0;

static volatile sig_atomic_t g_damage_pending = 0;

static void on_term(int sig) {
    (void)sig;
    g_stop = 1;
}

static void on_damage(int sig) {
    (void)sig;
    LOGD("g_damage_pending flag raised. (g_damage_pending = 1)");
    g_damage_pending = 1;
}

static void patrol_action(ipc_ctx_t *ctx,
    unit_id_t unit_id,
    unit_stats_t *st,
    point_t *target_pri,
    int8_t *have_target_pri,
    unit_id_t *target_sec,
    int8_t *have_target_sec,
    int count,
    unit_id_t *detect_id,
    point_t from,
    int *aproach

)
{
    

    // chosing enemy target
    if (!*have_target_sec && count != 0){
        *target_sec = unit_chose_secondary_target(ctx, detect_id, count, unit_id,
            target_pri, have_target_pri, have_target_sec);
    }



    // Chosing patrol point
    if (*have_target_pri && in_disk_i(from.x, from.y, target_pri->x, target_pri->y, *aproach)) *have_target_pri = 0;
    if (!*have_target_pri) {
        *have_target_pri = unit_chose_patrol_point(ctx, unit_id, target_pri, *st); 
    }
    LOGD("[SQ %u] target (%d,%d)", unit_id, target_pri->x, target_pri->y);
    if (*have_target_sec) {
        unit_type_t target_type = (unit_type_t)ctx->S->units[*target_sec].type;
        *aproach = (int)unit_calculate_aproach(st->ba, target_type);
    }


    
}

static void squadrone_action(ipc_ctx_t *ctx,
    unit_id_t unit_id,
    unit_stats_t *st,
    point_t *target_pri,
    int8_t *have_target_pri,
    unit_id_t *target_sec,
    int8_t *have_target_sec
)
{
    // Detect units
    unit_id_t detect_id[MAX_UNITS];
    (void)memset(detect_id, 0, sizeof(detect_id));
    st_points_t out_dmg[st->ba.count];
    (void)memset(out_dmg, 0, sizeof(out_dmg));
    int count = unit_radar(unit_id, *st, ctx->S->units, detect_id, ctx->S->units[unit_id].faction);

    //DEBUG: Print detected units
    printf("[SQ %d] ", unit_id);
    printf("dr=%d [ ", st->dr);
    for (int i = 0; i<MAX_UNITS; i++)
        if (detect_id[i]) printf("%d,", detect_id[i]);
    printf(" ] ");
    LOGD("detected %d units", count);
    printf("detected %d units\n", count);
    fflush(stdout);

    int aproach = 1;
    point_t from = ctx->S->units[unit_id].position;

    switch (order)
        {
        case PATROL:
            patrol_action(ctx, unit_id, st, target_pri, have_target_pri, target_sec, have_target_sec, count, detect_id, from, &aproach);
            break;
        case ATTACK:
            break;
        case MOVE:
            break;
        case MOVE_ATTACK:
            break;
        case GUARD:
            break;
        default:
            // patrol_action(&ctx);
            break;
        }


        // Moving
    unit_move(ctx, unit_id, from, target_pri, st, aproach);



    // Second scan
    (void)memset(detect_id, 0, sizeof(detect_id));
    count = unit_radar(unit_id, *st, ctx->S->units, detect_id, ctx->S->units[unit_id].faction);

    // Checking if secondary target is within DR
    if (*have_target_sec){
        int8_t f = 0;
        for (int i = 0; i < count; i++ ){
            if (detect_id[i] == *target_sec) {f=1; break;}
        }
        if (!f){
            *have_target_sec = 0;
            *target_sec = 0;
        }
    }

    if (*have_target_sec) {
        (void)unit_weapon_shoot(ctx, unit_id, st, *target_sec, count, detect_id, out_dmg);
        LOGD("[SQ %d] ap=%d Sec target %d", unit_id, aproach, *target_sec);
        printf("[SQ %d] ap=%d Sec target %d\n", unit_id, aproach, *target_sec);
    }

        
}


int main(int argc, char **argv) {
    setpgid(getpid(), 0);
    
    const char *ftok_path = "./ipc.key";
    int faction = 0, type_i = 0, x = -1, y = -1;

    int8_t have_target_pri = 0;
    int8_t have_target_sec = 0;
    point_t primary_target = {0};
    unit_id_t secondary_target = 0;
    unit_id_t unit_id = 0;

    unit_type_t type;
    unit_stats_t st;

    ipc_ctx_t ctx;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--ftok") && i + 1 < argc) ftok_path = argv[++i];
        else if (!strcmp(argv[i], "--unit") && i + 1 < argc) unit_id = (unit_id_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--faction") && i + 1 < argc) faction = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--type") && i + 1 < argc) type_i = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--x") && i + 1 < argc) x = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--y") && i + 1 < argc) y = atoi(argv[++i]);
    }

    if (unit_id <= 0 || unit_id > MAX_UNITS) {
        LOGE("[SQ] invalid unit_id");
        fprintf(stderr, "[SQ] invalid unit_id\n");
        return 1;
    }

    // signals
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_term;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &(struct sigaction){ .sa_handler = SIG_IGN }, NULL);

    // damage payload
    struct sigaction sa2;
    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = on_damage;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = 0;
    sigaction(SIGRTMAX, &sa2, NULL);

    //RNG per process
    srand((unsigned)(time(NULL) ^ (getpid() << 16)));

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
    // if (in_bounds(x, y, M, N) && ctx.S->grid[x][y] == 0) ctx.S->grid[x][y] = unit_id;
    sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

    type = (unit_id_t)type_i;
    st = unit_stats_for_type(type);

    LOGI("pid=%d faction=%d type=%d pos=(%d,%d)", (int)getpid(), faction, type_i, x, y);
    printf("[SQ %u] pid=%d faction=%d type=%d pos=(%d,%d)\n",
           unit_id, (int)getpid(), faction, type_i, x, y);
    fflush(stdout);

    while (!g_stop) {
        // wait for tick to start
        if (sem_wait_intr(ctx.sem_id, SEM_TICK_START, -1, &g_stop) == -1) {
            if (g_stop) break;
            continue;
        }

        if (sem_lock_intr(ctx.sem_id, SEM_GLOBAL_LOCK, &g_stop) == -1) break;

        uint32_t t;
        uint8_t alive;
        point_t cp;

        // take a snapshot for this tick (minimal time under lock)
        t = ctx.S->ticks;
        alive = ctx.S->units[unit_id].alive;
        cp = (point_t)ctx.S->units[unit_id].position;
        if (!alive) {
            sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);
            (void)sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1);
            break;
        }

        if (g_damage_pending) {
            g_damage_pending = 0;
            LOGD("[BS %d] hp=%d dmg_payload=%ld", unit_id, st.hp, ctx.S->units[unit_id].dmg_payload);
            compute_dmg_payload(&ctx, unit_id, &st);
            LOGD("[BS %d] damage computed hp=%d", unit_id, st.hp);
        }

        if (st.hp <= 0) {
            LOGD("[BS %d] mark as dead", unit_id);
            mark_dead(&ctx, unit_id);
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

        sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

        if (!alive) {
            (void)sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1);
            break;
        }
        
        // perform action based on current order
        LOGD("[SQ %u] taking order | tick=%u pos=(%d,%d) order=%d",
             unit_id, t, cp.x, cp.y, order);
        if (sem_lock_intr(ctx.sem_id, SEM_GLOBAL_LOCK, &g_stop) == -1) break;
        
        // perform action based on current order
        squadrone_action(&ctx, unit_id, &st, &primary_target, &have_target_pri, &secondary_target, &have_target_sec);

        sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

        if (sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1) == -1) break;
    }

    LOGW("[SQ %u] terminating, cleaning registry/grid", unit_id);
    printf("[SQ %u] terminating, cleaning registry/grid\n", unit_id);
    fflush(stdout);

    mark_dead(&ctx, unit_id);
    ipc_detach(&ctx);
    return 0;
}
