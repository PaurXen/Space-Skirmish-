#ifndef UNIT_STATS_H
#define UNIT_STATS_H

#include <stdint.h>
#include "ipc/shared.h" 


// Returns default stats for a given unit type.
// Safe fallback is provided for unknown types.
unit_stats_t unit_stats_for_type(unit_type_t type);

#endif
