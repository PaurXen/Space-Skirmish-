#include "unit_ipc.h"



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
    
    // Clear old position on grid
    if (old_pos.x >= 0 && old_pos.x < M && old_pos.y >= 0 && old_pos.y < N) {
        if (ctx->S->grid[old_pos.x][old_pos.y] == (unit_id_t)unit_id) {
            ctx->S->grid[old_pos.x][old_pos.y] = 0;
        }
    }

    // Set new position on grid
    if (new_pos.x >= 0 && new_pos.x < M && new_pos.y >= 0 && new_pos.y < N) {
        ctx->S->grid[new_pos.x][new_pos.y] = unit_id;
    }

    // Update unit's position
    ctx->S->units[unit_id].position = new_pos;
    // sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
}


void unit_add_to_dmg_payload(
    ipc_ctx_t *ctx,
    uint16_t target_id,
    int64_t dmg
) {
    // sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
    ctx->S->units[target_id].dmg_payload += dmg;

    // sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
}