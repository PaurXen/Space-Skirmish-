#include "CC/scenario.h"
#include "error_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LINE 256

void scenario_default(scenario_t *out) {
    memset(out, 0, sizeof(scenario_t));
    strncpy(out->name, "default", MAX_SCENARIO_NAME);
    
    out->map_width = M;
    out->map_height = N;
    
    /* Default: 4 carriers at corners (current behavior) */
    out->placement_mode = PLACEMENT_CORNERS;
    out->republic_carriers = 2;
    out->republic_destroyers = 0;
    out->republic_fighters = 0;
    out->cis_carriers = 2;
    out->cis_destroyers = 0;
    out->cis_fighters = 0;
}

static void trim(char *str) {
    char *end;
    while (*str == ' ' || *str == '\t') str++;
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    memmove(str - (str - str), str, strlen(str) + 1);
}

int scenario_load(const char *filename, scenario_t *out) {
    FILE *f = CHECK_NULL_NONFATAL(fopen(filename, "r"), "scenario_load:fopen");
    if (!f) {
        return -1;
    }
    
    scenario_default(out);  // Start with defaults
    
    char line[MAX_LINE];
    char section[64] = "";
    
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        
        /* Skip empty lines and comments */
        if (line[0] == '\0' || line[0] == '#') continue;
        
        /* Section headers */
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(section, line + 1, sizeof(section) - 1);
            }
            continue;
        }
        
        /* Key=value pairs */
        char *eq = strchr(line, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char *key = line;
        char *value = eq + 1;
        trim(key);
        trim(value);
        
        /* Parse based on section */
        if (strcmp(section, "scenario") == 0) {
            if (strcmp(key, "name") == 0) {
                strncpy(out->name, value, MAX_SCENARIO_NAME - 1);
            }
        } else if (strcmp(section, "map") == 0) {
            if (strcmp(key, "width") == 0) {
                out->map_width = atoi(value);
                if (out->map_width < 40 || out->map_width > 200) out->map_width = M;
            } else if (strcmp(key, "height") == 0) {
                out->map_height = atoi(value);
                if (out->map_height < 20 || out->map_height > 100) out->map_height = N;
            }
        } else if (strcmp(section, "obstacles") == 0) {
            if (strcmp(key, "add") == 0 && out->obstacle_count < MAX_OBSTACLES) {
                int x, y;
                if (sscanf(value, "%d,%d", &x, &y) == 2) {
                    out->obstacles[out->obstacle_count].x = x;
                    out->obstacles[out->obstacle_count].y = y;
                    out->obstacle_count++;
                }
            }
        } else if (strcmp(section, "republic") == 0) {
            if (strcmp(key, "carriers") == 0) out->republic_carriers = atoi(value);
            else if (strcmp(key, "destroyers") == 0) out->republic_destroyers = atoi(value);
            else if (strcmp(key, "fighters") == 0) out->republic_fighters = atoi(value);
            else if (strcmp(key, "placement") == 0) {
                if (strcmp(value, "corners") == 0) out->placement_mode = PLACEMENT_CORNERS;
                else if (strcmp(value, "edges") == 0) out->placement_mode = PLACEMENT_EDGES;
                else if (strcmp(value, "random") == 0) out->placement_mode = PLACEMENT_RANDOM;
                else if (strcmp(value, "line") == 0) out->placement_mode = PLACEMENT_LINE;
                else if (strcmp(value, "scattered") == 0) out->placement_mode = PLACEMENT_SCATTERED;
            }
        } else if (strcmp(section, "cis") == 0) {
            if (strcmp(key, "carriers") == 0) out->cis_carriers = atoi(value);
            else if (strcmp(key, "destroyers") == 0) out->cis_destroyers = atoi(value);
            else if (strcmp(key, "fighters") == 0) out->cis_fighters = atoi(value);
        } else if (strcmp(section, "units") == 0) {
            if (strcmp(key, "add") == 0 && out->unit_count < MAX_INITIAL_UNITS) {
                char type_str[32], faction_str[32];
                int x, y;
                if (sscanf(value, "%[^,],%[^,],%d,%d", type_str, faction_str, &x, &y) == 4) {
                    unit_placement_t *u = &out->units[out->unit_count];
                    
                    /* Parse type */
                    if (strcmp(type_str, "carrier") == 0) u->type = TYPE_CARRIER;
                    else if (strcmp(type_str, "destroyer") == 0) u->type = TYPE_DESTROYER;
                    else if (strcmp(type_str, "flagship") == 0) u->type = TYPE_FLAGSHIP;
                    else if (strcmp(type_str, "fighter") == 0) u->type = TYPE_FIGHTER;
                    else if (strcmp(type_str, "bomber") == 0) u->type = TYPE_BOMBER;
                    else if (strcmp(type_str, "elite") == 0) u->type = TYPE_ELITE;
                    else u->type = atoi(type_str);
                    
                    /* Parse faction */
                    if (strcmp(faction_str, "republic") == 0) u->faction = FACTION_REPUBLIC;
                    else if (strcmp(faction_str, "cis") == 0) u->faction = FACTION_CIS;
                    else u->faction = atoi(faction_str);
                    
                    u->x = x;
                    u->y = y;
                    out->unit_count++;
                }
            }
        }
    }
    
    fclose(f);
    return 0;
}

void scenario_generate_placements(scenario_t *scenario) {
    if (scenario->unit_count > 0) return;  // Already has manual placements
    
    srand(time(NULL));
    int count = 0;
    
    /* Helper to add a unit with bounds checking */
    #define ADD_UNIT(t, f, px, py) \
        if (count < MAX_INITIAL_UNITS && (px) >= 0 && (px) < scenario->map_width && \
            (py) >= 0 && (py) < scenario->map_height) { \
            scenario->units[count].type = (t); \
            scenario->units[count].faction = (f); \
            scenario->units[count].x = (px); \
            scenario->units[count].y = (py); \
            count++; \
        }
    
    switch (scenario->placement_mode) {
        case PLACEMENT_CORNERS: {
            int idx = 0;
            for (int i = 0; i < scenario->republic_carriers; i++, idx++)
                ADD_UNIT(TYPE_CARRIER, FACTION_REPUBLIC, 5 + idx * 3, 5 + idx * 3);
            for (int i = 0; i < scenario->republic_destroyers; i++, idx++)
                ADD_UNIT(TYPE_DESTROYER, FACTION_REPUBLIC, 8 + idx * 3, 8 + idx * 3);
            for (int i = 0; i < scenario->republic_fighters; i++, idx++)
                ADD_UNIT(TYPE_FIGHTER, FACTION_REPUBLIC, 10 + idx * 2, 10 + idx * 2);
            
            idx = 0;
            for (int i = 0; i < scenario->cis_carriers; i++, idx++)
                ADD_UNIT(TYPE_CARRIER, FACTION_CIS, scenario->map_width - 8 - idx * 3, 
                         scenario->map_height - 8 - idx * 3);
            for (int i = 0; i < scenario->cis_destroyers; i++, idx++)
                ADD_UNIT(TYPE_DESTROYER, FACTION_CIS, scenario->map_width - 11 - idx * 3,
                         scenario->map_height - 11 - idx * 3);
            for (int i = 0; i < scenario->cis_fighters; i++, idx++)
                ADD_UNIT(TYPE_FIGHTER, FACTION_CIS, scenario->map_width - 13 - idx * 2,
                         scenario->map_height - 13 - idx * 2);
            break;
        }
        
        case PLACEMENT_RANDOM: {
            for (int i = 0; i < scenario->republic_carriers; i++)
                ADD_UNIT(TYPE_CARRIER, FACTION_REPUBLIC, rand() % (scenario->map_width / 2),
                         rand() % scenario->map_height);
            for (int i = 0; i < scenario->republic_destroyers; i++)
                ADD_UNIT(TYPE_DESTROYER, FACTION_REPUBLIC, rand() % (scenario->map_width / 2),
                         rand() % scenario->map_height);
            for (int i = 0; i < scenario->republic_fighters; i++)
                ADD_UNIT(TYPE_FIGHTER, FACTION_REPUBLIC, rand() % (scenario->map_width / 2),
                         rand() % scenario->map_height);
            
            for (int i = 0; i < scenario->cis_carriers; i++)
                ADD_UNIT(TYPE_CARRIER, FACTION_CIS, scenario->map_width / 2 + rand() % (scenario->map_width / 2),
                         rand() % scenario->map_height);
            for (int i = 0; i < scenario->cis_destroyers; i++)
                ADD_UNIT(TYPE_DESTROYER, FACTION_CIS, scenario->map_width / 2 + rand() % (scenario->map_width / 2),
                         rand() % scenario->map_height);
            for (int i = 0; i < scenario->cis_fighters; i++)
                ADD_UNIT(TYPE_FIGHTER, FACTION_CIS, scenario->map_width / 2 + rand() % (scenario->map_width / 2),
                         rand() % scenario->map_height);
            break;
        }
        
        case PLACEMENT_LINE: {
            int y_rep = scenario->map_height / 3;
            int y_cis = scenario->map_height * 2 / 3;
            int x = 10;
            
            for (int i = 0; i < scenario->republic_carriers; i++, x += 8)
                ADD_UNIT(TYPE_CARRIER, FACTION_REPUBLIC, x, y_rep);
            for (int i = 0; i < scenario->republic_destroyers; i++, x += 6)
                ADD_UNIT(TYPE_DESTROYER, FACTION_REPUBLIC, x, y_rep);
            for (int i = 0; i < scenario->republic_fighters; i++, x += 4)
                ADD_UNIT(TYPE_FIGHTER, FACTION_REPUBLIC, x, y_rep);
            
            x = 10;
            for (int i = 0; i < scenario->cis_carriers; i++, x += 8)
                ADD_UNIT(TYPE_CARRIER, FACTION_CIS, x, y_cis);
            for (int i = 0; i < scenario->cis_destroyers; i++, x += 6)
                ADD_UNIT(TYPE_DESTROYER, FACTION_CIS, x, y_cis);
            for (int i = 0; i < scenario->cis_fighters; i++, x += 4)
                ADD_UNIT(TYPE_FIGHTER, FACTION_CIS, x, y_cis);
            break;
        }
        
        default:
            /* Default to corners */
            scenario->placement_mode = PLACEMENT_CORNERS;
            scenario_generate_placements(scenario);
            return;
    }
    
    #undef ADD_UNIT
    scenario->unit_count = count;
}
