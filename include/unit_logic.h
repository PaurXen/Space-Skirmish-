#pragma once
#include <stdint.h>

#include "ipc/shared.h"

float damage_multiplyer(unit_type_t unit, unit_type_t target);
float accuracy_multiplier(weapon_type_t weapon, unit_type_t target);

int64_t demage_to_target(unit_entity_t *attacker, unit_entity_t *target, weapon_stats_t *weapon);

/* -----------------------------
 * Random radar helpers
 * ----------------------------- */

// Returns 1 on success, 0 if no point exists (e.g., radius < 0 or no in-bounds points).
int radar_pick_random_point_in_circle(
    int16_t cx, int16_t cy, int16_t r,
    int grid_w, int grid_h,
    point_t *out
);

// Returns 1 on success, 0 if no border point exists.
int radar_pick_random_point_on_circle_border(
    point_t pos, int16_t r,
    int grid_w, int grid_h,
    point_t *out
);

int in_disk_i(int x, int y, int cx, int cy, int r);

/* -----------------------------
 * Movement + local pathfinding
 * -----------------------------
 *
 * Grid is a 2D array stored row-major:
 *   cell = grid[y*grid_stride + x]
 * where 0 means empty and any non-zero means blocked/occupied.
 *
 * Coordinates are in [0..grid_w-1] x [0..grid_h-1].
 */

// Chooses a patrol target within radius dr of `pos`.
int unit_pick_patrol_target_local(
    point_t pos,
    int16_t dr,
    int grid_w, int grid_h,
    point_t *out_target
);

// Computes a "goal for this tick": either the target itself (if within sp),
// or a point on the speed-circle border closest to the target direction.
// Returns 1 on success, 0 if sp < 0 or no in-bounds circle-border points exist.
int unit_compute_goal_for_tick(
    point_t from,
    point_t target,
    int16_t sp,
    int grid_w, int grid_h,
    point_t *out_goal
);

// Plans a movement step (only the *next* cell) toward `target`.
//
// - `sp` is max travel radius for this tick.
// - `approach` is how close you need to be to the target to consider it "reached".
//   For plain movement: approach=0.
//   For attack/chase: approach=weapon_range (or similar).
//
// Returns:
//   1 if out_next is written (may equal `from` if already in range),
//   0 if parameters invalid.
int unit_next_step_towards(
    point_t from,
    point_t target,
    int16_t sp,
    int approach,
    int grid_w, int grid_h,
    const unit_id_t grid[grid_w][grid_h],
    point_t *out_next
);

// Computes a "goal" point within detection/planning radius `dr`.
// If `target` is within `dr`, the goal becomes the target (even if it's
// outside `sp`). Otherwise, it picks a point on the `dr` circle-border
// closest to the target direction.
int unit_compute_goal_for_tick_dr(
    point_t from,
    point_t target,
    int16_t dr,
    int grid_w, int grid_h,
    point_t *out_goal
);

// Like unit_next_step_towards(), but the high-level goal is selected within
// `dr` (detection/planning range) and the per-tick move is selected within
// `sp` (speed range).
int unit_next_step_towards_dr(
    point_t from,
    point_t target,
    int16_t sp,
    int16_t dr,
    int approach,
    int grid_w, int grid_h,
    const unit_id_t grid[grid_w][grid_h],
    point_t *out_next
);
