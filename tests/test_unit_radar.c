#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>


#define N 100
#define M 100

typedef struct { int16_t dx, dy; } offset_t;

typedef struct { int16_t x, y; } point_t;


static inline int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}



static int build_circle_border_offsets(int r, offset_t *out, int max_out) {
    // For each x in [-r..r], track min/max y among interior points
    int miny[256], maxy[256];
    int minx[256], maxx[256];
    int idx0 = r; // shift so x=-r maps to 0

    if (r < 0 || r > 127) return 0; // safety for fixed arrays

    for (int i = 0; i < 2*r + 1; i++) {
        miny[i] = INT_MAX; maxy[i] = INT_MIN;
        minx[i] = INT_MAX; maxx[i] = INT_MIN;
    }

    // interior points
    for (int x = -r; x <= r; x++) {
        for (int y = -r; y <= r; y++) {
            if (x*x + y*y <= r*r) {
                int xi = x + idx0;
                int yi = y + idx0;
                if (y < miny[xi]) miny[xi] = y;
                if (y > maxy[xi]) maxy[xi] = y;
                if (x < minx[yi]) minx[yi] = x;
                if (x > maxx[yi]) maxx[yi] = x;
            }
        }
    }

    // collect border points (use a small "seen" bitmap to avoid duplicates)
    // map dx,dy in [-r..r] to [0..2r] index for seen
    unsigned char seen[256][256];
    memset(seen, 0, sizeof(seen));

    int count = 0;

    // x-slices: (x, miny[x]) and (x, maxy[x])
    for (int x = -r; x <= r; x++) {
        int xi = x + idx0;
        if (miny[xi] != INT_MAX) {
            int y1 = miny[xi], y2 = maxy[xi];

            int sx = x + idx0;
            int sy1 = y1 + idx0;
            int sy2 = y2 + idx0;

            if (!seen[sx][sy1] && count < max_out) {
                seen[sx][sy1] = 1;
                out[count++] = (offset_t){ (int16_t)x, (int16_t)y1 };
            }
            if (!seen[sx][sy2] && count < max_out) {
                seen[sx][sy2] = 1;
                out[count++] = (offset_t){ (int16_t)x, (int16_t)y2 };
            }
        }
    }

    // y-slices: (minx[y], y) and (maxx[y], y)
    for (int y = -r; y <= r; y++) {
        int yi = y + idx0;
        if (minx[yi] != INT_MAX) {
            int x1 = minx[yi], x2 = maxx[yi];

            int sx1 = x1 + idx0;
            int sx2 = x2 + idx0;
            int sy  = y + idx0;

            if (!seen[sx1][sy] && count < max_out) {
                seen[sx1][sy] = 1;
                out[count++] = (offset_t){ (int16_t)x1, (int16_t)y };
            }
            if (!seen[sx2][sy] && count < max_out) {
                seen[sx2][sy] = 1;
                out[count++] = (offset_t){ (int16_t)x2, (int16_t)y };
            }
        }
    }

    return count;
}



static int build_circle_border_points_clamped(
    int cx, int cy, int r,
    point_t *out, int max_out
) {
    offset_t offs[2048];
    int n = build_circle_border_offsets(r, offs, (int)(sizeof(offs)/sizeof(offs[0])));
    if (n <= 0) return 0;

    // Deduplicate AFTER clamping to grid
    // N and M are compile-time constants in your file.
    unsigned char seen[M][N];
    memset(seen, 0, sizeof(seen));

    int count = 0;
    for (int i = 0; i < n; i++) {
        int gx = clampi(cx + offs[i].dx, 0, N - 1);
        int gy = clampi(cy + offs[i].dy, 0, M - 1);

        if (!seen[gx][gy]) {
            seen[gx][gy] = 1;

            if (count < max_out) {
                out[count++] = (point_t){ (int16_t)gx, (int16_t)gy };
            } else {
                break;
            }
        }
    }

    return count;
}

static int pick_random_target_border_clamped(int cx, int cy, int r, point_t *picked) {
    point_t pts[4096];
    int n = build_circle_border_points_clamped(
        cx, cy, r, pts, (int)(sizeof(pts)/sizeof(pts[0]))
    );

    if (n <= 0) return 0;

    int k = rand() % n;
    *picked = pts[k];
    return 1;
}


static void print_all_targets(int cx, int cy, int r) {
    point_t pts[4096];
    int n = build_circle_border_points_clamped(cx, cy, r, pts, (int)(sizeof(pts)/sizeof(pts[0])));

    printf("Unique clamped border targets for center (%d,%d), r=%d: total=%d\n[", cx, cy, r, n);
    for (int i = 0; i < n; i++) {
        printf("(%d, %d), ", pts[i].x, pts[i].y);
    }
    printf("]\n");
}

static int pick_random_point_in_circle_clamped(int cx, int cy, int r, point_t *picked) {
    // Collect all integer offsets inside/on the circle
    offset_t offs[65536]; // enough for r up to ~127 (area ~ 50k)
    int count = 0;

    for (int dx = -r; dx <= r; dx++) {
        for (int dy = -r; dy <= r; dy++) {
            if (dx*dx + dy*dy <= r*r) {
                if (count < (int)(sizeof(offs)/sizeof(offs[0]))) {
                    offs[count++] = (offset_t){ (int16_t)dx, (int16_t)dy };
                }
            }
        }
    }

    if (count == 0) return 0;

    int k = rand() % count;
    int gx = clampi(cx + offs[k].dx, 0, N - 1);
    int gy = clampi(cy + offs[k].dy, 0, M - 1);
    *picked = (point_t){ (int16_t)gx, (int16_t)gy };
    return 1;
}


int main(void) {
    srand((unsigned)time(NULL) ^ (unsigned)clock());
    int x = 5, y = 5, dr = 10;
    print_all_targets(x, y, dr);

    point_t t;
    if (pick_random_target_border_clamped(x, y, dr, &t)) {
        printf("Random picked target: (%d, %d)\n", t.x, t.y);
    } else {
        printf("No targets.\n");
    }
    point_t p;
    if (pick_random_point_in_circle_clamped(x, y, dr, &p)) {
        printf("Random point inside circle: (%d, %d)\n", p.x, p.y);
    }

    return 0;
}

