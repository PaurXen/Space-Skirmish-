#pragma once
#include <stdio.h>
#include <stdint.h>
#include <sys/msg.h>

#include "ipc/shared.h"
#include "ipc/ipc_context.h"
#include "ipc/semaphores.h"
#include "log.h"
#include "unit_logic.h"


/*
# GRID
checks if position is occupyed:
    args:
        -ctx (ipc_ctx_t*) -> context of sheared memory and synchronization
        -point (point_t) -> cheking point
    return (unit_id_t):
        returns (0) if not ocupyed or (unit_id_t) if it is
*/
unit_id_t check_if_occupied(ipc_ctx_t *ctx, point_t point);

/*
# GRID
changes unit position and moves it on the grid to specific location
    args:
        -ctx (ipc_ctx_t*) -> --//--
        -unit_id (unit_id_t) -> id of the unit that position to change
        -new_pos (point_t) -> position to which to go
    return (void):
        None
*/
void unit_change_position(ipc_ctx_t *ctx, unit_id_t unit_id, point_t new_pos);

/*
Get the target position for aiming/movement, accounting for unit size.
Returns the closest cell of the target unit to the attacker.
    args:
        -ctx (ipc_ctx_t*) -> context
        -attacker_id (unit_id_t) -> id of attacking/moving unit
        -target_id (unit_id_t) -> id of target unit
    return (point_t):
        closest cell position of target to attacker
*/
point_t get_target_position(ipc_ctx_t *ctx, unit_id_t attacker_id, unit_id_t target_id);

/*
adds damage to damage payload of trgeted unit
    args:
        -ctx (ipc_ctx_t*) -> --//--
        -target_id (unit_id_t) -> id of unit to which demage is added
        -dmg (st_points_t) -> demage recived by target
    return (void):
        None
*/
void unit_add_to_dmg_payload(ipc_ctx_t *ctx, unit_id_t target_id, st_points_t dmg);

/*
caculating demage recived and updating unit stats
    args:
        -ctx (ipc_ctx_t*) -> --//--
        -unit_id (unit_id_t) -> unit_id for demage computation 
        -st (unit_stats_t*) -> statistics of unit with unit_id
    return (void):
        None
*/
void compute_dmg_payload(ipc_ctx_t *ctx, unit_id_t unit_id, unit_stats_t *st);

/*
setting weapons targets and
calculating amount of demage given to target
    args:
        -ctx (ipc_ctx_t*) -> --//--
        -unit_id (unit_id_t) -> id of unit atacking
        -st (unit_stats_t*) -> statistics of attacking units
        -target_sec (unit_id_t) -> id of target that is attcked
        -count (int) -> number of detected units in detect_id array
        -detect_id (unit_id_t*) -> array of detected enemy ids
        -out_dmg (st_points_t*) -> output array for dmg per weapon
    return (st_points_t):
        sum of calculated damage for this shot (currently always 0 in implementation)
*/
st_points_t unit_weapon_shoot(ipc_ctx_t *ctx,
    unit_id_t unit_id,
    unit_stats_t *st,
    unit_id_t target_sec,
    int count,
    unit_id_t *detect_id,
    st_points_t *out_dmg
);

/*
# GRID
choses secondary target based on detected enemies and damage multiplier
    args:
        -ctx (ipc_ctx_t*) -> --//--
        -detected_id (unit_id_t*) -> array of detected enemy ids
        -count (int) -> size of detected_id array
        -unit_id (unit_id_t) -> current unit id (attacker)
        -target_pri (point_t*) -> output position of chosen target (primary target point)
        -have_target_pri (int8_t*) -> set to 1 if target_pri is valid
        -have_target_sec (int8_t*) -> set to 1 if secondary target is valid
    return (unit_id_t):
        returns chosen target id, or 0 if none
*/
unit_id_t unit_chose_secondary_target(ipc_ctx_t *ctx,
    unit_id_t *detected_id,
    int count,
    unit_id_t unit_id,
    point_t *target_pri,
    int8_t *have_target_pri,
    int8_t *have_target_sec
);

int8_t unit_chose_patrol_point(ipc_ctx_t *ctx,
    unit_id_t unit_id,
    point_t *target_pri,
    unit_stats_t st
);

/*
# GRID
moves unit one tick towards target_pri using local goal selection (DR) and speed movement (SP)
    args:
        -ctx (ipc_ctx_t*) -> --//--
        -unit_id (unit_id_t) -> id of moved unit
        -from (point_t) -> current position
        -target_pri (point_t*) -> target position to move towards
        -st (unit_stats_t*) -> unit statistics (sp, dr used here)
        -aproach (int) -> distance threshold treated as "already reached"
    return (void):
        None
*/
void unit_move(ipc_ctx_t *ctx,
    unit_id_t unit_id,
    point_t from,
    point_t *target_pri,
    unit_stats_t *st,
    int aproach
);

/*
# GRID
marks unit as dead and clears its grid cell
    args:
        -ctx (ipc_ctx_t*) -> --//--
        -unit_id (unit_id_t) -> id of unit to mark as dead
    return (void):
        None
*/
void mark_dead(ipc_ctx_t *ctx, unit_id_t unit_id);
