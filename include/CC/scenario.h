#pragma once
#include <stdint.h>
#include "ipc/shared.h"

#define MAX_SCENARIO_NAME 64
#define MAX_OBSTACLES 200
#define MAX_INITIAL_UNITS 32

typedef enum {
    PLACEMENT_CORNERS,
    PLACEMENT_EDGES,
    PLACEMENT_RANDOM,
    PLACEMENT_LINE,
    PLACEMENT_SCATTERED,
    PLACEMENT_MANUAL  // Explicitly specified positions
} placement_mode_t;

typedef struct {
    unit_type_t type;
    faction_t faction;
    int16_t x;
    int16_t y;
} unit_placement_t;

typedef struct {
    int16_t x;
    int16_t y;
} obstacle_t;

typedef struct {
    char name[MAX_SCENARIO_NAME];
    
    /* Map settings */
    int map_width;
    int map_height;
    
    /* Obstacles */
    obstacle_t obstacles[MAX_OBSTACLES];
    int obstacle_count;
    
    /* Initial units */
    unit_placement_t units[MAX_INITIAL_UNITS];
    int unit_count;
    
    /* Auto-generation settings (if unit_count == 0) */
    placement_mode_t placement_mode;
    int republic_flagships;
    int republic_carriers;
    int republic_destroyers;
    int republic_fighters;
    int republic_bombers;
    int republic_elites;
    int cis_flagships;
    int cis_carriers;
    int cis_destroyers;
    int cis_fighters;
    int cis_bombers;
    int cis_elites;
} scenario_t;

/* Load scenario from file, returns 0 on success, -1 on error */
int scenario_load(const char *filename, scenario_t *out);

/* Get default scenario */
void scenario_default(scenario_t *out);

/* Generate random placement for auto-generation mode */
void scenario_generate_placements(scenario_t *scenario);
