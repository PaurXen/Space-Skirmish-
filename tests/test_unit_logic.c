// test_unit_logic.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "unit_logic.h"


/* -------------------------
   small helpers
   ------------------------- */

static void die_usage(const char *argv0) {
    fprintf(stderr,
        "Usage: %s [seed]\n"
        "  seed: optional integer seed for rand()\n", argv0);
    exit(2);
}

static int in_bounds_i(int x, int y, int w, int h) {
    return (x >= 0 && x < w && y >= 0 && y < h);
}

static int in_disk_i(int x, int y, int cx, int cy, int r) {
    int dx = x - cx;
    int dy = y - cy;
    return dx*dx + dy*dy <= r*r;
}

// 4-neighbor border definition (matches build_circle_border_offsets in unit_logic.c)
static int on_circle_border_4n_i(int x, int y, int cx, int cy, int r) {
    if (!in_disk_i(x, y, cx, cy, r)) return 0;
    if (r == 0) return (x == cx && y == cy);

    const int r2 = r*r;
    int dx = x - cx, dy = y - cy;

    int n_out =
        ((dx+1)*(dx+1) + dy*dy > r2) ||
        ((dx-1)*(dx-1) + dy*dy > r2) ||
        (dx*dx + (dy+1)*(dy+1) > r2) ||
        (dx*dx + (dy-1)*(dy-1) > r2);

    return n_out ? 1 : 0;
}

static char heat_char(int v, int vmax) {
    // map 0..vmax -> ' ' '.' ':' '-' '=' '+' '*' '#' '%' '@'
    static const char *ramp = " .:-=+*#%@";
    if (v <= 0) return ' ';
    if (vmax <= 0) return '@';
    int idx = (v * 9) / vmax;
    if (idx < 0) idx = 0;
    if (idx > 9) idx = 9;
    return ramp[idx];
}

static void print_grid_chars(const char *title, const char *buf, int w, int h) {
    printf("\n=== %s ===\n\n", title);

    printf("    ");
    for (int x = 0; x < w; x++) putchar('0' + (x % 10));
    putchar('\n');

    for (int y = 0; y < h; y++) {
        printf("%3d ", y);
        for (int x = 0; x < w; x++) {
            putchar(buf[y*w + x]);
        }
        putchar('\n');
    }
}

static void print_grid_heat(const char *title, const int *hits, int w, int h) {
    int vmax = 0;
    for (int i = 0; i < w*h; i++) if (hits[i] > vmax) vmax = hits[i];

    printf("\n=== %s ===\n", title);
    printf("(heat ramp: ' ' . : - = + * # %% @ ; max=%d)\n\n", vmax);

    printf("    ");
    for (int x = 0; x < w; x++) putchar('0' + (x % 10));
    putchar('\n');

    for (int y = 0; y < h; y++) {
        printf("%3d ", y);
        for (int x = 0; x < w; x++) {
            putchar(heat_char(hits[y*w + x], vmax));
        }
        putchar('\n');
    }
}

/* -------------------------
   demos
   ------------------------- */

// Demo 1: show circle interior + discrete border (4-neighbor) like your logic
static void demo_circle_mask(int w, int h, int cx, int cy, int r) {
    char *buf = (char*)malloc((size_t)w*h);
    if (!buf) exit(1);
    memset(buf, '.', (size_t)w*h);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!in_disk_i(x, y, cx, cy, r)) continue;
            buf[y*w + x] = 'o';
            if (on_circle_border_4n_i(x, y, cx, cy, r)) buf[y*w + x] = '#';
        }
    }
    if (in_bounds_i(cx, cy, w, h)) buf[cy*w + cx] = '@';

    print_grid_chars("Circle mask (o inside, # border, @ center)", buf, w, h);
    free(buf);
}

// Demo 2: sample random points + show hit heatmaps
static void demo_radar_heat(int w, int h, int cx, int cy, int r, int samples) {
    int *hits_in = (int*)calloc((size_t)w*h, sizeof(int));
    int *hits_bd = (int*)calloc((size_t)w*h, sizeof(int));
    if (!hits_in || !hits_bd) exit(1);

    for (int i = 0; i < samples; i++) {
        point_t p;

        if (radar_pick_random_point_in_circle((int16_t)cx, (int16_t)cy, (int16_t)r, w, h, &p)) {
            int x = p.x, y = p.y;
            if (in_bounds_i(x, y, w, h)) hits_in[y*w + x]++;
        }

        if (radar_pick_random_point_on_circle_border((point_t){(int16_t)cx, (int16_t)cy}, (int16_t)r, w, h, &p)) {
            int x = p.x, y = p.y;
            if (in_bounds_i(x, y, w, h)) hits_bd[y*w + x]++;
        }
    }

    print_grid_heat("Radar heat: pick_random_point_in_circle", hits_in, w, h);
    print_grid_heat("Radar heat: pick_random_point_on_circle_border", hits_bd, w, h);

    free(hits_in);
    free(hits_bd);
}

// Demo 3: movement + obstacles + next step
// Demo 3: movement + obstacles + next step
static void demo_movement(int w, int h, point_t from, point_t *next_step) {
    // unit_id_t **grid = (int16_t*)calloc((size_t)h*stride, sizeof(int16_t));
    // if (!grid) exit(1);
    int16_t grid[w][h];
    memset(grid, 0, sizeof(grid));
    
    // Build a simple "wall" with a gap
    for (int y = 2; y < h-2; y++) {
        int x = w/2;
        grid[x][y] = 1;
    }
    // gap
    grid[w/2][h/2] = 0;
    
    // point_t from   = { 2, (int16_t)(h/2) };
    point_t target = { (int16_t)30, (int16_t)4 };
    int16_t sp = 4;
    int16_t dr = 8;
    int approach = 1;
    
    point_t goal = from;
    point_t next = from;
    
    // Goal chosen from DR, next step chosen from SP toward that goal
    (void)unit_compute_goal_for_tick_dr(from, target, dr, w, h, &goal);
    (void)unit_next_step_towards_dr(from, goal, sp, dr, approach, w, h, grid, &next);


    char *buf = (char*)malloc((size_t)w*h);
    if (!buf) exit(1);
    memset(buf, '.', (size_t)w*h);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (grid[x][y] != 0) buf[y*w + x] = 'X';
        }
    }

    // mark detection/planning disk (DR) around 'from' (visual aid)
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!in_disk_i(x, y, from.x, from.y, dr)) continue;
            if (buf[y*w + x] == '.') buf[y*w + x] = 'd';
            if (on_circle_border_4n_i(x, y, from.x, from.y, dr) && buf[y*w + x] == 'd')
                buf[y*w + x] = 'D';
        }
    }

    // mark speed disk border around 'from' (SP) (visual aid)
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!in_disk_i(x, y, from.x, from.y, sp)) continue;
                if (buf[y*w + x] != 'X') buf[y*w + x] = 'o';
                if (on_circle_border_4n_i(x, y, from.x, from.y, sp) && buf[y*w + x] != 'X')
                    buf[y*w + x] = '#';
            }
    }

    // overlay special markers
    if (in_bounds_i(from.x, from.y, w, h))     buf[from.y*w + from.x]     = 'S';
    if (in_bounds_i(target.x, target.y, w, h)) buf[target.y*w + target.x] = 'T';
    if (in_bounds_i(goal.x, goal.y, w, h))     buf[goal.y*w + goal.x]     = 'G';
    if (in_bounds_i(next.x, next.y, w, h))     buf[next.y*w + next.x]     = 'N';

    print_grid_chars(
        "Movement demo (X obstacle, S start, T target, G goal (from DR), N next (from SP); d/D show DR, o/# show SP)",
        buf, w, h
    );

    printf("\nMovement numbers:\n");
    printf("  S=(%d,%d)  T=(%d,%d)  sp=%d  dr=%d\n", from.x, from.y, target.x, target.y, sp, dr);
    printf("  G(goal_for_tick)=(%d,%d)\n", goal.x, goal.y);
    printf("  N(next_step)     =(%d,%d)\n", next.x, next.y);

    *next_step = next;
    free(buf);
    // free(grid);
}



static void demo_move_loop(int w, int h, point_t pos, point_t target){
    do
    {
        demo_movement(w,h,pos, &pos);
    } while (!in_disk_i(pos.x, pos.y, target.x, target.y, 3));
    
} 


int main(int argc, char **argv) {
    if (argc > 2) die_usage(argv[0]);

    unsigned seed;
    if (argc == 2) seed = (unsigned)strtoul(argv[1], NULL, 10);
    else seed = (unsigned)time(NULL);

    srand(seed);
    printf("seed=%u\n", seed);

    // tweak these freely
    int w = 41, h = 21;
    int cx = 20, cy = 10;
    int r = 7;
    point_t pos = {2, 10};
    point_t target = {30, 4};

    demo_circle_mask(w, h, cx, cy, r);
    demo_radar_heat(w, h, cx, cy, r, 8000);
    demo_movement(w, h, pos, &pos);
    demo_move_loop(w,h,pos, target);

    return 0;
}
