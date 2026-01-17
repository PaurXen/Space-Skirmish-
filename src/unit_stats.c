#include "unit_stats.h"
#include "weapon_stats.h"
#include "ipc/shared.h"



unit_stats_t unit_stats_for_type(unit_type_t type) {
    // You can tune these anytime; they are just defaults.
    switch (type) {
        case DUMMY:          return (unit_stats_t){.hp = 200,   .sh = 100,  .en = -1,   .sp = 0,    .si = 1,    .dr = 20,   .ba = weapon_loadout_for_unit_type(type)};
        case TYPE_FLAGSHIP:  return (unit_stats_t){.hp = 200,   .sh = 100,  .en = -1,   .sp = 2,    .si = 3,    .dr = M,    .ba = weapon_loadout_for_unit_type(type)};
        case TYPE_DESTROYER: return (unit_stats_t){.hp = 100,   .sh = 100,  .en = -1,   .sp = 3,    .si = 2,    .dr = 20,   .ba = weapon_loadout_for_unit_type(type)};
        case TYPE_CARRIER:   return (unit_stats_t){.hp = 100,   .sh = 100,  .en = -1,   .sp = 6,    .si = 2,    .dr = 20,   .ba = weapon_loadout_for_unit_type(type)};
        case TYPE_FIGTER:    return (unit_stats_t){.hp = 50,    .sh = 0,    .en = 20,   .sp = 5,    .si = 1,    .dr = 10,   .ba = weapon_loadout_for_unit_type(type)};
        case TYPE_BOMBER:    return (unit_stats_t){.hp = 50,    .sh = 0,    .en = 20,   .sp = 4,    .si = 1,    .dr = 15,   .ba = weapon_loadout_for_unit_type(type)};
        case TYPE_ELITE:     return (unit_stats_t){.hp = 50,    .sh = 20,   .en = 20,   .sp = 6,    .si = 1,    .dr = 15,   .ba = weapon_loadout_for_unit_type(type)};
        default:             return (unit_stats_t){.hp = 100,   .sh = 100,  .en = -1,   .sp = 3,    .si = 2,    .dr = 20,   .ba = weapon_loadout_for_unit_type(type)};
    }
}
