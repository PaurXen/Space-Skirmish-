#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

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

static volatile unit_id_t commander = 0;

static volatile sig_atomic_t g_stop = 0;

static volatile sig_atomic_t g_damage_pending = 0;

static ipc_ctx_t *g_ctx = NULL;
static unit_id_t g_unit_id = 0;

/* Cleanup function to detach IPC and mark unit dead on error */
static void cleanup_and_exit(int exit_code) {
    if (g_ctx && g_unit_id > 0) {
        LOGW("[SQ %u] cleanup_and_exit called, marking dead", g_unit_id);
        mark_dead(g_ctx, g_unit_id);
        ipc_detach(g_ctx);
    }
    exit(exit_code);
}

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

static void attack_action(ipc_ctx_t *ctx,
    unit_id_t unit_id,
    unit_stats_t *st,
    point_t *target_pri,
    int8_t *have_target_pri,
    unit_id_t *target_sec,
    int8_t *have_target_sec,
    int *aproach
)
{
    if (ctx->S->units[*target_sec].alive) {
        *target_pri = ctx->S->units[*target_sec].position;
        *have_target_pri = 1;
    }
    if (*have_target_sec) {
        unit_type_t target_type = (unit_type_t)ctx->S->units[*target_sec].type;
        *aproach = (int)unit_calculate_aproach(st->ba, target_type);
    }
}

static void guard_action(ipc_ctx_t *ctx,
    unit_id_t unit_id,
    unit_stats_t *st,
    point_t *target_pri,
    int8_t *have_target_pri,
    unit_id_t *target_sec,
    int8_t *have_target_sec,
    unit_id_t *target_ter,
    int8_t *have_target_ter,
    int *aproach
)
{
    // If no tertiary target or it's dead, clear and return
    if (!*have_target_ter || !ctx->S->units[*target_ter].alive) {
        *have_target_ter = 0;
        *target_ter = 0;
        return;
    }
    
    unit_stats_t t_st = unit_stats_for_type(ctx->S->units[*target_ter].type);
    point_t ter_pos = ctx->S->units[*target_ter].position;
    point_t my_pos = ctx->S->units[unit_id].position;
    
    // Calculate guard range (1/3 to 2/3 of DR, using middle)
    int16_t guard_range = st->dr / 2;
    int32_t dist_to_ter = dist2(my_pos, ter_pos);
    
    // Set primary target to guard position
    *target_pri = ter_pos;
    *have_target_pri = 1;
    *aproach = (dist_to_ter > guard_range * guard_range) ? guard_range : 1;
    
    // Detect enemies in area around tertiary target
    unit_id_t detect_id[MAX_UNITS];
    (void)memset(detect_id, 0, sizeof(detect_id));
    
    faction_t my_faction = ctx->S->units[unit_id].faction;
    int enemy_count = unit_radar(*target_ter, t_st, ctx->S->units, detect_id, my_faction);
    
    
    // If enemies detected near tertiary target, engage them
    if (enemy_count > 0 && !*have_target_sec) {
        *target_sec = unit_chose_secondary_target(ctx, detect_id, enemy_count, unit_id,
            target_pri, have_target_pri, have_target_sec);
        
        if (*have_target_sec) {
            unit_type_t target_type = (unit_type_t)ctx->S->units[*target_sec].type;
            *aproach = (int)unit_calculate_aproach(st->ba, target_type);
        }
    }
    
    // Validate secondary target - drop if outside both unit DR and tertiary target DR
    if (*have_target_sec && ctx->S->units[*target_sec].alive) {
        point_t sec_pos = ctx->S->units[*target_sec].position;
        
        // Drop target if outside both detection ranges
        if (!in_disk_i(sec_pos.x, sec_pos.y, my_pos.x, my_pos.y, st->dr) && 
            !in_disk_i(sec_pos.x, sec_pos.y, ter_pos.x, ter_pos.y, st->dr)) {
            *have_target_sec = 0;
            *target_sec = 0;
            *aproach = guard_range;
        }
    }
}


static void squadrone_action(ipc_ctx_t *ctx,
    unit_id_t unit_id,
    unit_stats_t *st,
    point_t *target_pri,
    int8_t *have_target_pri,
    unit_id_t *target_sec,
    int8_t *have_target_sec,
    unit_id_t *target_ter,
    int8_t *have_target_ter
)
{
    // Detect units
    unit_id_t detect_enemy_id[MAX_UNITS];
    (void)memset(detect_enemy_id, 0, sizeof(detect_enemy_id));
    st_points_t out_dmg[st->ba.count];
    (void)memset(out_dmg, 0, sizeof(out_dmg));
    int enemy_count = unit_radar(unit_id, *st, ctx->S->units, detect_enemy_id, ctx->S->units[unit_id].faction);
    
    // check for commander assignment replies
    mq_commander_rep_t cmd_rep;
    while (mq_try_recv_commander_reply(ctx->q_rep, &cmd_rep) == 1) {
        if (cmd_rep.status == 0) {
            commander = cmd_rep.commander_id;
            LOGD("[SQ %u] assigned to commander %u", unit_id, commander);
        }
    }
    
    // check for orders from commander
    mq_order_t order_msg = {0};
    while (mq_try_recv_order(ctx->q_req, &order_msg) == 1) {
        order = order_msg.order;
        LOGD("[SQ %u] received order %d with target %u", unit_id, order, order_msg.target_id);
        
        // Set targets based on order
        if (order == ATTACK && order_msg.target_id > 0) {
            *target_sec = order_msg.target_id;
            *have_target_sec = 1;
        } else if (order == GUARD && order_msg.target_id > 0) {
            if (ctx->S->units[order_msg.target_id].alive) {
                *target_ter = order_msg.target_id;
                *have_target_ter = 1;
            }
        }
    }
    
    // logging SQ commander id
    LOGD("[SQ %u] current commander %u state %u", unit_id, commander, ctx->S->units[commander].alive);

    // Only request commander if we don't have one or it's dead
    if (!commander || !ctx->S->units[commander].alive) {
        // find ally flagship/carrier and send commander request
        unit_id_t detect_ally_id[MAX_UNITS];
        (void)memset(detect_ally_id, 0, sizeof(detect_ally_id));
        int ally_count = unit_radar(unit_id, *st, ctx->S->units, detect_ally_id, FACTION_NONE);
        for (int i=0; i<ally_count; i++){
            unit_entity_t u = ctx->S->units[detect_ally_id[i]];
            if (TYPE_FLAGSHIP <= u.type && u.type <= TYPE_CARRIER) {
                // send commander request
                mq_commander_req_t req = {
                    .mtype = MSG_COMMANDER_REQ,
                    .sender = getpid(),
                    .sender_id = unit_id,
                    .req_id = (uint32_t)(unit_id * 1000 + ctx->S->ticks)
                };
                mq_send_commander_req(ctx->q_req, &req);
                LOGD("[SQ %u] sent commander request to potential BS %u", unit_id, detect_ally_id[i]);
                break;  // only send one request per tick
            }
        }
    }
    if (commander && !ctx->S->units[commander].alive) {
        commander = 0; // reset commander if dead
        order = PATROL; // default to PATROL
        LOGD("[SQ %u] commander %u is dead, resetting", unit_id, commander);
    }
    
    //DEBUG: Print detected units
    printf("[SQ %d] ", unit_id);
    printf("dr=%d [ ", st->dr);
    for (int i = 0; i<MAX_UNITS; i++)
        if (detect_enemy_id[i]) printf("%d,", detect_enemy_id[i]);
    printf(" ] ");
    LOGD("detected %d units", enemy_count);
    printf("detected %d units\n", enemy_count);
    fflush(stdout);

    int aproach = 1;
    point_t from = ctx->S->units[unit_id].position;

    switch (order)
        {
        case PATROL:
            patrol_action(ctx, unit_id, st, target_pri, have_target_pri, target_sec, have_target_sec, enemy_count, detect_enemy_id, from, &aproach);
            break;
        case ATTACK:
            attack_action(ctx, unit_id, st,  target_pri, have_target_pri, target_sec, have_target_sec, &aproach);
            break;
        case MOVE:
            break;
        case MOVE_ATTACK:
            break;
        case GUARD:
            guard_action(ctx, unit_id, st, target_pri, have_target_pri, target_sec, have_target_sec, target_ter, have_target_ter, &aproach);
            break;
        default:
            patrol_action(ctx, unit_id, st, target_pri, have_target_pri, target_sec, have_target_sec, enemy_count, detect_enemy_id, from, &aproach);
            break;
        }


        // Moving
    unit_move(ctx, unit_id, from, target_pri, st, aproach);



    // Second scan
    (void)memset(detect_enemy_id, 0, sizeof(detect_enemy_id));
    enemy_count = unit_radar(unit_id, *st, ctx->S->units, detect_enemy_id, ctx->S->units[unit_id].faction);

    // Checking if secondary target is within DR
    if (*have_target_sec){
        int8_t f = 0;
        for (int i = 0; i < enemy_count; i++ ){
            if (detect_enemy_id[i] == *target_sec) {f=1; break;}
        }
        if (!f){
            *have_target_sec = 0;
            *target_sec = 0;
        }
    }

    if (*have_target_sec) {
        (void)unit_weapon_shoot(ctx, unit_id, st, *target_sec, enemy_count, detect_enemy_id, out_dmg);
        LOGD("[SQ %d] ap=%d Sec target %d", unit_id, aproach, *target_sec);
        printf("[SQ %d] ap=%d Sec target %d\n", unit_id, aproach, *target_sec);
    }

        
}


int main(int argc, char **argv) {
    setpgid(getpid(), 0);
    
    const char *ftok_path = "./ipc.key";
    int faction = 0, type_i = 0, x = -1, y = -1;

    int8_t have_target_pri = 0;
    point_t primary_target = {0};
    int8_t have_target_sec = 0;
    unit_id_t secondary_target = 0;
    int8_t have_target_ter = 0;
    unit_id_t tertiary_target = 0;

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
        else if (!strcmp(argv[i], "--commander") && i + 1 < argc) commander = (unit_id_t)atoi(argv[++i]);
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
        LOGE("[SQ] ipc_attach failed: %s", strerror(errno));
        perror("ipc_attach");
        return 1;
    }
    g_ctx = &ctx;
    g_unit_id = unit_id;

    if (log_init("SQ", unit_id) == -1) {
        fprintf(stderr, "[SQ %u] log_init failed, continuing without logs\n", unit_id);
    }
    atexit(log_close);

    // ensure registry entry is correct
    if (sem_lock(ctx.sem_id, SEM_GLOBAL_LOCK) == -1) {
        LOGE("[SQ %u] failed to acquire initial lock: %s", unit_id, strerror(errno));
        perror("sem_lock");
        cleanup_and_exit(1);
    }
    ctx.S->units[unit_id].pid = getpid();
    ctx.S->units[unit_id].faction = (uint8_t)faction;
    ctx.S->units[unit_id].type = (uint8_t)type_i;
    ctx.S->units[unit_id].alive = 1;
    ctx.S->units[unit_id].position.x = (int16_t)x;
    ctx.S->units[unit_id].position.y = (int16_t)y;
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

        if (sem_lock_intr(ctx.sem_id, SEM_GLOBAL_LOCK, &g_stop) == -1) {
            if (g_stop) break;
            LOGE("[SQ %u] sem_lock_intr failed: %s", unit_id, strerror(errno));
            continue;
        }

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
            st_points_t old_hp = st.hp;
            compute_dmg_payload(&ctx, unit_id, &st);
            LOGD("[SQ %d] damage received: hp %ld -> %ld", unit_id, old_hp, st.hp);
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
        if (sem_lock_intr(ctx.sem_id, SEM_GLOBAL_LOCK, &g_stop) == -1) {
            if (g_stop) break;
            LOGE("[SQ %u] sem_lock_intr(action) failed: %s", unit_id, strerror(errno));
            if (sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1) == -1) {
                LOGE("sem_post_retry(TICK_DONE)");
            }
            break;
        }
        
        // perform action based on current order
        squadrone_action(&ctx, unit_id, &st,
                        &primary_target, &have_target_pri,
                        &secondary_target, &have_target_sec,
                        &tertiary_target, &have_target_ter);

        point_t pos = ctx.S->units[unit_id].position;
        sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

        if ((t % 1) == 0) {
            LOGI("[BS %u] tick=%u pos=(%d,%d) target=(%d,%d) dt2=%d  hp=%d, sp=%d, fa=%d",
                unit_id, t, pos.x, pos.y, primary_target.x, primary_target.y,
                dist2(pos, primary_target), st.hp, st.sp, faction);
        }

        if (sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1) == -1) {
            LOGE("sem_post_retry(TICK_DONE)");
            perror("sem_post_retry(TICK_DONE)");
            break;
        }
    }

    LOGW("[SQ %u] terminating, cleaning registry/grid", unit_id);
    printf("[SQ %u] terminating, cleaning registry/grid\n", unit_id);
    fflush(stdout);

    mark_dead(&ctx, unit_id);
    ipc_detach(&ctx);
    return 0;
}
