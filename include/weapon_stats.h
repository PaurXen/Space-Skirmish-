#ifndef WEAPON_STATS_H
#define WEAPON_STATS_H
#include <stdint.h>

#include "ipc/shared.h"


weapon_stats_t weapon_stats_for_weapon_type(weapon_type_t type);
weapon_loadout_view_t weapon_loadout_for_unit_type(unit_type_t type);

#endif