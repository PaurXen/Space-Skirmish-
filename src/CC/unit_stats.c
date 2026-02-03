#include "CC/unit_stats.h"
#include "CC/weapon_stats.h"
#include "ipc/shared.h"

static const fighter_bay_view_t k_fighter_types(unit_type_t type){
    switch (type) {
        case TYPE_FLAGSHIP:  return (fighter_bay_view_t){.capacity = 5, .current = 0, .sq_types = {TYPE_ELITE, TYPE_ELITE, TYPE_BOMBER, TYPE_ELITE, TYPE_BOMBER, DUMMY}};
        case TYPE_CARRIER:   return (fighter_bay_view_t){.capacity = 3, .current = 0, .sq_types = {TYPE_BOMBER, TYPE_BOMBER, TYPE_FIGHTER, TYPE_BOMBER, TYPE_FIGHTER, TYPE_FIGHTER}};
        case TYPE_DESTROYER: return (fighter_bay_view_t){.capacity = 2, .current = 0, .sq_types = {TYPE_FIGHTER, TYPE_FIGHTER, TYPE_BOMBER, TYPE_FIGHTER, DUMMY, DUMMY}};
        case TYPE_FIGHTER:    return (fighter_bay_view_t){.capacity = 0, .current = 0, .sq_types = {DUMMY,DUMMY,DUMMY,DUMMY,DUMMY,DUMMY}};
        case TYPE_BOMBER:    return (fighter_bay_view_t){.capacity = 0, .current = 0, .sq_types = {DUMMY,DUMMY,DUMMY,DUMMY,DUMMY,DUMMY}};
        case TYPE_ELITE:     return (fighter_bay_view_t){.capacity = 0, .current = 0, .sq_types = {DUMMY,DUMMY,DUMMY,DUMMY,DUMMY,DUMMY}};
        default:             return (fighter_bay_view_t){.capacity = 0, .current = 0, .sq_types = {DUMMY,DUMMY,DUMMY,DUMMY,DUMMY,DUMMY}};
    }
};

unit_stats_t unit_stats_for_type(unit_type_t type) {
    switch (type) {
        case DUMMY:          return (unit_stats_t){.hp = 200,   .sh = 100,  .en = -1,   .sp = 0,    .si = 1,    .dr = 20,   .ba = weapon_loadout_for_unit_type(type),   .fb = k_fighter_types(type)};
        case TYPE_FLAGSHIP:  return (unit_stats_t){.hp = 200,   .sh = 100,  .en = -1,   .sp = 2,    .si = 3,    .dr = M,    .ba = weapon_loadout_for_unit_type(type),   .fb = k_fighter_types(type)};
        case TYPE_DESTROYER: return (unit_stats_t){.hp = 100,   .sh = 100,  .en = -1,   .sp = 3,    .si = 2,    .dr = 20,   .ba = weapon_loadout_for_unit_type(type),   .fb = k_fighter_types(type)};
        case TYPE_CARRIER:   return (unit_stats_t){.hp = 100,   .sh = 100,  .en = -1,   .sp = 6,    .si = 2,    .dr = 20,   .ba = weapon_loadout_for_unit_type(type),   .fb = k_fighter_types(type)};
        case TYPE_FIGHTER:    return (unit_stats_t){.hp = 20,    .sh = 0,    .en = 20,   .sp = 5,    .si = 1,    .dr = 10,   .ba = weapon_loadout_for_unit_type(type),   .fb = k_fighter_types(type)};
        case TYPE_BOMBER:    return (unit_stats_t){.hp = 30,    .sh = 0,    .en = 20,   .sp = 4,    .si = 1,    .dr = 15,   .ba = weapon_loadout_for_unit_type(type),   .fb = k_fighter_types(type)};
        case TYPE_ELITE:     return (unit_stats_t){.hp = 20,    .sh = 20,   .en = 20,   .sp = 6,    .si = 1,    .dr = 15,   .ba = weapon_loadout_for_unit_type(type),   .fb = k_fighter_types(type)};
        default:             return (unit_stats_t){.hp = 100,   .sh = 100,  .en = -1,   .sp = 3,    .si = 2,    .dr = 20,   .ba = weapon_loadout_for_unit_type(type),   .fb = k_fighter_types(type)};
    }
}
