#include "unit_size.h"
#include "ipc/ipc_context.h"
#include <stdlib.h>

/* Hardcoded patterns for each size */

/* Size 1: single cell at origin */
static const size_pattern_t pattern_size_1 = {
    .count = 1,
    .cells = {{0, 0}}
};

/* Size 2: 3x3 grid (radius 1) */
static const size_pattern_t pattern_size_2 = {
    .count = 5,
    .cells = {
                    {0, -1},
        {-1,  0},   {0,  0},    {1,  0},
                    {0,  1}
    }
};

/* Size 3: 5x5 grid (radius 2) */
static const size_pattern_t pattern_size_3 = {
    .count = 13,
    .cells = {
                                {0, -2},
                    {-1, -1},   {0, -1},    {1, -1},
        {-2,  0},   {-1,  0},   {0,  0},    {1,  0},    {2,  0},
                    {-1,  1},   {0,  1},    {1,  1},
                                {0,  2}
    }
};

const size_pattern_t* get_size_pattern(st_points_t size) {
    switch (size) {
        case 1: return &pattern_size_1;
        case 2: return &pattern_size_2;
        case 3: return &pattern_size_3;
        default: return &pattern_size_1; // fallback to size 1
    }
}

int can_fit_at_position(ipc_ctx_t *ctx, point_t center, st_points_t size, unit_id_t ignore_unit) {
    const size_pattern_t *pattern = get_size_pattern(size);
    
    for (int i = 0; i < pattern->count; i++) {
        int16_t x = center.x + pattern->cells[i].x;
        int16_t y = center.y + pattern->cells[i].y;
        
        // Check bounds
        if (x < 0 || x >= M || y < 0 || y >= N) {
            return 0; // out of bounds
        }
        
        // Check if occupied by another unit
        unit_id_t occupant = ctx->S->grid[x][y];
        if (occupant != 0 && occupant != ignore_unit) {
            return 0; // cell occupied
        }
    }
    
    return 1; // all cells available
}

void get_occupied_cells(point_t center, st_points_t size, point_t *out_cells, int *out_count) {
    const size_pattern_t *pattern = get_size_pattern(size);
    *out_count = pattern->count;
    
    for (int i = 0; i < pattern->count; i++) {
        out_cells[i].x = center.x + pattern->cells[i].x;
        out_cells[i].y = center.y + pattern->cells[i].y;
    }
}

point_t get_closest_cell_to_attacker(point_t attacker_pos, point_t target_center, st_points_t target_size) {
    const size_pattern_t *pattern = get_size_pattern(target_size);
    
    point_t closest = target_center;
    int32_t min_dist2 = INT32_MAX;
    
    for (int i = 0; i < pattern->count; i++) {
        point_t cell = {
            .x = target_center.x + pattern->cells[i].x,
            .y = target_center.y + pattern->cells[i].y
        };
        
        // Check if cell is in bounds
        if (cell.x < 0 || cell.x >= M || cell.y < 0 || cell.y >= N) {
            continue;
        }
        
        // Calculate distance squared
        int32_t dx = cell.x - attacker_pos.x;
        int32_t dy = cell.y - attacker_pos.y;
        int32_t dist2 = dx * dx + dy * dy;
        
        if (dist2 < min_dist2) {
            min_dist2 = dist2;
            closest = cell;
        }
    }
    
    return closest;
}

void place_unit_on_grid(ipc_ctx_t *ctx, unit_id_t unit_id, point_t center, st_points_t size) {
    const size_pattern_t *pattern = get_size_pattern(size);
    
    for (int i = 0; i < pattern->count; i++) {
        int16_t x = center.x + pattern->cells[i].x;
        int16_t y = center.y + pattern->cells[i].y;
        
        // Only place if in bounds
        if (x >= 0 && x < M && y >= 0 && y < N) {
            ctx->S->grid[x][y] = unit_id;
        }
    }
}

void remove_unit_from_grid(ipc_ctx_t *ctx, unit_id_t unit_id, point_t center, st_points_t size) {
    const size_pattern_t *pattern = get_size_pattern(size);
    
    for (int i = 0; i < pattern->count; i++) {
        int16_t x = center.x + pattern->cells[i].x;
        int16_t y = center.y + pattern->cells[i].y;
        
        // Only clear if in bounds and occupied by this unit
        if (x >= 0 && x < M && y >= 0 && y < N) {
            if (ctx->S->grid[x][y] == unit_id) {
                ctx->S->grid[x][y] = 0;
            }
        }
    }
}
