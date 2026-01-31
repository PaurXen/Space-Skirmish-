#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "unit_logic.h"
#include "ipc/shared.h"

static const char *weapon_name(weapon_type_t w) {
    switch (w) {
        case NONE: return "NONE";
        case LR_CANNON: return "LR_CANNON";
        case MR_CANNON: return "MR_CANNON";
        case SR_CANNON: return "SR_CANNON";
        case LR_GUN: return "LR_GUN";
        case MR_GUN: return "MR_GUN";
        case SR_GUN: return "SR_GUN";
        default: return "UNKNOWN";
    }
}

static const char *unit_name(unit_type_t u) {
    switch (u) {
        case DUMMY: return "DUMMY";
        case TYPE_FLAGSHIP: return "FLAGSHIP";
        case TYPE_DESTROYER: return "DESTROYER";
        case TYPE_CARRIER: return "CARRIER";
        case TYPE_FIGTER: return "FIGTER";
        case TYPE_BOMBER: return "BOMBER";
        case TYPE_ELITE: return "ELITE";
        default: return "UNKNOWN";
    }
}

static int float_eq(float a, float b) {
    return fabsf(a - b) < 1e-6f;
}

int main(void) {
    weapon_type_t weapons[] = { NONE, LR_CANNON, MR_CANNON, SR_CANNON, LR_GUN, MR_GUN, SR_GUN };
    const int n_weapons = sizeof(weapons) / sizeof(weapons[0]);

    unit_type_t units[] = { DUMMY, TYPE_FLAGSHIP, TYPE_DESTROYER, TYPE_CARRIER, TYPE_FIGTER, TYPE_BOMBER, TYPE_ELITE };
    const int n_units = sizeof(units) / sizeof(units[0]);

    printf("Accuracy table (weapon -> target):\n\n");

    // header
    printf("%12s", "");
    for (int j = 0; j < n_units; j++) printf("%10s", unit_name(units[j]));
    printf("\n");

    // table rows
    for (int i = 0; i < n_weapons; i++) {
        weapon_type_t w = weapons[i];
        printf("%12s", weapon_name(w));
        for (int j = 0; j < n_units; j++) {
            unit_type_t u = units[j];
            float acc = accuracy_multiplier(w, u);
            printf("%10.2f", acc);
        }
        printf("\n");
    }

    // Run a set of explicit assertions for expected behavior
    struct {
        weapon_type_t w;
        unit_type_t u;
        float expected;
    } tests[] = {
        { NONE, TYPE_FLAGSHIP, 0.0f },
        { LR_CANNON, TYPE_FLAGSHIP, 0.75f },
        { LR_CANNON, TYPE_FIGTER, 0.25f },
        { SR_GUN, TYPE_FLAGSHIP, 0.0f },
        { SR_GUN, TYPE_FIGTER, 0.75f },
        { MR_GUN, TYPE_BOMBER, 0.75f },
        { SR_CANNON, TYPE_ELITE, 0.25f },
    };

    int n_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;

    printf("\nRunning %d explicit checks...\n", n_tests);
    for (int t = 0; t < n_tests; t++) {
        float got = accuracy_multiplier(tests[t].w, tests[t].u);
        if (float_eq(got, tests[t].expected)) {
            printf("[PASS] %10s vs %8s -> %.2f\n", weapon_name(tests[t].w), unit_name(tests[t].u), got);
            passed++;
        } else {
            printf("[FAIL] %10s vs %8s -> got %.2f expected %.2f\n",
                weapon_name(tests[t].w), unit_name(tests[t].u), got, tests[t].expected);
        }
    }

    printf("\nSummary: %d/%d checks passed.\n", passed, n_tests);

    return (passed == n_tests) ? 0 : 2;
}
