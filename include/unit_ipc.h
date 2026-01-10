#pragma once
#include <stdint.h>

#include "ipc/shared.h"
#include "ipc/ipc_context.h"
#include "ipc/semaphores.h"


// changes unit position and moves it on the grid to specific location
void unit_move_to(ipc_ctx_t *ctx, uint16_t unit_id, point_t new_pos);

// depending on weapon type sends mesage to hit target unit process
void unit_add_to_dmg_payload(ipc_ctx_t *ctx,uint16_t target_id, int64_t dmg);
