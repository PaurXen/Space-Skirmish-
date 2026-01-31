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

static void on_damage(int sig) {
    (void)sig;
    LOGD("g_damage_pending flag raised. (g_damage_pending = 1)");
    g_damage_pending = 1;
}

static void on_term(int sig) {
    (void)sig;
    LOGD("g_stop flag raised. (g_stop = 1)");
    g_stop = 1;
}

static void print_stats(unit_id_t id, unit_stats_t st) {
    printf("[BS %d] STATS:\n");
    printf("hp=%d\n", st.hp);
    printf("sh=%d\n", st.sh);
    printf("en=%d\n", st.en);
    printf("sp=%d\n", st.sp);
    printf("si=%d\n", st.si);
    printf("dr=%d\n", st.dr);
    printf("ba.count=%d\n", st.ba.count);
    for (int i=0;i<st.ba.count;i++){
        printf("Weapon %i:", i);
        weapon_stats_t ar = st.ba.arr[i];
        printf(" dmg=%d", ar.dmg);
        printf(" range=%d", ar.range);
        printf(" type=%d", ar.type);
        printf(" target=%d\n", ar.w_target);
    }
    fflush(stdout);
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
    LOGD("[BS %u] target (%d,%d)", unit_id, target_pri->x, target_pri->y);
    if (*have_target_sec) {
        unit_type_t target_type = (unit_type_t)ctx->S->units[*target_sec].type;
        *aproach = (int)unit_calculate_aproach(st->ba, target_type);
    }


    
}

static void battleship_action(ipc_ctx_t *ctx,
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
    printf("[BS %d] ", unit_id);
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
        LOGD("[BS %d] ap=%d Sec target %d", unit_id, aproach, *target_sec);
        printf("[BS %d] ap=%d Sec target %d\n", unit_id, aproach, *target_sec);
    }

        
}

int main(int argc, char **argv) {
    setpgid(getpid(), 0);
    const char *ftok_path = "./ipc.key";
    int faction = 0, type_i = 0, x = -1, y = -1;
    
    uint32_t req_id_counter = 0;
    
    int8_t have_target_pri = 0;
    int8_t have_target_sec = 0;
    point_t primary_target = {0};
    unit_id_t secondary_target = 0;
    unit_id_t unit_id = 0;
    int16_t spawn_range = 2;

    unit_id_t underlings[MAX_UNITS];
    memset(underlings, 0, sizeof(underlings));
    
    unit_type_t type;
    unit_stats_t st;

    ipc_ctx_t ctx;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--ftok") && i + 1 < argc) ftok_path = argv[++i];
        else if (!strcmp(argv[i], "--unit") && i + 1 < argc) unit_id = (uint16_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--faction") && i + 1 < argc) faction = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--type") && i + 1 < argc) type_i = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--x") && i + 1 < argc) x = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--y") && i + 1 < argc) y = atoi(argv[++i]);
    }

    if (unit_id == 0 || unit_id > MAX_UNITS) {
        LOGE("[BS] invalid unit_id");
        fprintf(stderr, "[BS] invalid unit_id\n");
        return 1;
    }

    // signals
    // KILL signal
    struct sigaction sa1;
    memset(&sa1, 0, sizeof(sa1));
    sa1.sa_handler = on_term;
    sigaction(SIGTERM, &sa1, NULL);

    // damage payload
    struct sigaction sa2;
    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = on_damage;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = 0;
    sigaction(SIGRTMAX, &sa2, NULL);

    // units ignore SIGINT; only CC handles Ctrl+C and sends SIGTERM
    signal(SIGINT, SIG_IGN);

    // RNG per process
    srand((unsigned)(time(NULL) ^ (getpid() << 16)));

    if (ipc_attach(&ctx, ftok_path) == -1) {
        perror("ipc_attach");
        return 1;
    }


    // ensure registry entry is correct
    sem_lock(ctx.sem_id, SEM_GLOBAL_LOCK);
    ctx.S->units[unit_id].pid = getpid();
    ctx.S->units[unit_id].faction = (uint8_t)faction;
    ctx.S->units[unit_id].type = (uint8_t)type_i;
    ctx.S->units[unit_id].alive = 1;
    ctx.S->units[unit_id].position.x = (int16_t)x;
    ctx.S->units[unit_id].position.y = (int16_t)y;
    ctx.S->units[unit_id].dmg_payload = 0;
    sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

    log_init("BS", unit_id);
    atexit(log_close);

    type = (unit_type_t)type_i;
    st = unit_stats_for_type(type);

    // print_stats(unit_id, st);

    // Adjust these prints to match your real struct fields:
    LOGI("pid=%d faction=%d type=%d pos=(%d,%d) SP=%d DR=%d",
           (int)getpid(), faction, type_i, x, y, st.sp, st.dr);
    printf("[BS %u] pid=%d faction=%d type=%d pos=(%d,%d) SP=%d DR=%d\n",
           unit_id, (int)getpid(), faction, type_i, x, y, st.sp, st.dr);
    fflush(stdout);

        while (!g_stop) {
                // wait for tick start
        if (sem_wait_intr(ctx.sem_id, SEM_TICK_START, -1, &g_stop) == -1) {
            if (g_stop) break;
            continue;
        }
        
        if (sem_lock_intr(ctx.sem_id, SEM_GLOBAL_LOCK, &g_stop) == -1) {
            break;
        }
        
        uint32_t t;
        uint8_t alive;
        point_t cp;

        // take a snapshot for this tick (minimal time under lock)
        t = ctx.S->ticks;
        alive = ctx.S->units[unit_id].alive;
        cp = (point_t)ctx.S->units[unit_id].position;
        if (alive == 0) {
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
        

        // ensure at most one action per tick
        if (ctx.S->last_step_tick[unit_id] == t) {
            sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);
            (void)sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1);
            continue;
        }
        ctx.S->last_step_tick[unit_id] = t;

        mq_spawn_rep_t rep;
        while (mq_try_recv_reply(ctx.q_rep, &rep) == 1) {
            if (rep.status == 0) st.fb.current++;
            // for (int i=0; i<MAX_UNITS;i++);
        }

        sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);
        
        if (!alive) {
            (void)sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1);
            break;
        }
        
        // perform action based on current order
        LOGD("[BS %u] taking order | tick=%u pos=(%d,%d) order=%d",
             unit_id, t, cp.x, cp.y, order);
        if (sem_lock_intr(ctx.sem_id, SEM_GLOBAL_LOCK, &g_stop) == -1) break;
        
        // perform action based on current order
        battleship_action(&ctx, unit_id, &st, &primary_target, &have_target_pri, &secondary_target, &have_target_sec);


        LOGD("[BS %u] fighter bay: capacity=%d current=%d",
            unit_id, st.fb.capacity, st.fb.current);
        point_t pos = ctx.S->units[unit_id].position;
        if (st.fb.capacity > st.fb.current) {
            point_t out;
            radar_pick_random_point_in_circle(pos.x, pos.y, spawn_range, M, N, &out);
            mq_spawn_req_t req = {
            .mtype = MSG_SPAWN,
            .sender = getpid(),
            .pos = out,
            .sender_id = unit_id,
            .utype = st.fb.sq_types[st.fb.current],
            .req_id = ++req_id_counter
        };
        mq_send_spawn(ctx.q_req, &req);
        LOGD("[BS %u] request to spawn squadron at (%d,%d)",
            unit_id, out.x, out.y);
        }
        sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

        

        

        
        // debug print sometimes
        if ((t % 1) == 0) {
            LOGI("[BS %u] tick=%u pos=(%d,%d) target=(%d,%d) dt2=%d  hp=%d, sp=%d, fa=%d",
                unit_id, t, pos.x, pos.y, primary_target.x, primary_target.y,
                dist2(pos, primary_target), st.hp, st.sp, faction);
            printf("[BS %u] tick=%u pos=(%d,%d) target=(%d,%d) dt2=%d  hp=%d, sp=%d, fa=%d\n",
                unit_id, t, pos.x, pos.y, primary_target.x, primary_target.y,
                dist2(pos, primary_target), st.hp, st.sp, faction);
            fflush(stdout);
        }

        // printf("[BS %d]pos x=%d y=%d\n",unit_id,nx,ny);
        // fflush(stdout);

                // notify CC done
        if (sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1) == -1) {
            LOGE("sem_post_retry(TICK_DONE)");
            perror("sem_post_retry(TICK_DONE)");
            break;
        }
        // printf("[BS %d] posted\n", unit_id);
        // fflush(stdout);
    }

    LOGW("[BS %u] terminating, cleaning registry/grid", unit_id);
    printf("[BS %u] terminating, cleaning registry/grid\n", unit_id);
    fflush(stdout);

    mark_dead(&ctx, unit_id);
    ipc_detach(&ctx);
    return 0;
}
