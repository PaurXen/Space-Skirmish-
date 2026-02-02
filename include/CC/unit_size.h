#ifndef UNIT_SIZE_H
#define UNIT_SIZE_H

#include "ipc/shared.h"
#include "ipc/ipc_context.h"

/* Maximum cells a unit can occupy (for size 3, radius 2 = 5x5 = 25 cells) */
#define MAX_SIZE_CELLS 25

/* Structure to hold the cell pattern for a unit size */
typedef struct {
    int8_t count;                   // number of cells occupied
    point_t cells[MAX_SIZE_CELLS];  // relative offsets from center
} size_pattern_t;

/* Get the hardcoded pattern for a given size
 * size 1 = single cell (radius 0)
 * size 2 = 3x3 grid (radius 1)
 * size 3 = 5x5 grid (radius 2)
 */
const size_pattern_t* get_size_pattern(st_points_t size);

/* Check if all cells required for a unit of given size at position are empty
 * Returns 1 if all cells are empty, 0 otherwise
 */
int can_fit_at_position(ipc_ctx_t *ctx, point_t center, st_points_t size, unit_id_t ignore_unit);

/* Get all grid positions occupied by a unit at center with given size */
void get_occupied_cells(point_t center, st_points_t size, point_t *out_cells, int *out_count);

/* Find the closest cell of a target unit to the attacker position
 * Returns the position of the closest cell
 */
point_t get_closest_cell_to_attacker(point_t attacker_pos, point_t target_center, st_points_t target_size);

/* Place a unit on the grid at all cells it occupies */
void place_unit_on_grid(ipc_ctx_t *ctx, unit_id_t unit_id, point_t center, st_points_t size);

/* Remove a unit from all grid cells it occupies */
void remove_unit_from_grid(ipc_ctx_t *ctx, unit_id_t unit_id, point_t center, st_points_t size);

#endif
