#pragma once
#include <stdint.h>

#include "ipc/shared.h"
#include "ipc/ipc_context.h"

/*
calculates damage multiplier based on attacker type and target type
    args:
        -unit (unit_type_t) -> attacker type
        -target (unit_type_t) -> target type
    return (float):
        multiplier applied to base weapon damage
*/
float damage_multiplyer(unit_type_t unit, unit_type_t target);

/*
calculates accuracy multiplier based on weapon type and target type
    args:
        -weapon (weapon_type_t) -> weapon type
        -target (unit_type_t) -> target type
    return (float):
        chance-to-hit multiplier (0..1)
*/
float accuracy_multiplier(weapon_type_t weapon, unit_type_t target);

/*
calculates final damage dealt to target, including hit roll and type multipliers
    args:
        -attacker (unit_entity_t*) -> attacking unit entity
        -target (unit_entity_t*) -> target unit entity
        -weapon (weapon_stats_t*) -> used weapon stats
        -accuracy (float) -> hit chance (0..1)
    return (st_points_t):
        dealt damage (0 if miss)
*/
st_points_t damage_to_target(unit_entity_t *attacker, unit_entity_t *target, weapon_stats_t *weapon, float accuracy);

/* -----------------------------
 * Random radar helpers
 * ----------------------------- */


 /*
 # GRID
checks if (x,y) is within grid bounds
    args:
        -x,y (int) -> checked point
        -w,h (int) -> grid width and height
    return (int):
        1 if in bounds, 0 otherwise
 */
int in_bounds(int x, int y, int w, int h);

/*
# GRID
picks random point inside a circle (disk) on the grid
    args:
        -cx, cy (int16_t) -> center of disk
        -r (int16_t) -> radius
        -grid_w, grid_h (int) -> grid bounds
        -out (point_t*) -> output chosen point
    return (int):
        1 if point chosen, 0 if none exists / invalid params
*/
// Returns 1 on success, 0 if no point exists (e.g., radius < 0 or no in-bounds points).
int radar_pick_random_point_in_circle(
    int16_t cx, int16_t cy, int16_t r,
    int grid_w, int grid_h,
    point_t *out
);

/*
# GRID
picks random point on discrete circle border (ring) on the grid
    args:
        -pos (point_t) -> center position
        -r (int16_t) -> radius
        -grid_w, grid_h (int) -> grid bounds
        -out (point_t*) -> output chosen point
    return (int):
        1 if point chosen, 0 if none exists / invalid params
*/
// Returns 1 on success, 0 if no border point exists.
int radar_pick_random_point_on_circle_border(
    point_t pos, int16_t r,
    int grid_w, int grid_h,
    int8_t unit_size,
    unit_id_t moving_unit_id,
    ipc_ctx_t *ctx,
    point_t *out
);

/*
# GRID
checks if (x,y) is inside disk centered at (cx,cy) with radius r (integer math)
    args:
        -x,y (int) -> checked point
        -cx,cy (int) -> center
        -r (int) -> radius
    return (int):
        1 if inside, 0 otherwise
*/
int in_disk_i(int x, int y, int cx, int cy, int r);

/*
# GRID
squared euclidean distance between two grid points
    args:
        -a,b (point_t) -> points on grid
    return (int):
        (dx^2 + dy^2)
*/
int dist2(point_t a, point_t b);

/* -----------------------------
 * Movement + local pathfinding
 * ----------------------------- */

/*
# GRID
chooses patrol target near position (inside dr radius)
    args:
        -pos (point_t) -> current position
        -dr (int16_t) -> patrol radius
        -grid_w, grid_h (int) -> grid bounds
        -out_target (point_t*) -> output patrol target
    return (int):
        1 if selected, 0 otherwise
*/
int unit_pick_patrol_target_local(
    point_t pos,
    int16_t dr,
    int grid_w, int grid_h,
    point_t *out_target
);

/*
# GRID
computes goal point for this tick:
    if target is within sp radius -> goal = target
    else -> goal is best point on sp border closest to target direction
    args:
        -from (point_t) -> start position
        -target (point_t) -> final target
        -sp (int16_t) -> move radius for this tick
        -grid_w, grid_h (int) -> grid bounds
        -out_goal (point_t*) -> output chosen goal
    return (int):
        1 if goal computed, 0 otherwise
*/
int unit_compute_goal_for_tick(
    point_t from,
    point_t target,
    int16_t sp,
    int grid_w, int grid_h,
    point_t *out_goal
);

/*
# GRID
plans next grid cell toward target using occupancy grid and local pathfinding
    args:
        -from (point_t) -> current position
        -target (point_t) -> target position
        -sp (int16_t) -> move radius for this tick
        -approach (int) -> distance threshold treated as already reached
        -grid_w, grid_h (int) -> grid bounds
        -grid (unit_id_t[grid_w][grid_h]) -> occupancy grid (0 empty, !=0 blocked)
        -out_next (point_t*) -> output next cell to step into
    return (int):
        1 if out_next is written, 0 if invalid params
*/
int unit_next_step_towards(
    point_t from,
    point_t target,
    int16_t sp,
    int approach,
    int grid_w, int grid_h,
    const unit_id_t grid[grid_w][grid_h],
    unit_id_t moving_unit_id,
    st_points_t unit_size,
    ipc_ctx_t *ctx,
    point_t *out_next
);

/*
# GRID
computes planning goal within dr radius:
    if target within dr -> goal = target
    else -> goal is best point on dr border closest to target direction
    args:
        -from (point_t) -> start position
        -target (point_t) -> final target
        -dr (int16_t) -> detection/planning radius
        -grid_w, grid_h (int) -> grid bounds
        -out_goal (point_t*) -> output chosen goal
    return (int):
        1 if goal computed, 0 otherwise
*/
int unit_compute_goal_for_tick_dr(
    point_t from,
    point_t target,
    int16_t dr,
    int grid_w, int grid_h,
    point_t *out_goal
);

/*
# GRID
like unit_next_step_towards but with two radiuses:
    - dr for planning goal
    - sp for actual per-tick movement
    args:
        -from (point_t) -> current position
        -target (point_t) -> target position
        -sp (int16_t) -> per tick speed radius
        -dr (int16_t) -> detection/planning radius
        -approach (int) -> distance threshold treated as already reached
        -grid_w, grid_h (int) -> grid bounds
        -grid (unit_id_t[grid_w][grid_h]) -> occupancy grid
        -out_next (point_t*) -> output next cell
    return (int):
        1 if out_next is written, 0 if invalid params
*/
int unit_next_step_towards_dr(
    point_t from,
    point_t target,
    int16_t sp,
    int16_t dr,
    int approach,
    int grid_w, int grid_h,
    const unit_id_t grid[grid_w][grid_h],
    unit_id_t moving_unit_id,
    st_points_t unit_size,
    ipc_ctx_t *ctx,
    point_t *out_next
);

/*
calculates approach distance for chase/attack based on weapon loadout and target type
    args:
        -ba (weapon_loadout_view_t) -> loadout
        -t_type (unit_type_t) -> target type
    return (int16_t):
        minimal weapon range that can hit target minus 1
*/
int16_t unit_calculate_aproach(weapon_loadout_view_t ba, unit_type_t t_type);


/* -----------------------------
 * Targeting
 * -----------------------------
 */

/*
# GRID
radar scan: finds units within detection radius (dr) around unit_id
    args:
        -unit_id (unit_id_t) -> scanning unit id
        -u_st (unit_stats_t) -> unit stats (dr used)
        -units (unit_entity_t*) -> units array
        -out (unit_id_t*) -> output list of detected ids
        -faction (faction_t) -> ignored faction (FACTION_NONE means detect all)
    return (int):
        number of detected units written to out
*/
int unit_radar(
    unit_id_t unit_id,
    unit_stats_t u_st,
    unit_entity_t *units,
    unit_id_t *out,
    faction_t faction
);
