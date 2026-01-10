#include "unit_ipc.h"



void unit_move_to(ipc_ctx_t *ctx, uint16_t unit_id, point_t new_pos) {
    sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
    point_t old_pos = ctx->S->units[unit_id].position;
    
    // Clear old position on grid
    if (old_pos.x >= 0 && old_pos.x < N && old_pos.y >= 0 && old_pos.y < M) {
        if (ctx->S->grid[old_pos.x][old_pos.y] == (unit_id_t)unit_id) {
            ctx->S->grid[old_pos.x][old_pos.y] = 0;
        }
    }

    // Set new position on grid
    if (new_pos.x >= 0 && new_pos.x < N && new_pos.y >= 0 && new_pos.y < M) {
        ctx->S->grid[new_pos.x][new_pos.y] = unit_id;
    }

    // Update unit's position
    ctx->S->units[unit_id].position = new_pos;
    sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
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