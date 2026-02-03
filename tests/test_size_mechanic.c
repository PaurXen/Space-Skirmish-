#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "CC/unit_size.h"
#include "ipc/shared.h"
#include "ipc/ipc_context.h"

/* Mock IPC context for testing */
static shm_state_t mock_shm;
static ipc_ctx_t mock_ctx;

void setup_mock_context() {
    memset(&mock_shm, 0, sizeof(mock_shm));
    mock_ctx.S = &mock_shm;
    mock_shm.magic = SHM_MAGIC;
}

void test_size_patterns() {
    printf("Testing size patterns...\n");
    
    // Test size 1 (single cell)
    const size_pattern_t *p1 = get_size_pattern(1);
    assert(p1->count == 1);
    assert(p1->cells[0].x == 0 && p1->cells[0].y == 0);
    printf("  ✓ Size 1 pattern correct (1 cell)\n");
    
    // Test size 2 (cross/plus pattern = 5 cells)
    const size_pattern_t *p2 = get_size_pattern(2);
    assert(p2->count == 5);
    // Check that it includes center
    int has_center = 0;
    for (int i = 0; i < p2->count; i++) {
        if (p2->cells[i].x == 0 && p2->cells[i].y == 0) {
            has_center = 1;
            break;
        }
    }
    assert(has_center);
    printf("  ✓ Size 2 pattern correct (5 cells cross, includes center)\n");
    
    // Test size 3 (diamond pattern = 13 cells)
    const size_pattern_t *p3 = get_size_pattern(3);
    assert(p3->count == 13);
    // Check that center exists
    int has_center_3 = 0;
    for (int i = 0; i < p3->count; i++) {
        if (p3->cells[i].x == 0 && p3->cells[i].y == 0) {
            has_center_3 = 1;
            break;
        }
    }
    assert(has_center_3);
    printf("  ✓ Size 3 pattern correct (13 cells diamond)\n");
}

void test_can_fit_at_position() {
    printf("Testing can_fit_at_position...\n");
    setup_mock_context();
    
    // Test size 1 in empty grid
    assert(can_fit_at_position(&mock_ctx, (point_t){10, 10}, 1, 0) == 1);
    printf("  ✓ Size 1 fits in empty grid\n");
    
    // Test size 2 in empty grid
    assert(can_fit_at_position(&mock_ctx, (point_t){10, 10}, 2, 0) == 1);
    printf("  ✓ Size 2 fits in empty grid\n");
    
    // Occupy a cell and test collision
    mock_ctx.S->grid[10][10] = 5;
    assert(can_fit_at_position(&mock_ctx, (point_t){10, 10}, 1, 0) == 0);
    printf("  ✓ Size 1 detects occupied cell\n");
    
    // Test ignoring own unit
    assert(can_fit_at_position(&mock_ctx, (point_t){10, 10}, 1, 5) == 1);
    printf("  ✓ Ignores own unit correctly\n");
    
    // Test size 2 with partial collision
    mock_ctx.S->grid[10][10] = 0;
    mock_ctx.S->grid[11][10] = 7; // occupy adjacent cell
    assert(can_fit_at_position(&mock_ctx, (point_t){10, 10}, 2, 0) == 0);
    printf("  ✓ Size 2 detects partial collision\n");
    
    // Test bounds checking
    assert(can_fit_at_position(&mock_ctx, (point_t){0, 0}, 2, 0) == 0); // -1 out of bounds
    printf("  ✓ Detects out of bounds for size 2 at edge\n");
    
    assert(can_fit_at_position(&mock_ctx, (point_t){1, 1}, 2, 0) == 1); // should fit
    printf("  ✓ Size 2 fits at (1,1)\n");
}

void test_get_occupied_cells() {
    printf("Testing get_occupied_cells...\n");
    
    point_t cells[MAX_SIZE_CELLS];
    int count;
    
    // Test size 1
    get_occupied_cells((point_t){5, 5}, 1, cells, &count);
    assert(count == 1);
    assert(cells[0].x == 5 && cells[0].y == 5);
    printf("  ✓ Size 1 returns single cell\n");
    
    // Test size 2 (cross pattern = 5 cells)
    get_occupied_cells((point_t){10, 10}, 2, cells, &count);
    assert(count == 5);
    // Check that center is included
    int found_center = 0;
    for (int i = 0; i < count; i++) {
        if (cells[i].x == 10 && cells[i].y == 10) {
            found_center = 1;
            break;
        }
    }
    assert(found_center);
    printf("  ✓ Size 2 returns 5 cells (cross) including center\n");
    
    // Test size 3 (diamond = 13 cells)
    get_occupied_cells((point_t){20, 20}, 3, cells, &count);
    assert(count == 13);
    printf("  ✓ Size 3 returns 13 cells (diamond)\n");
}

void test_closest_cell() {
    printf("Testing get_closest_cell_to_attacker...\n");
    
    // Size 1: closest should be center
    point_t closest = get_closest_cell_to_attacker(
        (point_t){0, 0},
        (point_t){10, 10},
        1
    );
    assert(closest.x == 10 && closest.y == 10);
    printf("  ✓ Size 1 returns center\n");
    
    // Size 2 (cross): attacker to the left should get leftmost cell
    closest = get_closest_cell_to_attacker(
        (point_t){5, 10},
        (point_t){10, 10},
        2
    );
    assert(closest.x == 9); // left arm of cross
    assert(closest.y == 10);
    printf("  ✓ Size 2 returns closest cell (left arm)\n");
    
    // Size 3 (diamond): attacker above should get top cell
    closest = get_closest_cell_to_attacker(
        (point_t){20, 5},
        (point_t){20, 20},
        3
    );
    assert(closest.x == 20);
    assert(closest.y == 18); // top of diamond
    printf("  ✓ Size 3 returns closest cell (top)\n");
    
    // Size 2: attacker at diagonal should get closest diagonal cell
    closest = get_closest_cell_to_attacker(
        (point_t){5, 5},
        (point_t){15, 15},
        2
    );
    // Cross pattern doesn't have diagonal corners, so it should be center or adjacent
    assert((closest.x == 14 && closest.y == 15) || (closest.x == 15 && closest.y == 14) || (closest.x == 15 && closest.y == 15));
    printf("  ✓ Size 2 returns closest cell for diagonal attacker\n");
}

void test_place_and_remove_unit() {
    printf("Testing place_unit_on_grid and remove_unit_from_grid...\n");
    setup_mock_context();
    
    // Place size 1 unit
    place_unit_on_grid(&mock_ctx, 10, (point_t){5, 5}, 1);
    assert(mock_ctx.S->grid[5][5] == 10);
    printf("  ✓ Size 1 unit placed correctly\n");
    
    // Remove size 1 unit
    remove_unit_from_grid(&mock_ctx, 10, (point_t){5, 5}, 1);
    assert(mock_ctx.S->grid[5][5] == 0);
    printf("  ✓ Size 1 unit removed correctly\n");
    
    // Place size 2 unit (cross pattern = 5 cells)
    place_unit_on_grid(&mock_ctx, 20, (point_t){10, 10}, 2);
    int cells_occupied = 0;
    // Check center
    if (mock_ctx.S->grid[10][10] == 20) cells_occupied++;
    // Check cross arms
    if (mock_ctx.S->grid[10][9] == 20) cells_occupied++;   // up
    if (mock_ctx.S->grid[10][11] == 20) cells_occupied++;  // down
    if (mock_ctx.S->grid[9][10] == 20) cells_occupied++;   // left
    if (mock_ctx.S->grid[11][10] == 20) cells_occupied++;  // right
    assert(cells_occupied == 5);
    printf("  ✓ Size 2 unit occupies 5 cells (cross)\n");
    
    // Remove size 2 unit
    remove_unit_from_grid(&mock_ctx, 20, (point_t){10, 10}, 2);
    cells_occupied = 0;
    if (mock_ctx.S->grid[10][10] == 20) cells_occupied++;
    if (mock_ctx.S->grid[10][9] == 20) cells_occupied++;
    if (mock_ctx.S->grid[10][11] == 20) cells_occupied++;
    if (mock_ctx.S->grid[9][10] == 20) cells_occupied++;
    if (mock_ctx.S->grid[11][10] == 20) cells_occupied++;
    assert(cells_occupied == 0);
    printf("  ✓ Size 2 unit removed from all cells\n");
    
    // Test that remove only clears own cells
    mock_ctx.S->grid[15][15] = 30; // different unit
    place_unit_on_grid(&mock_ctx, 20, (point_t){15, 15}, 2);
    mock_ctx.S->grid[15][15] = 30; // restore other unit at center
    remove_unit_from_grid(&mock_ctx, 20, (point_t){15, 15}, 2);
    assert(mock_ctx.S->grid[15][15] == 30); // other unit preserved
    printf("  ✓ Remove doesn't clear other units' cells\n");
}

void test_edge_cases() {
    printf("Testing edge cases...\n");
    setup_mock_context();
    
    // Size 2 (cross) near left edge - needs at least 1 cell left for left arm
    assert(can_fit_at_position(&mock_ctx, (point_t){1, 10}, 2, 0) == 1);
    printf("  ✓ Size 2 can fit at x=1 (cross needs 1 cell left)\n");
    assert(can_fit_at_position(&mock_ctx, (point_t){0, 10}, 2, 0) == 0); // too close to left edge
    printf("  ✓ Size 2 cannot fit at x=0 (cross needs left arm)\n");
    
    // Size 2 near top edge - needs at least 1 cell up for top arm
    assert(can_fit_at_position(&mock_ctx, (point_t){10, 1}, 2, 0) == 1);
    printf("  ✓ Size 2 can fit at y=1 (cross needs 1 cell up)\n");
    assert(can_fit_at_position(&mock_ctx, (point_t){10, 0}, 2, 0) == 0); // too close to top
    printf("  ✓ Size 2 cannot fit at y=0 (cross needs top arm)\n");
    
    // Size 2 near right edge - M=80, right arm needs x+1 < 80
    assert(can_fit_at_position(&mock_ctx, (point_t){78, 10}, 2, 0) == 1); // x+1=79, ok
    printf("  ✓ Size 2 can fit at x=78 (cross right arm fits)\n");
    assert(can_fit_at_position(&mock_ctx, (point_t){79, 10}, 2, 0) == 0); // x+1=80, out
    printf("  ✓ Size 2 cannot fit at x=79 (cross right arm out)\n");
    
    // Size 2 near bottom edge - N=40, bottom arm needs y+1 < 40
    assert(can_fit_at_position(&mock_ctx, (point_t){10, 38}, 2, 0) == 1); // y+1=39, ok
    printf("  ✓ Size 2 can fit at y=38 (cross bottom arm fits)\n");
    assert(can_fit_at_position(&mock_ctx, (point_t){10, 39}, 2, 0) == 0); // y+1=40, out
    printf("  ✓ Size 2 cannot fit at y=39 (cross bottom arm out)\n");
    
    // Size 3 (diamond) near left edge - needs at least 2 cells left for diamond extent
    assert(can_fit_at_position(&mock_ctx, (point_t){2, 10}, 3, 0) == 1);
    printf("  ✓ Size 3 can fit at x=2 (diamond needs 2 cells left)\n");
    assert(can_fit_at_position(&mock_ctx, (point_t){1, 10}, 3, 0) == 0); // too close to left
    printf("  ✓ Size 3 cannot fit at x=1 (diamond needs 2 left)\n");
    
    // Size 3 near top edge
    assert(can_fit_at_position(&mock_ctx, (point_t){10, 2}, 3, 0) == 1);
    printf("  ✓ Size 3 can fit at y=2 (diamond needs 2 cells up)\n");
    assert(can_fit_at_position(&mock_ctx, (point_t){10, 1}, 3, 0) == 0); // too close to top
    printf("  ✓ Size 3 cannot fit at y=1 (diamond needs 2 up)\n");
    
    // Size 3 near right edge - needs x+2 < 80
    assert(can_fit_at_position(&mock_ctx, (point_t){77, 10}, 3, 0) == 1); // x+2=79, ok
    printf("  ✓ Size 3 can fit at x=77 (diamond right fits)\n");
    assert(can_fit_at_position(&mock_ctx, (point_t){78, 10}, 3, 0) == 0); // x+2=80, out
    printf("  ✓ Size 3 cannot fit at x=78 (diamond right out)\n");
    
    // Size 3 near bottom edge - needs y+2 < 40
    assert(can_fit_at_position(&mock_ctx, (point_t){10, 37}, 3, 0) == 1); // y+2=39, ok
    printf("  ✓ Size 3 can fit at y=37 (diamond bottom fits)\n");
    assert(can_fit_at_position(&mock_ctx, (point_t){10, 38}, 3, 0) == 0); // y+2=40, out
    printf("  ✓ Size 3 cannot fit at y=38 (diamond bottom out)\n");
    
    // Collision test - place unit, check can_fit returns 0
    place_unit_on_grid(&mock_ctx, 100, (point_t){20, 20}, 1);
    assert(can_fit_at_position(&mock_ctx, (point_t){20, 20}, 1, 0) == 0);
    printf("  ✓ Cannot fit at occupied position\n");
    
    // Collision with size 2 cross - any overlapping cell should block
    remove_unit_from_grid(&mock_ctx, 100, (point_t){20, 20}, 1);
    place_unit_on_grid(&mock_ctx, 100, (point_t){30, 30}, 2);
    assert(can_fit_at_position(&mock_ctx, (point_t){30, 30}, 1, 0) == 0); // center occupied
    assert(can_fit_at_position(&mock_ctx, (point_t){29, 30}, 1, 0) == 0); // left arm occupied
    assert(can_fit_at_position(&mock_ctx, (point_t){31, 30}, 1, 0) == 0); // right arm occupied
    assert(can_fit_at_position(&mock_ctx, (point_t){30, 29}, 1, 0) == 0); // top arm occupied
    assert(can_fit_at_position(&mock_ctx, (point_t){30, 31}, 1, 0) == 0); // bottom arm occupied
    printf("  ✓ Cannot fit at any cell occupied by cross pattern\n");
    
    // Test invalid sizes (fallback to size 1)
    const size_pattern_t *p = get_size_pattern(99);
    assert(p->count == 1);
    printf("  ✓ Invalid size falls back to size 1\n");
}

int main() {
    printf("========================================\n");
    printf("  Size Mechanic Unit Tests\n");
    printf("========================================\n\n");
    
    test_size_patterns();
    printf("\n");
    
    test_can_fit_at_position();
    printf("\n");
    
    test_get_occupied_cells();
    printf("\n");
    
    test_closest_cell();
    printf("\n");
    
    test_place_and_remove_unit();
    printf("\n");
    
    test_edge_cases();
    printf("\n");
    
    printf("========================================\n");
    printf("  All tests passed! ✓\n");
    printf("========================================\n");
    
    return 0;
}
