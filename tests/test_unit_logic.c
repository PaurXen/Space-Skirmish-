#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "ipc/shared.h"
#include "unit_logic.h"

/* ---------- tiny test helpers ---------- */

#define TEST_START(name) \
    do { printf("[TEST] %s ...\n", name); fflush(stdout); } while (0)

#define TEST_OK() \
    do { printf("       OK\n"); } while (0)

#define CHECK(cond, fmt, ...) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "\n[FAIL] " fmt "\n", ##__VA_ARGS__); \
            assert(cond); \
        } \
    } while (0)

/* ---------- geometry helpers ---------- */

static int in_bounds(point_t p, int w, int h) {
    return (p.x >= 0 && p.x < w && p.y >= 0 && p.y < h);
}

static int in_disk(point_t p, int cx, int cy, int r) {
    int dx = (int)p.x - cx;
    int dy = (int)p.y - cy;
    return (dx*dx + dy*dy) <= r*r;
}

// Border = inside disk, but at least one 8-neighbor outside disk
static int on_disk_border(point_t p, int cx, int cy, int r) {
    if (!in_disk(p, cx, cy, r)) return 0;
    for (int oy = -1; oy <= 1; oy++) {
        for (int ox = -1; ox <= 1; ox++) {
            if (ox == 0 && oy == 0) continue;
            point_t q = { (int16_t)(p.x + ox), (int16_t)(p.y + oy) };
            if (!in_disk(q, cx, cy, r)) return 1;
        }
    }
    return 0;
}

/* ---------- tests ---------- */

static void test_in_circle_basic(void) {
    TEST_START("radar_pick_random_point_in_circle / basic");

    srand(0);
    point_t p;
    int ok = radar_pick_random_point_in_circle(5, 5, 3, 20, 20, &p);

    printf("       returned ok=%d, p=(%d,%d)\n", ok, p.x, p.y);

    CHECK(ok == 1, "expected ok=1");
    CHECK(in_bounds(p, 20, 20),
          "point out of bounds: (%d,%d)", p.x, p.y);
    CHECK(in_disk(p, 5, 5, 3),
          "point not in disk: (%d,%d)", p.x, p.y);

    TEST_OK();
}

static void test_in_circle_clipped_by_bounds(void) {
    TEST_START("radar_pick_random_point_in_circle / clipped by bounds");

    srand(1);
    point_t p;
    int ok = radar_pick_random_point_in_circle(0, 0, 5, 4, 4, &p);

    printf("       returned ok=%d, p=(%d,%d)\n", ok, p.x, p.y);

    CHECK(ok == 1, "expected ok=1");
    CHECK(in_bounds(p, 4, 4),
          "point out of bounds: (%d,%d)", p.x, p.y);
    CHECK(in_disk(p, 0, 0, 5),
          "point not in disk relative to center");

    TEST_OK();
}

static void test_in_circle_radius_zero(void) {
    TEST_START("radar_pick_random_point_in_circle / radius 0");

    srand(2);
    point_t p;
    int ok = radar_pick_random_point_in_circle(2, 3, 0, 10, 10, &p);

    printf("       returned ok=%d, p=(%d,%d)\n", ok, p.x, p.y);

    CHECK(ok == 1, "expected ok=1");
    CHECK(p.x == 2 && p.y == 3,
          "radius=0 must return center");

    TEST_OK();
}

static void test_in_circle_invalid(void) {
    TEST_START("radar_pick_random_point_in_circle / invalid params");

    srand(3);
    point_t p;

    CHECK(radar_pick_random_point_in_circle(1,1,-1,10,10,&p) == 0,
          "negative radius should fail");
    CHECK(radar_pick_random_point_in_circle(1,1,2,0,10,&p) == 0,
          "zero width should fail");
    CHECK(radar_pick_random_point_in_circle(1,1,2,10,0,&p) == 0,
          "zero height should fail");
    CHECK(radar_pick_random_point_in_circle(1,1,2,10,10,NULL) == 0,
          "NULL output pointer should fail");

    TEST_OK();
}

static void test_on_border_basic(void) {
    TEST_START("radar_pick_random_point_on_circle_border / basic");

    srand(4);
    point_t p;
    int ok = radar_pick_random_point_on_circle_border(10, 10, 5, 50, 50, &p);

    printf("       returned ok=%d, p=(%d,%d)\n", ok, p.x, p.y);

    CHECK(ok == 1, "expected ok=1");
    CHECK(in_bounds(p, 50, 50),
          "point out of bounds: (%d,%d)", p.x, p.y);
    CHECK(on_disk_border(p, 10, 10, 5),
          "point not on disk border");

    TEST_OK();
}

static void test_on_border_radius_one(void) {
    TEST_START("radar_pick_random_point_on_circle_border / radius 1");

    srand(5);
    point_t p;
    int ok = radar_pick_random_point_on_circle_border(5, 5, 1, 20, 20, &p);

    printf("       returned ok=%d, p=(%d,%d)\n", ok, p.x, p.y);

    CHECK(ok == 1, "expected ok=1");
    CHECK(!(p.x == 5 && p.y == 5),
          "border point must not be center");
    CHECK(on_disk_border(p, 5, 5, 1),
          "point not on border");

    TEST_OK();
}

static void test_on_border_invalid(void) {
    TEST_START("radar_pick_random_point_on_circle_border / invalid params");

    srand(6);
    point_t p;

    CHECK(radar_pick_random_point_on_circle_border(1,1,-1,10,10,&p) == 0,
          "negative radius should fail");
    CHECK(radar_pick_random_point_on_circle_border(1,1,2,0,10,&p) == 0,
          "zero width should fail");
    CHECK(radar_pick_random_point_on_circle_border(1,1,2,10,0,&p) == 0,
          "zero height should fail");
    CHECK(radar_pick_random_point_on_circle_border(1,1,2,10,10,NULL) == 0,
          "NULL output pointer should fail");

    TEST_OK();
}

/* ---------- main ---------- */

int main(void) {
    test_in_circle_basic();
    test_in_circle_clipped_by_bounds();
    test_in_circle_radius_zero();
    test_in_circle_invalid();

    test_on_border_basic();
    test_on_border_radius_one();
    test_on_border_invalid();

    printf("\nALL unit_logic radar tests PASSED ✅\n");
    return 0;
}
