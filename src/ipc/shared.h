#ifndef SHARED_H
#define SHEARD_H

#include <stdint.h>
#include <sys/types.h>

#define N 40
#define M 80
#define MAX_UNITS 64

// unit_id strored in grid (0 = empty)
typedef int16_t unit_id_t;

typedef enum { FACTION_REPUBLIC=1, FACTION_CIS=2} faction_t;
typedef enum { TYPE_FLAGSHIP=1, TYPE_DESTROYER=2, TYPE_CARRIER=3, TYPE_FIGTER=4, TYPE_BOMBER=5, TYPE_ELITE=6} unit_type_t;

typedef struct {
    pid_t pid;              // mapping unit_id -> pid
    uint8_t faction;        // faction_t
    uint8_t type;           // unit_type_t
    uint8_t alive;          // 1/0
    uint16_t x,y;           // position on grid
    uint32_t flags;         // future: status,orders,etc.
    
}unit_entity_t;

typedef struct {
    uint32_t magic;         // for sanity checking
    uint32_t ticks;         // global tick_counter
    uint16_t next_unit_id;  // allocates IDs (starts at 1)
    uint16_t unit_count;    // active entitys

    // tick barrier data
    uint16_t tick_expected; // units step per tick
    uint16_t tick_done;     // units finished tick
    uint32_t last_step_tick[MAX_UNITS+1];   // tick per unit

    unit_id_t grid[N][M];   // stored unit_id only!
    unit_entity_t units[MAX_UNITS+1];   //index by unit_id, [0] unused
}shm_state_t;

#define SHM_MAGIC 0x53504143u   // 'SPAC'

// Semaphores
enum {
    SEM_GLOBAL_LOCK = 0,    // protects shm_state_t (grid+units+ticks)
    SEM_TICK_START,         // CC posts N permits per tick
    SEM_TICK_DONE,          // unit post done; CC waits N times
    SEM_COUNT
};

#endif