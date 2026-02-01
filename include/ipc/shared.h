#ifndef IPC_SHARED_H
#define IPC_SHARED_H

#include <stdint.h>
#include <sys/types.h>

/* Grid dimensions */
#define M 80
#define N 40

/* limits */
#define MAX_UNITS 64
#define MAX_WEAPONS 4
#define MAX_FIGHTERS_PER_BAY 6

/* unit_id stored in grid (0 = empty) */
typedef int16_t unit_id_t;
typedef int32_t st_points_t;


/* Simple enums stored as small integers in shared memory */
typedef enum { NONE = 0, LR_CANNON = 1, MR_CANNON = 2, SR_CANNON = 3, LR_GUN = 4, MR_GUN = 5, SR_GUN = 6 } weapon_type_t;
typedef enum { DO_NOTHING= 0, PATROL = 1, ATTACK = 2, MOVE = 3, MOVE_ATTACK = 4, GUARD = 5 } unit_order_t;
typedef enum { FACTION_NONE = 0, FACTION_REPUBLIC=1, FACTION_CIS=2 } faction_t;
typedef enum { DUMMY = 0, TYPE_FLAGSHIP=1, TYPE_DESTROYER=2, TYPE_CARRIER=3, TYPE_FIGHTER=4, TYPE_BOMBER=5, TYPE_ELITE=6 } unit_type_t;


/* point on a grid */
typedef struct {
    int16_t x;      // 
    int16_t y;      // 
} point_t;


/* Per-unit record stored in shared memory. */
typedef struct {
    pid_t pid;              // process id for this unit (for signaling)
    uint8_t faction;        // faction_t
    uint8_t type;           // unit_type_t
    uint8_t alive;          // 1 == alive, 0 == dead
    point_t position;       // position on grid (M x N)
    uint32_t flags;         // reserved for status / orders
    st_points_t dmg_payload;    // demage recived by unit
} unit_entity_t;


/* statistics of weapons*/
typedef struct {
    st_points_t dmg;            // demage per shoot
    st_points_t range;          // range
    unit_id_t w_target;     // target
    weapon_type_t type;     // type 
}weapon_stats_t;


/* structure necessery for assigning weapons stats for units*/
typedef struct {
    weapon_type_t types[MAX_WEAPONS];   // type of weapons unit has
    int n;                              // number of weapons unit has
} weapon_loadout_types_view_t;


/* list of batteries caried by unit */
typedef struct {
    weapon_stats_t arr[MAX_WEAPONS];    // weapons and their stats unit has
    uint8_t count;                      // number of weapons unit has
} weapon_loadout_view_t;


typedef struct {
    int16_t capacity;       // total capacity of fighter bay
    int16_t current;        // current number of fighters in bay
    unit_type_t  sq_types[MAX_FIGHTERS_PER_BAY]; // types of squadron in bay
} fighter_bay_view_t;

/* statistics of unit */
typedef struct {
    st_points_t hp;     // hit points 
    st_points_t sh;     // shields
    st_points_t en;     // energy (TBI)
    st_points_t sp;     // speed: radus of movment
    st_points_t si;     // unit size (TBI)
    st_points_t dr;     // detection radius: tiles
    weapon_loadout_view_t ba;  // list of batteries
    fighter_bay_view_t fb; // fighter bay stats (TBI)
} unit_stats_t;


/* Global shared state placed in SysV shared memory segment.
 * Indexing: units[0] is unused; valid unit IDs range 1..MAX_UNITS.
 */
typedef struct {
    uint32_t magic;         // magic for sanity checking
    uint32_t ticks;         // global tick counter incremented by CC
    uint16_t next_unit_id;  // allocator for new unit IDs (starts at 1)
    uint16_t unit_count;    // number of active units

    /* Tick barrier synchronization bookkeeping */
    uint16_t tick_expected;                     // how many units are expected this tick
    uint16_t tick_done;                         // how many units have finished this tick
    uint32_t last_step_tick[MAX_UNITS+1];       // per-unit last-tick performed

    unit_id_t grid[M][N];                       // grid of unit IDs (0 == empty)
    unit_entity_t units[MAX_UNITS+1];           // units indexed by unit_id (0 unused)
} shm_state_t;


#define SHM_MAGIC 0x53504143u   /* 'SPAC' */

/* Semaphore indices within the semaphore set.
 *  - SEM_GLOBAL_LOCK: mutex protecting the entire shm_state_t (grid + units + ticks).
 *  - SEM_TICK_START: CC posts N permits (one per alive unit) to allow units to run a tick.
 *  - SEM_TICK_DONE: each unit posts when finished; CC waits N times to collect them.
 */
enum {
    SEM_GLOBAL_LOCK = 0,
    SEM_TICK_START,
    SEM_TICK_DONE,
    SEM_COUNT
};

#endif