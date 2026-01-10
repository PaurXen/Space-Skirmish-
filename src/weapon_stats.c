#include "weapon_stats.h"
#include "ipc/shared.h"

weapon_stats_t weapon_stats_for_weapon_type(weapon_type_t type) {
    switch (type)
    {
        case LR_CANNON: return (weapon_stats_t){.dmg = 10, .range = 15, .type = type};
        case MR_CANNON: return (weapon_stats_t){.dmg = 10, .range = 10, .type = type};
        case SR_CANNON: return (weapon_stats_t){.dmg = 10, .range = 5, .type = type};
        case LR_GUN: return (weapon_stats_t){.dmg = 10, .range = 15, .type = type};
        case MR_GUN: return (weapon_stats_t){.dmg = 10, .range = 15, .type = type};
        case SR_GUN: return (weapon_stats_t){.dmg = 10, .range = 5, .type = type};
        default: return (weapon_stats_t){.dmg = 0, .range = 0, .type = NONE};
    }
}

weapon_loadout_view_t weapon_loadout_for_unit_type(unit_type_t type) {
    static const weapon_type_t flagship[]  = { LR_CANNON, LR_CANNON, MR_GUN, MR_GUN };
    static const weapon_type_t destroyer[] = { LR_CANNON, LR_CANNON, MR_GUN };
    static const weapon_type_t carrier[]   = { LR_CANNON, MR_GUN, MR_GUN };
    static const weapon_type_t fighter[]   = { SR_GUN };
    static const weapon_type_t bomber[]    = { SR_CANNON };
    static const weapon_type_t elite[]     = { SR_GUN };
    static const weapon_type_t none[]     = {};

    switch (type) {
        case TYPE_FLAGSHIP:  return (weapon_loadout_view_t){ flagship,  4 };
        case TYPE_DESTROYER: return (weapon_loadout_view_t){ destroyer, 3 };
        case TYPE_CARRIER:   return (weapon_loadout_view_t){ carrier,   3 };
        case TYPE_FIGTER:    return (weapon_loadout_view_t){ fighter,   1 };
        case TYPE_BOMBER:    return (weapon_loadout_view_t){ bomber,    1 };
        case TYPE_ELITE:     return (weapon_loadout_view_t){ elite,     1 };
        default:             return (weapon_loadout_view_t){ none,      0 };
    }
}