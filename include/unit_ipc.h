#pragma once
#include <stdint.h>

#include "ipc/shared.h"
#include "ipc/ipc_context.h"
#include "ipc/semaphores.h"

unit_id_t check_if_occupied(ipc_ctx_t *ctx, point_t point);

// changes unit position and moves it on the grid to specific location
void unit_move_to(ipc_ctx_t *ctx, unit_id_t unit_id, point_t new_pos);

// depending on weapon type sends mesage to hit target unit process
void unit_add_to_dmg_payload(ipc_ctx_t *ctx,uint16_t target_id, int64_t dmg);

int unit_cell_is_free(ipc_ctx_t *ctx, unit_id_t self, point_t p);
void unit_snapshot_occupancy(ipc_ctx_t *ctx, unit_id_t self,
                             int16_t *occ, int w, int h, int stride);
int unit_try_move_to(ipc_ctx_t *ctx, unit_id_t self,
                     point_t from, point_t to);

