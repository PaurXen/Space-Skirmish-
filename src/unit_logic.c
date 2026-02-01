#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "unit_logic.h"
#include "weapon_stats.h"
#include "unit_ipc.h"
#include "ipc/shared.h"

typedef struct { int16_t dx, dy; } offset_t;

float damage_multiplyer(unit_type_t unit, unit_type_t target) {
    switch (unit) {
        case TYPE_FLAGSHIP:
            if (target == TYPE_CARRIER) return 1.5f;
            else return 1.0f;
        case TYPE_DESTROYER:
            if (target == TYPE_FLAGSHIP || target == TYPE_DESTROYER || target == TYPE_CARRIER) return 1.5f;
            else return 1.0f;
        case TYPE_CARRIER:
            if (target == TYPE_FIGHTER || target == TYPE_BOMBER || target == TYPE_ELITE) return 1.5f;
            else return 1.0f;
        case TYPE_FIGHTER:
            if (target == TYPE_FIGHTER || target == TYPE_BOMBER) return 1.5f;
            else return 1.0f;
        case TYPE_BOMBER:
            if (target == TYPE_FLAGSHIP || target == TYPE_DESTROYER || target == TYPE_CARRIER) return 3.0f;
            else return 1.0f;
        case TYPE_ELITE:
            if (target == TYPE_FIGHTER || target == TYPE_BOMBER || target == TYPE_ELITE) return 2.0f;
            else return 1.0f;
        default:
            return 1.0f;
    }
}

float accuracy_multiplier(weapon_type_t weapon, unit_type_t target) {
    if (weapon == NONE) return 0.0f;
    if (LR_CANNON <= weapon && weapon <= SR_CANNON) {
        if (target == TYPE_FLAGSHIP || target == TYPE_DESTROYER || target == TYPE_CARRIER)
            return 0.75f;
        if (target == TYPE_FIGHTER || target == TYPE_BOMBER || target == TYPE_ELITE)
            return 0.25f;
    } else if (LR_GUN <= weapon && weapon <= SR_GUN) {
        if (target == TYPE_FLAGSHIP || target == TYPE_DESTROYER || target == TYPE_CARRIER)
            return 0.0f;
        if (target == TYPE_FIGHTER || target == TYPE_BOMBER || target == TYPE_ELITE)
            return 0.75f;
    }
    return 0.0f;
}

st_points_t damage_to_target(unit_entity_t *attacker, unit_entity_t *target, weapon_stats_t *weapon, float accuracy) {
    float roll = (float)rand() / (float)RAND_MAX;

    if (roll > accuracy) {
        // MISS (0 dmg)
        return 0;
    }

    return (st_points_t)(damage_multiplyer(attacker->type, target->type) * (float)weapon->dmg);
    
}




int in_bounds(int x, int y, int w, int h) {
    return (x >= 0 && x < w && y >= 0 && y < h);
}

static inline int sqr_i(int v) { return v * v; }

int dist2(point_t a, point_t b) {
    int dx = (int)a.x - (int)b.x;
    int dy = (int)a.y - (int)b.y;
    return dx*dx + dy*dy;
}

int in_disk_i(int x, int y, int cx, int cy, int r) {
    int dx = x - cx;
    int dy = y - cy;
    return (dx*dx + dy*dy) <= r*r;
}


/* Build a list of unique offsets on the *discrete* circle border of radius r.
 * Border definition:
 *  - inside circle (dx^2+dy^2 <= r^2)
 *  - AND at least one of its 4-neighbors is outside the circle
 * This produces a "ring" that works well for grid movement.
 */
static int build_circle_border_offsets(int r, offset_t *out, int max_out) {
    if (r < 0) return 0;
    if (r == 0) {
        if (max_out <= 0) return 0;
        out[0] = (offset_t){0, 0};
        return 1;
    }

    const int r2 = r * r;
    int count = 0;

    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            const int d2 = dx*dx + dy*dy;
            if (d2 > r2) continue;

            // 4-neighbor border test
            const int n_out =
                ((dx+1)*(dx+1) + dy*dy > r2) ||
                ((dx-1)*(dx-1) + dy*dy > r2) ||
                (dx*dx + (dy+1)*(dy+1) > r2) ||
                (dx*dx + (dy-1)*(dy-1) > r2);

            if (!n_out) continue;

            if (count < max_out) {
                out[count++] = (offset_t){ (int16_t)dx, (int16_t)dy };
            } else {
                // too many points for buffer (should not happen for small r)
                return count;
            }
        }
    }

    return count;
}

int radar_pick_random_point_in_circle(
    int16_t cx, int16_t cy, int16_t r,
    int grid_w, int grid_h,
    point_t *out
) {
    if (!out) return 0;
    if (r < 0 || grid_w <= 0 || grid_h <= 0) return 0;

    const int r2 = r * r;

    // worst-case number of points in bounding box
    const int max_pts = (2*r + 1) * (2*r + 1);
    if (max_pts <= 0) return 0;

    point_t *cands = (point_t*)malloc((size_t)max_pts * sizeof(point_t));
    if (!cands) return 0;

    int n = 0;
    for (int y = cy - r; y <= cy + r; y++) {
        for (int x = cx - r; x <= cx + r; x++) {
            if (!in_bounds(x, y, grid_w, grid_h)) continue;
            const int dx = x - cx;
            const int dy = y - cy;
            if (dx*dx + dy*dy > r2) continue;
            cands[n++] = (point_t){ (int16_t)x, (int16_t)y };
        }
    }

    if (n <= 0) {
        free(cands);
        return 0;
    }

    int k = rand() % n;
    *out = cands[k];
    free(cands);
    return 1;
}

int radar_pick_random_point_on_circle_border(
    point_t pos, int16_t r,
    int grid_w, int grid_h,
    point_t *out
) {
    if (!out) return 0;
    if (r < 0 || grid_w <= 0 || grid_h <= 0) return 0;

    // safe upper bound for discrete border points in a box
    const int max_off = (2*r + 1) * (2*r + 1);
    if (max_off <= 0) return 0;

    offset_t *offs = (offset_t*)malloc((size_t)max_off * sizeof(offset_t));
    if (!offs) return 0;

    const int n_off = build_circle_border_offsets(r, offs, max_off);
    if (n_off <= 0) {
        free(offs);
        return 0;
    }

    // Collect in-bounds border points (absolute positions)
    point_t *cands = (point_t*)malloc((size_t)n_off * sizeof(point_t));
    if (!cands) {
        free(offs);
        return 0;
    }

    int n = 0;
    for (int i = 0; i < n_off; i++) {
        int x = pos.x + offs[i].dx;
        int y = pos.y + offs[i].dy;
        if (!in_bounds(x, y, grid_w, grid_h)) continue;
        cands[n++] = (point_t){ (int16_t)x, (int16_t)y };
    }

    free(offs);

    if (n <= 0) {
        free(cands);
        return 0;
    }

    int k = rand() % n;
    *out = cands[k];
    free(cands);
    return 1;
}

int unit_pick_patrol_target_local(
    point_t pos,
    int16_t dr,
    int grid_w, int grid_h,
    point_t *out_target
) {
    if (!out_target) return 0;
    if (dr < 0) return 0;
    // 75% of speed radius (integer)
    // const int r = (dr * 3) / 4;
    return radar_pick_random_point_in_circle(pos.x, pos.y, dr, grid_w, grid_h, out_target);
}

int unit_compute_goal_for_tick(
    point_t from,
    point_t target,
    int16_t sp,
    int grid_w, int grid_h,
    point_t *out_goal
) {
    if (!out_goal) return 0;
    if (sp < 0 || grid_w <= 0 || grid_h <= 0) return 0;

    // If target itself is reachable and in bounds, use it.
    if (in_bounds(target.x, target.y, grid_w, grid_h)) {
        const int sp2 = sp * sp;
        if (dist2(from, target) <= sp2) {
            *out_goal = target;
            return 1;
        }
    }

    // Otherwise, pick the border point at distance ~sp that is closest to the target.
    const int max_off = (2*sp + 1) * (2*sp + 1);
    if (max_off <= 0) return 0;

    offset_t *offs = (offset_t*)malloc((size_t)max_off * sizeof(offset_t));
    if (!offs) return 0;

    const int n_off = build_circle_border_offsets(sp, offs, max_off);
    if (n_off <= 0) {
        free(offs);
        return 0;
    }

    int best_d2 = INT_MAX;
    point_t best = from;
    int found = 0;

    for (int i = 0; i < n_off; i++) {
        int gx = (int)from.x + (int)offs[i].dx;
        int gy = (int)from.y + (int)offs[i].dy;

        if (!in_bounds(gx, gy, grid_w, grid_h)) continue;

        point_t g = { (int16_t)gx, (int16_t)gy };
        int d2 = dist2(g, target);
        if (d2 < best_d2) {
            best_d2 = d2;
            best = g;
            found = 1;
        }
    }

    free(offs);

    if (!found) return 0;
    *out_goal = best;
    return 1;
}

static inline int idx_of_local(int x, int y, int minx, int miny, int lw) {
    return (y - miny) * lw + (x - minx);
}

static point_t bfs_next_step_local(
    point_t from,
    point_t goal,
    int16_t sp,
    int grid_w, int grid_h,
    const unit_id_t grid[grid_w][grid_h]
) {
    // No movement possible
    if (sp <= 0) return from;

    const int sp2 = sp * sp;

    // Bounding box (clamped to map)
    const int minx = (from.x - sp < 0) ? 0 : (from.x - sp);
    const int maxx = (from.x + sp >= grid_w) ? (grid_w - 1) : (from.x + sp);
    const int miny = (from.y - sp < 0) ? 0 : (from.y - sp);
    const int maxy = (from.y + sp >= grid_h) ? (grid_h - 1) : (from.y + sp);

    const int lw = maxx - minx + 1;
    const int lh = maxy - miny + 1;
    const int area = lw * lh;

    if (area <= 0) return from;

    // visited + parent + distance for tie-breaking
    uint8_t *vis = (uint8_t*)calloc((size_t)area, 1);
    int32_t *prev = (int32_t*)malloc((size_t)area * sizeof(int32_t));
    uint16_t *dist = (uint16_t*)malloc((size_t)area * sizeof(uint16_t));
    point_t *q = (point_t*)malloc((size_t)area * sizeof(point_t));

    if (!vis || !prev || !dist || !q) {
        free(vis); free(prev); free(dist); free(q);
        return from;
    }

    for (int i = 0; i < area; i++) prev[i] = -1;

    const int start_idx = idx_of_local(from.x, from.y, minx, miny, lw);
    vis[start_idx] = 1;
    dist[start_idx] = 0;
    q[0] = from;
    int qh = 0, qt = 1;

    int reached = 0;
    int goal_idx = -1;

    int best_idx = start_idx;
    int best_h = dist2(from, goal);

    static const int8_t dirs8[8][2] = {
        { 1, 0}, {-1, 0}, {0, 1}, {0,-1},
        { 1, 1}, { 1,-1}, {-1, 1}, {-1,-1}
    };

    while (qh < qt) {
        point_t cur = q[qh++];
        const int cur_idx = idx_of_local(cur.x, cur.y, minx, miny, lw);

        if (cur.x == goal.x && cur.y == goal.y) {
            reached = 1;
            goal_idx = cur_idx;
            break;
        }

        for (int di = 0; di < 8; di++) {
            int nx = (int)cur.x + dirs8[di][0];
            int ny = (int)cur.y + dirs8[di][1];

            if (nx < minx || nx > maxx || ny < miny || ny > maxy) continue;

            // stay within speed circle
            const int dx = nx - (int)from.x;
            const int dy = ny - (int)from.y;
            if (dx*dx + dy*dy > sp2) continue;

            const int nidx = idx_of_local(nx, ny, minx, miny, lw);
            if (vis[nidx]) continue;

            // must be empty (except the starting cell)
            if (!(nx == from.x && ny == from.y)) {
                if (grid[nx][ny] != 0) continue;
            }

            vis[nidx] = 1;
            prev[nidx] = cur_idx;
            dist[nidx] = (uint16_t)(dist[cur_idx] + 1);
            q[qt++] = (point_t){ (int16_t)nx, (int16_t)ny };

            // update best fallback (closest to goal; tie by fewer steps)
            {
                const int h = dist2((point_t){(int16_t)nx,(int16_t)ny}, goal);
                if (h < best_h || (h == best_h && dist[nidx] < dist[best_idx])) {
                    best_h = h;
                    best_idx = nidx;
                }
            }
        }
    }

    {
        int target_idx = reached ? goal_idx : best_idx;

        // Reconstruct: walk back until parent is start; that node is our next step.
        if (target_idx == start_idx) {
            free(vis); free(prev); free(dist); free(q);
            return from;
        }

        int walk = target_idx;
        int parent = prev[walk];
        while (parent != -1 && parent != start_idx) {
            walk = parent;
            parent = prev[walk];
        }

        // Convert local idx back to global coords
        const int wx = (walk % lw) + minx;
        const int wy = (walk / lw) + miny;

        free(vis); free(prev); free(dist); free(q);
        return (point_t){ (int16_t)wx, (int16_t)wy };
    }
}




// "Border" of disk in 4-neighborhood sense: inside disk, but has a 4-neighbor outside disk
static inline int on_circle_border_4n_i(int x, int y, int cx, int cy, int r) {
    if (!in_disk_i(x, y, cx, cy, r)) return 0;

    // if any 4-neighbor is outside disk, treat this cell as border
    if (!in_disk_i(x + 1, y,     cx, cy, r)) return 1;
    if (!in_disk_i(x - 1, y,     cx, cy, r)) return 1;
    if (!in_disk_i(x,     y + 1, cx, cy, r)) return 1;
    if (!in_disk_i(x,     y - 1, cx, cy, r)) return 1;

    return 0;
}


// Pick the best reachable position within SP disk, preferably on the SP border,
// that is closest to `goal`. Returns that position (can be `from` if stuck).
static point_t bfs_best_reachable_in_sp_disk_prefer_border(
    point_t from,
    point_t goal,
    int16_t sp,
    int w, int h,
    const unit_id_t grid[w][h]
) {
    point_t out = from;
    if (sp <= 0) return out;

    const int max_cells = w * h;
    if (max_cells <= 0) return out;

    // visited bitmap + queue
    uint8_t *vis = (uint8_t*)calloc((size_t)max_cells, 1);
    if (!vis) return out;

    // We only need to store visited cells; easiest is parent index per cell (flattened),
    // but here we only need the final cell, not a path. Still store parent if you
    // later want the actual step chain.
    int *q = (int*)malloc((size_t)max_cells * sizeof(int));
    if (!q) { free(vis); return out; }

    const int sx = from.x, sy = from.y;
    const int sidx = sy * w + sx;

    // If start is out of bounds, give up
    if (!in_bounds(sx, sy, w, h)) { free(q); free(vis); return out; }

    // BFS within SP disk; blocked cells are grid!=0 (adjust if your project uses other meaning)
    int head = 0, tail = 0;
    q[tail++] = sidx;
    vis[sidx] = 1;

    int best_border_idx = -1;
    int best_border_d2 = INT_MAX;

    int best_any_idx = -1;
    int best_any_d2 = INT_MAX;

    while (head < tail) {
        int idx = q[head++];
        int x = idx % w;
        int y = idx / w;

        // Only consider cells inside SP disk
        if (!in_disk_i(x, y, sx, sy, sp)) continue;

        // Evaluate candidate (skip blocked; start can be allowed)
        if (!(x == sx && y == sy)) {
            if (grid[x][y] != 0) {
                // blocked cell can't be occupied
                goto expand_neighbors;
            }
        }

        // Candidate scoring: closer to goal is better
        point_t p = { (int16_t)x, (int16_t)y };
        int d2 = dist2(p, goal);

        // Track best "any" reachable inside disk
        if (d2 < best_any_d2) {
            best_any_d2 = d2;
            best_any_idx = idx;
        }

        // Track best reachable on border
        if (on_circle_border_4n_i(x, y, sx, sy, sp)) {
            if (d2 < best_border_d2) {
                best_border_d2 = d2;
                best_border_idx = idx;
            }
        }

expand_neighbors:
        // 4-neighborhood expansion
        const int nx[4] = { x+1, x-1, x, x };
        const int ny[4] = { y, y, y+1, y-1 };

        for (int k = 0; k < 4; k++) {
            int xx = nx[k], yy = ny[k];
            if (!in_bounds(xx, yy, w, h)) continue;

            // Keep BFS limited to disk early
            if (!in_disk_i(xx, yy, sx, sy, sp)) continue;

            // Don’t enqueue blocked cells (except allow start already handled)
            if (grid[xx][yy] != 0) continue;

            int nidx = yy * w + xx;
            if (vis[nidx]) continue;
            vis[nidx] = 1;
            q[tail++] = nidx;
        }
    }

    int chosen = (best_border_idx != -1) ? best_border_idx : best_any_idx;
    if (chosen != -1) {
        out.x = (int16_t)(chosen % w);
        out.y = (int16_t)(chosen / w);
    }

    free(q);
    free(vis);
    return out;
}


int unit_compute_goal_for_tick_dr(
    point_t from,
    point_t target,
    int16_t dr,
    int grid_w, int grid_h,
    point_t *out_goal
) {
    if (!out_goal) return 0;
    if (dr < 0 || grid_w <= 0 || grid_h <= 0) return 0;

    // If target itself is within detection/planning radius, use it as the goal.
    if (in_bounds(target.x, target.y, grid_w, grid_h)) {
        const int dr2 = dr * dr;
        if (dist2(from, target) <= dr2) {
            *out_goal = target;
            return 1;
        }
    }

    // Otherwise, pick the border point at distance ~dr that is closest to the target.
    const int max_off = (2*dr + 1) * (2*dr + 1);
    if (max_off <= 0) return 0;

    offset_t *offs = (offset_t*)malloc((size_t)max_off * sizeof(offset_t));
    if (!offs) return 0;

    const int n_off = build_circle_border_offsets(dr, offs, max_off);
    if (n_off <= 0) {
        free(offs);
        return 0;
    }

    int best_d2 = INT_MAX;
    point_t best = from;
    int found = 0;

    for (int i = 0; i < n_off; i++) {
        int gx = (int)from.x + (int)offs[i].dx;
        int gy = (int)from.y + (int)offs[i].dy;
        if (!in_bounds(gx, gy, grid_w, grid_h)) continue;

        point_t g = { (int16_t)gx, (int16_t)gy };
        int d2 = dist2(g, target);
        if (d2 < best_d2) {
            best_d2 = d2;
            best = g;
            found = 1;
        }
    }

    free(offs);

    if (!found) return 0;
    *out_goal = best;
    return 1;
}


int unit_next_step_towards(
    point_t from,
    point_t target,
    int16_t sp,
    int approach,
    int grid_w, int grid_h,
    const unit_id_t grid[grid_w][grid_h],
    point_t *out_next
) {
    // Backwards-compatible behavior: if caller does not pass DR,
    // treat DR == SP (old logic).
    return unit_next_step_towards_dr(
        from, target,
        sp, sp,
        approach,
        grid_w, grid_h, grid,
        out_next
    );
}

int unit_next_step_towards_dr(
    point_t from,
    point_t target,
    int16_t sp,
    int16_t dr,
    int approach,
    int grid_w, int grid_h,
    const unit_id_t grid[grid_w][grid_h],
    point_t *out_next
) {
    if (!out_next) return 0;
    *out_next = from;

    if (!grid || grid_w <= 0 || grid_h <= 0) return 0;
    if (sp < 0 || dr < 0) return 0;
    if (approach < 0) approach = 0;

    // Already within approach radius (no move needed)
    const int approach2 = approach * approach;
    if (dist2(from, target) <= approach2) {
        *out_next = from;
        return 1;
    }

    // 1) Choose a farther planning "Goal" within DR.
    point_t goal;
    if (!unit_compute_goal_for_tick_dr(from, target, dr, grid_w, grid_h, &goal)) {
        *out_next = from;
        return 1;
    }

    // 2a) If goal is already reachable within SP, go directly to goal
    if (in_disk_i(goal.x, goal.y, from.x, from.y, sp)) {
        // goal must be in bounds and not blocked
        if (in_bounds(goal.x, goal.y, grid_w, grid_h) &&
            grid[goal.x][goal.y] == 0) {
            *out_next = goal;
            return 1;
        }
    }

    // 2b) Otherwise, choose best reachable SP position (prefer border)
    point_t step = bfs_best_reachable_in_sp_disk_prefer_border(
        from, goal, sp,
        grid_w, grid_h, grid
    );
    *out_next = step;
    return 1;
}




int unit_radar(
    unit_id_t unit_id,
    unit_stats_t u_st,
    unit_entity_t *units,
    unit_id_t *out,
    faction_t faction
){
    int count = 0;
    point_t from = units[unit_id].position;

    for (unit_id_t id = 1; id <= MAX_UNITS; id++){
        if (id == unit_id) continue;
        if (faction != FACTION_NONE && units[id].faction == faction) continue;
        if (!units[id].pid) continue;
        if (!units[id].alive) continue;

        point_t pos = units[id].position;

        if (in_disk_i(pos.x, pos.y, from.x, from.y, u_st.dr)){
            out[count++] = id;
        }
    }
    return count;
}

int16_t unit_calculate_aproach(weapon_loadout_view_t ba, unit_type_t t_type){
    int16_t min_range = INT16_MAX;
    float ac = 0;
    for (int8_t i = 0; i < ba.count; i++){
        ac = accuracy_multiplier(ba.arr[i].type, t_type);
        if (ac > 0 && ba.arr[i].range < min_range) 
            min_range =  ba.arr[i].range;
    }   
    return min_range-1;
}

