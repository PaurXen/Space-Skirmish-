#include "unit_ipc.h"
#include "ipc/ipc_mesq.h"
#include "unit_size.h"
#include "unit_stats.h"



unit_id_t check_if_occupied(ipc_ctx_t *ctx, point_t point) {
    // sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
    unit_id_t unit_id = ctx->S->grid[point.x][point.y];
    // sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
    if (0 < unit_id && unit_id <=MAX_UNITS) return unit_id;
    return 0;

    
}

void unit_change_position(ipc_ctx_t *ctx, unit_id_t unit_id, point_t new_pos) {
    // sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
    point_t old_pos = ctx->S->units[unit_id].position;
    unit_type_t type = (unit_type_t)ctx->S->units[unit_id].type;
    unit_stats_t stats = unit_stats_for_type(type);
    st_points_t size = stats.si;
    
    // Remove unit from old position (all cells)
    remove_unit_from_grid(ctx, unit_id, old_pos, size);
    
    // Place unit at new position (all cells)
    place_unit_on_grid(ctx, unit_id, new_pos, size);

    // Update unit's center position
    ctx->S->units[unit_id].position = new_pos;
    // sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
}

point_t get_target_position(ipc_ctx_t *ctx, unit_id_t attacker_id, unit_id_t target_id) {
    point_t attacker_pos = ctx->S->units[attacker_id].position;
    point_t target_center = ctx->S->units[target_id].position;
    unit_type_t target_type = (unit_type_t)ctx->S->units[target_id].type;
    unit_stats_t target_stats = unit_stats_for_type(target_type);
    
    // Get closest cell of target to attacker
    return get_closest_cell_to_attacker(attacker_pos, target_center, target_stats.si);
}


void unit_add_to_dmg_payload(
    ipc_ctx_t *ctx,
    unit_id_t target_id,
    st_points_t dmg
) {
    if (ctx->S->units[target_id].pid <= 0) return;
    
    pid_t target_pid = ctx->S->units[target_id].pid;
    
    mq_damage_t dmg_msg = {
        .mtype = target_pid,
        .target_id = target_id,
        .damage = dmg
    };
    mq_send_damage(ctx->q_req, &dmg_msg);
    
    // Signal the target unit that damage is waiting
    kill(target_pid, SIGRTMAX);
}

void compute_dmg_payload(ipc_ctx_t *ctx, unit_id_t unit_id, unit_stats_t *st){
    st_points_t total_damage = 0;
    mq_damage_t dmg_msg;
    int msg_count = 0;
    
    // Process all pending damage messages
    while (mq_try_recv_damage(ctx->q_req, &dmg_msg) == 1) {
        msg_count++;
        if (dmg_msg.target_id == unit_id) {
            total_damage += dmg_msg.damage;
        } else {
            // This shouldn't happen - message sent to wrong PID?
            fprintf(stderr, "[WARN] Unit %u received damage message for unit %u (damage=%ld)\n", 
                    unit_id, dmg_msg.target_id, dmg_msg.damage);
        }
    }
    
    if (msg_count > 0 && total_damage == 0) {
        fprintf(stderr, "[WARN] Unit %u received %d damage messages but total_damage=0\n", 
                unit_id, msg_count);
    }
    
    if (total_damage > 0) {
        if (st->hp <= total_damage) {
            st->hp = 0;
        } else {
            st->hp -= total_damage;
        }
    }
}

st_points_t unit_weapon_shoot(ipc_ctx_t *ctx,
    unit_id_t unit_id,
    unit_stats_t *st,
    unit_id_t target_sec,
    int count,
    unit_id_t *detect_id,
    st_points_t *out_dmg
)
{
    unit_entity_t unit = ctx->S->units[unit_id];
    unit_entity_t target;
    int8_t arr_count = st->ba.count;
    weapon_stats_t weapon;
    st_points_t total_dmg = 0;
    float ac_max = 0;
    int i_max = -1;

    for (int i=0; i < arr_count; i++){
        target = ctx->S->units[target_sec];
        weapon = st->ba.arr[i];
        weapon.w_target = 0;
        st->ba.arr[i].w_target = weapon.w_target;
        float accuracy = accuracy_multiplier(weapon.type, target.type); 
        if (!accuracy || !in_disk_i(
                                target.position.x, target.position.y,
                                unit.position.x, unit.position.y,
                                weapon.range)
        )
        {
            for (int j=0; j<count; j++){
                target = ctx->S->units[detect_id[j]];
                accuracy = accuracy_multiplier(weapon.type, target.type); 
                if (detect_id != target_sec && accuracy
                    && in_disk_i(
                                target.position.x, target.position.y,
                                unit.position.x, unit.position.y,
                                weapon.range)
                )
                {
                    if (accuracy>ac_max) {ac_max = accuracy; i_max = j;}
                }
            if (i_max == -1) weapon.w_target = 0;
            else weapon.w_target = detect_id[i_max];
            }
        }else {
            weapon.w_target = target_sec;
        }
        st->ba.arr[i].w_target = weapon.w_target;  
        if (weapon.w_target) {
            st_points_t dmg = damage_to_target(&unit, &target, &st->ba.arr[i], accuracy);
            out_dmg[i] = dmg;
            if (dmg) unit_add_to_dmg_payload(ctx, target_sec, dmg);
        }
    }
    char buf[256];
    int off = 0;

    off += snprintf(buf + off, sizeof(buf) - off, "[BS %d] damage to units: [ ", unit_id);

    for (int i = 0; i < st->ba.count; i++) {
        off += snprintf(buf + off, sizeof(buf) - off,
                        "%d:%ld, ",st->ba.arr[i].w_target, out_dmg[i]);
    }
    snprintf(buf + off, sizeof(buf) - off, "]");

    LOGD("%s", buf);
    printf("%s\n", buf);
    fflush(stdout);
    return total_dmg;
}

unit_id_t unit_chose_secondary_target(ipc_ctx_t *ctx,
    unit_id_t *detected_id,
    int count,
    unit_id_t unit_id,
    point_t *target_pri,
    int8_t *have_target_pri,
    int8_t *have_target_sec
)
{
    float max_multi = 0;
    unit_id_t max_id = 0;
    float multi = 0;

    unit_type_t t_type = DUMMY;
    unit_type_t u_type = DUMMY;

    unit_entity_t *u = ctx->S->units;
    for (int i = 0; i < count; i++){
        // if (u[detected_id[i]].faction == u[unit_id].faction) continue;
        t_type = u[detected_id[i]].type;
        u_type = u[unit_id].type;
        multi = damage_multiplyer(u_type, t_type);
        if (max_multi > multi) continue;
        max_multi = multi;
        max_id = detected_id[i];
    }
    if (!max_id) return 0;
    
    *target_pri = get_target_position(ctx, unit_id, max_id);
    *have_target_pri = 1;
    *have_target_sec = 1;

    return max_id;

}

int8_t unit_chose_patrol_point(ipc_ctx_t *ctx,
    unit_id_t unit_id,
    point_t *target_pri,
    unit_stats_t st
)
{
    // pick new patrol target
    if (radar_pick_random_point_on_circle_border(
            ctx->S->units[unit_id].position,
            st.dr,
            M, N,
            st.si,
            unit_id,
            ctx,
            target_pri)) {
        LOGD("[BS %u] picked new patrol target (%d,%d)",
                unit_id, target_pri->x, target_pri->y);
        return 1;
    } else {
        LOGD("[BS %u] no valid patrol target found", unit_id);
        return 0;
    }
}

void unit_move(ipc_ctx_t *ctx,
    unit_id_t unit_id,
    point_t from,
    point_t *target_pri,
    unit_stats_t *st,
    int aproach
)
{
    point_t goal = from;
    point_t next = from;
    
    // Goal chosen from DR, next step chosen from SP toward that goal
    (void)unit_compute_goal_for_tick_dr(from, *target_pri, st->dr, M, N, &goal);
    (void)unit_next_step_towards_dr(from, goal, st->sp, st->dr, aproach, M, N, ctx->S->grid, unit_id, st->si, ctx, &next);

    unit_change_position(ctx, unit_id, next);
}

void mark_dead(ipc_ctx_t *ctx, unit_id_t unit_id) {
    // sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);

    if (unit_id <= MAX_UNITS) {
        ctx->S->units[unit_id].alive = 0;

        point_t pos = ctx->S->units[unit_id].position;
        unit_type_t type = (unit_type_t)ctx->S->units[unit_id].type;
        unit_stats_t stats = unit_stats_for_type(type);
        st_points_t size = stats.si;
        
        // Remove unit from all grid cells it occupies
        remove_unit_from_grid(ctx, unit_id, pos, size);
    }

    // sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
}

