#include <stdint.h>
#include "ipc/shared.h"
#include "CC/weapon_stats.h"

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

weapon_stats_t weapon_stats_for_weapon_type(weapon_type_t type) {
    switch (type) {
        case LR_CANNON: return (weapon_stats_t){ .dmg = 10, .range = 15, .w_target = 0, .type = type };
        case MR_CANNON: return (weapon_stats_t){ .dmg = 10, .range = 10, .w_target = 0, .type = type };
        case SR_CANNON: return (weapon_stats_t){ .dmg = 10, .range = 5,  .w_target = 0, .type = type };
        case LR_GUN:    return (weapon_stats_t){ .dmg = 10, .range = 15, .w_target = 0, .type = type };
        case MR_GUN:    return (weapon_stats_t){ .dmg = 10, .range = 10, .w_target = 0, .type = type };
        case SR_GUN:    return (weapon_stats_t){ .dmg = 10, .range = 5,  .w_target = 0, .type = type };
        default:        return (weapon_stats_t){ .dmg = 0,  .range = 0,  .w_target = 0, .type = NONE };
    }
}



/*
 * One row per unit type, each row is MAX_WEAPONS long.
 * Any unspecified entries default to 0, so make sure NONE == 0 in your enum
 * (it usually is). If not, we’ll handle filling NONE below anyway.
 */
static const weapon_type_t k_loadout_types[][MAX_WEAPONS] = {
    [TYPE_FLAGSHIP]  = { LR_CANNON, LR_CANNON, MR_GUN,   MR_GUN   },
    [TYPE_DESTROYER] = { LR_CANNON, LR_CANNON, MR_GUN,   NONE     },
    [TYPE_CARRIER]   = { LR_CANNON, MR_GUN,    MR_GUN,   NONE     },
    [TYPE_FIGHTER]    = { SR_GUN,    NONE,      NONE,     NONE     },
    [TYPE_BOMBER]    = { SR_CANNON, NONE,      NONE,     NONE     },
    [TYPE_ELITE]     = { SR_GUN,    NONE,      NONE,     NONE     },
};

static const uint8_t k_loadout_counts[] = {
    [TYPE_FLAGSHIP]  = 4,
    [TYPE_DESTROYER] = 3,
    [TYPE_CARRIER]   = 3,
    [TYPE_FIGHTER]    = 1,
    [TYPE_BOMBER]    = 1,
    [TYPE_ELITE]     = 1,
};

weapon_loadout_types_view_t weapon_loadout_types_for_unit_type(unit_type_t type)
{
    weapon_loadout_types_view_t out = { .types = { NONE, NONE, NONE, NONE }, .n = 0 };

    if ((size_t)type >= ARRAY_LEN(k_loadout_counts))
        return out;

    out.n = k_loadout_counts[type];

    if ((size_t)type < ARRAY_LEN(k_loadout_types)) {
        for (int i = 0; i < MAX_WEAPONS; i++)
            out.types[i] = k_loadout_types[type][i];
    }

    return out;
}

weapon_loadout_view_t weapon_loadout_for_unit_type(unit_type_t type)
{
    weapon_loadout_view_t out = {0};
    weapon_loadout_types_view_t lo = weapon_loadout_types_for_unit_type(type);

    out.count = (uint8_t)lo.n;

    for (int i = 0; i < MAX_WEAPONS; i++) {
        out.arr[i] = weapon_stats_for_weapon_type(lo.types[i]);
        out.arr[i].w_target = 0;
    }

    return out;
}

