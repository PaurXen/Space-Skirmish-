# Command Center (CC) Module Documentation

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Components](#components)
4. [Core Processes](#core-processes)
5. [Game Logic](#game-logic)
6. [IPC Communication](#ipc-communication)
7. [Data Structures](#data-structures)
8. [API Reference](#api-reference)

---

## Overview

The **Command Center (CC)** module is the core orchestrator of the Space Skirmish simulation. It manages the lifecycle of all combat units, coordinates synchronization through a tick-based barrier system, and handles scenario configuration and execution.

### Key Responsibilities
- **IPC Management**: Create, initialize, and cleanup shared memory, semaphores, and message queues
- **Process Spawning**: Launch and manage battleship (heavy unit) and squadron (light unit) processes
- **Tick Synchronization**: Drive simulation forward with periodic ticks using barrier synchronization
- **Scenario Loading**: Parse and apply scenario configuration files
- **Signal Handling**: Coordinate graceful shutdown across all processes
- **Single Instance**: Ensure only one Command Center runs at a time

---

## Architecture

### Process Hierarchy

```
Command Center (CC)
├── Console Manager (CM) Thread
├── Battleship Processes (Type: FLAGSHIP, DESTROYER, CARRIER)
│   └── Squadron Underlings (via commander system)
└── Squadron Processes (Type: FIGHTER, BOMBER, ELITE)
```

### Synchronization Model

The CC implements a **tick-based barrier synchronization** pattern:

1. **Tick Start**: CC posts `SEM_TICK_START` semaphore once per alive unit
2. **Unit Processing**: Each unit waits for `SEM_TICK_START`, executes logic, posts `SEM_TICK_DONE`
3. **Tick Barrier**: CC waits for `SEM_TICK_DONE` from all alive units
4. **Repeat**: Cycle continues until shutdown signal

```
┌──────────────┐
│   CC tick    │
│   barrier    │
└──────┬───────┘
       │ SEM_TICK_START × N units
       ├────────────────────────────┐
       │                            │
  ┌────▼────┐                  ┌────▼────┐
  │ Unit 1  │                  │ Unit N  │
  │ Logic   │   ...            │ Logic   │
  └────┬────┘                  └────┬────┘
       │ SEM_TICK_DONE              │ SEM_TICK_DONE
       └────────────────────────────┤
                                    │
                        ┌───────────▼──────────┐
                        │ CC waits for all     │
                        │ SEM_TICK_DONE signals│
                        └──────────────────────┘
```

---

## Components

### 1. command_center.c

**Main coordinator process** - Orchestrates the entire simulation.

**Location**: `src/CC/command_center.c`

**Key Functions**:
- `main()`: Entry point, initialization, tick loop
- `check_single_instance()`: PID file locking to prevent multiple instances
- `spawn_unit()`: Fork and execute unit processes (battleship/squadron)
- `cm_thread()`: Console Manager command processing thread
- `handle_cm_command()`: Process commands (SPAWN, KILL, FREEZE, SPEED, GRID)
- `tick_barrier()`: Synchronize all units on each simulation tick
- `cleanup_cc()`: Shutdown: signal units, reap processes, cleanup IPC

**Responsibilities**:
- Create IPC resources (shared memory, semaphores, message queues)
- Load scenario configuration
- Spawn initial units based on scenario
- Drive tick barrier loop
- Handle console commands via CM thread
- Cleanup on shutdown

---

### 2. battleship.c

**Heavy unit process** - Represents large capital ships (Flagship, Destroyer, Carrier).

**Location**: `src/CC/battleship.c`

**Key Features**:
- **Commander System**: Can accept squadrons as underlings
- **Autonomous Combat**: Patrol, attack, and combat logic
- **Damage Processing**: Receive and process damage via message queues
- **Order Execution**: Support for PATROL, ATTACK, GUARD orders

**Main Loop**:
```c
while (!g_stop) {
    sem_wait(SEM_TICK_START);           // Wait for tick
    
    // 1. Process damage messages
    compute_dmg_payload(ctx, unit_id, &st);
    
    // 2. Detect nearby enemies
    count = unit_radar_scan(ctx, unit_id, &st, detect_id);
    
    // 3. Execute order logic (patrol/attack/guard)
    switch (order) {
        case PATROL: patrol_action(...); break;
        case ATTACK: attack_action(...); break;
        case GUARD:  guard_action(...);  break;
    }
    
    // 4. Move toward target
    unit_smart_move(ctx, unit_id, target_pri, &st);
    
    // 5. Combat (fire weapons)
    unit_try_combat(ctx, unit_id, &st, detect_id, count);
    
    sem_post(SEM_TICK_DONE);            // Signal tick complete
}
```

**Commander System**:
- Accepts commander requests via message queue
- Maintains array of underling squadron IDs
- Issues orders to underlings (PATROL, ATTACK, GUARD at specific coordinates)

---

### 3. squadron.c

**Light unit process** - Represents small fighters (Fighter, Bomber, Elite).

**Location**: `src/CC/squadron.c`

**Key Features**:
- **Commander Assignment**: Seeks and follows battleship commanders
- **Autonomous Behavior**: Independent patrol/combat when no commander
- **Order Following**: Execute PATROL, ATTACK, GUARD orders from commander
- **Damage Processing**: Same damage system as battleships

**Commander Assignment Logic**:
```c
// 1. Check if we have a living commander
if (commander != 0 && !is_alive(ctx, commander)) {
    commander = 0;  // Commander died, become autonomous
}

// 2. If no commander, try to find one
if (commander == 0) {
    commander_id = try_find_commander(ctx, unit_id);
    if (commander_id) {
        commander = commander_id;
    }
}

// 3. If we have commander, check for orders
if (commander != 0) {
    check_for_orders_from_commander(ctx, unit_id, &order, &target_pri);
}
```

---

### 4. unit_logic.c

**Combat and movement logic** - Pure game mechanics implementation.

**Location**: `src/CC/unit_logic.c`

**Key Systems**:

#### Damage Calculation
```c
float damage_multiplyer(unit_type_t attacker, unit_type_t target);
float accuracy_multiplier(weapon_type_t weapon, unit_type_t target);
st_points_t damage_to_target(unit_entity_t *attacker, 
                              unit_entity_t *target, 
                              weapon_stats_t *weapon, 
                              float accuracy);
```

**Type Effectiveness Matrix**:
| Attacker | Strong Against | Multiplier |
|----------|---------------|------------|
| Flagship | Carrier | 1.5× |
| Destroyer | Flagship, Destroyer, Carrier | 1.5× |
| Carrier | Fighter, Bomber, Elite | 1.5× |
| Fighter | Fighter, Bomber | 1.5× |
| Bomber | Flagship, Destroyer, Carrier | 3.0× |
| Elite | Fighter, Bomber, Elite | 2.0× |

**Accuracy Modifiers**:
- **Cannons** (LR/MR/SR): 75% vs large ships, 25% vs small ships
- **Guns** (LR/MR/SR): 0% vs large ships, 75% vs small ships

#### Radar & Detection
```c
int unit_radar_scan(ipc_ctx_t *ctx, unit_id_t unit_id, 
                    unit_stats_t *st, unit_id_t *detect_id);
```
- Scans grid within detection range
- Returns array of enemy unit IDs
- Used for target acquisition

#### Movement System
```c
int unit_smart_move(ipc_ctx_t *ctx, unit_id_t unit_id, 
                    point_t target, unit_stats_t *st);
```
- **A\* pathfinding** for obstacle avoidance
- Respects unit speed stat
- Handles multi-cell unit sizes
- Collision detection

#### Target Selection
```c
unit_id_t unit_chose_secondary_target(ipc_ctx_t *ctx, 
                                       unit_id_t *detect_id, 
                                       int count, 
                                       unit_id_t unit_id,
                                       point_t *pri_target, 
                                       int8_t *have_pri_target, 
                                       int8_t *have_sec_target);
```
- Prioritizes closest enemy
- Considers weapon range
- Filters by faction

#### Patrol Logic
```c
int8_t unit_chose_patrol_point(ipc_ctx_t *ctx, unit_id_t unit_id, 
                                point_t *target, unit_stats_t st);
```
- Generates random patrol destinations
- Respects grid boundaries
- Avoids obstacles

---

### 5. unit_ipc.c

**Unit-specific IPC operations** - Abstracts shared memory and message queue operations.

**Location**: `src/CC/unit_ipc.c`

**Key Functions**:

#### Grid Operations
```c
unit_id_t check_if_occupied(ipc_ctx_t *ctx, point_t point);
void unit_change_position(ipc_ctx_t *ctx, unit_id_t unit_id, point_t new_pos);
```
- Thread-safe grid access
- Multi-cell unit placement/removal
- Position updates in shared memory

#### Damage System
```c
void unit_add_to_dmg_payload(ipc_ctx_t *ctx, unit_id_t target_id, 
                              st_points_t dmg);
void compute_dmg_payload(ipc_ctx_t *ctx, unit_id_t unit_id, 
                          unit_stats_t *st);
```
- **Asynchronous damage**: Attacker sends damage message to target's message queue
- **Signal notification**: `SIGRTMAX` signals target that damage is pending
- **Batch processing**: Target processes all pending damage in one tick
- Handles shields and HP reduction

#### Combat
```c
void unit_try_combat(ipc_ctx_t *ctx, unit_id_t unit_id, 
                     unit_stats_t *st, unit_id_t *detect_id, int count);
```
- Iterates through all weapons
- Checks range to each detected enemy
- Calculates damage and sends to targets
- Logs combat events

#### State Marking
```c
void mark_dead(ipc_ctx_t *ctx, unit_id_t unit_id);
```
- Sets HP to 0
- Marks grid cells as empty
- Allows process cleanup

---

### 6. scenario.c

**Scenario configuration loader** - Parses `.conf` files to configure simulation.

**Location**: `src/CC/scenario.c`

**Configuration Format**:
```ini
[scenario]
name = Fleet Battle

[map]
width = 100
height = 100

[units]
# type faction x y
CARRIER REPUBLIC 10 10
DESTROYER CIS 90 90
FIGHTER REPUBLIC 15 15

[obstacles]
# x y
50 50
51 50
52 50

[autogenerate]
placement_mode = CORNERS
republic_carriers = 1
republic_destroyers = 2
cis_fighters = 5
```

**Placement Modes**:
- `CORNERS`: Units spawn at map corners
- `EDGES`: Units spawn along map edges
- `RANDOM`: Random positions
- `LINE`: Linear formation
- `SCATTERED`: Distributed across map
- `MANUAL`: Explicit coordinates in `[units]` section

**API**:
```c
int scenario_load(const char *filename, scenario_t *out);
void scenario_default(scenario_t *out);
void scenario_generate_placements(scenario_t *scenario);
```

---

### 7. unit_stats.c & unit_size.c

**Unit statistics database** - Defines characteristics of each unit type.

**Location**: `src/CC/unit_stats.c`, `src/CC/unit_size.c`

**Stats Structure**:
```c
typedef struct {
    st_points_t hp;     // Hit points
    st_points_t sh;     // Shields
    st_points_t en;     // Energy
    st_points_t sp;     // Speed
    st_points_t si;     // Size (grid cells)
    st_points_t dr;     // Detection range
    weapon_loadout_view_t ba;  // Weapon battery
} unit_stats_t;
```

**Example Stats** (Flagship):
- HP: 1000
- Shields: 500
- Speed: 1
- Size: 3×3 cells
- Detection Range: 30
- Weapons: 2× Long-Range Cannons

---

### 8. weapon_stats.c

**Weapon statistics database** - Defines weapon characteristics.

**Location**: `src/CC/weapon_stats.c`

**Stats Structure**:
```c
typedef struct {
    st_points_t dmg;      // Base damage
    st_points_t range;    // Maximum range
    weapon_type_t type;   // CANNON or GUN
    unit_type_t w_target; // Preferred target (large/small)
} weapon_stats_t;
```

**Weapon Types**:
- **Long-Range Cannon**: 100 dmg, range 30
- **Medium-Range Cannon**: 150 dmg, range 20
- **Short-Range Cannon**: 200 dmg, range 10
- **Long-Range Gun**: 50 dmg, range 25
- **Medium-Range Gun**: 75 dmg, range 15
- **Short-Range Gun**: 100 dmg, range 5

---

## Core Processes

### Tick Synchronization Algorithm

**File**: `src/CC/command_center.c`

```c
void tick_barrier(ipc_ctx_t *ctx) {
    int alive_count = 0;
    
    // Count alive units
    for (int i = 1; i <= MAX_UNITS; i++) {
        if (ctx->S->units[i].pid > 0 && 
            ctx->S->units[i].hp > 0) {
            alive_count++;
        }
    }
    
    // Signal each unit to start
    for (int i = 0; i < alive_count; i++) {
        sem_post(ctx->sem_id, SEM_TICK_START);
    }
    
    // Wait for all units to complete
    for (int i = 0; i < alive_count; i++) {
        sem_wait(ctx->sem_id, SEM_TICK_DONE);
    }
}
```

### Unit Spawn Process

**File**: `src/CC/command_center.c`

```c
pid_t spawn_unit(unit_type_t type, faction_t faction, 
                 point_t pos, unit_id_t unit_id) {
    pid_t pid = fork();
    
    if (pid == 0) {  // Child process
        char *binary = is_battleship(type) ? 
                       "./battleship" : "./squadron";
        
        char arg_type[16], arg_faction[16], 
             arg_x[16], arg_y[16], arg_id[16];
        
        snprintf(arg_type, sizeof(arg_type), "%d", type);
        snprintf(arg_faction, sizeof(arg_faction), "%d", faction);
        snprintf(arg_x, sizeof(arg_x), "%d", pos.x);
        snprintf(arg_y, sizeof(arg_y), "%d", pos.y);
        snprintf(arg_id, sizeof(arg_id), "%d", unit_id);
        
        execl(binary, binary, arg_type, arg_faction, 
              arg_x, arg_y, arg_id, NULL);
        
        perror("execl failed");
        exit(1);
    }
    
    return pid;  // Parent returns child PID
}
```

### Shutdown Sequence

**File**: `src/CC/command_center.c`

```c
void cleanup_cc(ipc_ctx_t *ctx, pid_t *unit_pids, int unit_count) {
    // 1. Signal all units to stop
    for (int i = 0; i < unit_count; i++) {
        if (unit_pids[i] > 0) {
            kill(unit_pids[i], SIGTERM);
        }
    }
    
    // 2. Wait for processes to terminate (with timeout)
    for (int i = 0; i < unit_count; i++) {
        if (unit_pids[i] > 0) {
            waitpid(unit_pids[i], NULL, 0);
        }
    }
    
    // 3. Cleanup IPC resources
    ipc_cleanup(ctx);
    
    // 4. Close log directory
    log_close();
}
```

---

## Game Logic

### Combat System

**Weapon Firing Decision**:
1. **Detect enemies** within detection range via radar scan
2. **For each weapon**:
   - Check if enemy is in weapon range
   - Calculate accuracy modifier
   - Roll to hit
   - Calculate damage with type multipliers
   - Send damage message to target
3. **Target processes damage** on next tick

**Damage Processing**:
```c
// 1. Reduce shields first
if (st->sh >= total_damage) {
    st->sh -= total_damage;
    total_damage = 0;
} else {
    total_damage -= st->sh;
    st->sh = 0;
}

// 2. Apply remaining damage to HP
st->hp -= total_damage;
if (st->hp <= 0) {
    mark_dead(ctx, unit_id);
}
```

### Movement & Pathfinding

**A\* Pathfinding** (unit_logic.c):
- **Cost function**: Euclidean distance to goal
- **Obstacle avoidance**: Check grid for occupied cells
- **Multi-cell units**: Validate all cells in unit's footprint
- **Speed limiting**: Move at most `speed` cells per tick

### Order System

**Supported Orders**:
1. **PATROL**: 
   - Choose random patrol points
   - Engage enemies opportunistically
   - Return to patrol when enemies defeated

2. **ATTACK**:
   - Move toward specific target
   - Engage enemies en route
   - Pursue target until destroyed

3. **GUARD**:
   - Hold position at specified coordinates
   - Engage enemies within range
   - Do not pursue

**Commander → Squadron Communication**:
```c
// Battleship sends order to squadron
mq_order_t order_msg = {
    .mtype = squadron_pid,
    .order = ATTACK,
    .target_x = enemy_x,
    .target_y = enemy_y
};
mq_send_order(ctx->q_req, &order_msg);
```

---

## IPC Communication

### Message Queue Protocol

**Damage Messages** (`q_req`):
```c
typedef struct {
    long mtype;           // Target unit PID
    unit_id_t target_id;  // Target unit ID
    st_points_t damage;   // Damage amount
} mq_damage_t;
```

**Commander Request** (`q_req`):
```c
typedef struct {
    long mtype;           // Battleship PID
    pid_t sender;         // Squadron PID
    unit_id_t sender_id;  // Squadron unit ID
    uint32_t req_id;      // Request identifier
} mq_commander_req_t;
```

**Commander Reply** (`q_rep`):
```c
typedef struct {
    long mtype;           // Squadron PID
    uint32_t req_id;      // Matching request ID
    int status;           // 0 = accepted, -1 = rejected
    unit_id_t commander_id; // Battleship unit ID
} mq_commander_rep_t;
```

**Order Messages** (`q_order`):
```c
typedef struct {
    long mtype;           // Squadron PID
    unit_order_t order;   // PATROL, ATTACK, GUARD
    int16_t target_x;     // Target X coordinate
    int16_t target_y;     // Target Y coordinate
} mq_order_t;
```

### Semaphore Usage

| Semaphore | Purpose |
|-----------|---------|
| `SEM_GLOBAL_LOCK` | Protect shared memory writes |
| `SEM_TICK_START` | Signal units to begin tick |
| `SEM_TICK_DONE` | Signal tick completion to CC |
| `SEM_UI_LOCK` | Protect UI updates |

### Shared Memory Layout

**File**: `include/ipc/shared.h`

```c
typedef struct {
    unit_id_t grid[M][N];        // Grid state
    unit_entity_t units[MAX_UNITS]; // Unit states
    int tick_count;              // Global tick counter
    // ... other global state
} shared_state_t;
```

---

## Data Structures

### unit_entity_t

```c
typedef struct {
    pid_t pid;              // Process ID
    unit_type_t type;       // FLAGSHIP, DESTROYER, etc.
    faction_t faction;      // REPUBLIC or CIS
    point_t position;       // Grid coordinates (center)
    st_points_t hp;         // Current hit points
    st_points_t sh;         // Current shields
    st_points_t en;         // Current energy
    // ... other runtime stats
} unit_entity_t;
```

### scenario_t

```c
typedef struct {
    char name[MAX_SCENARIO_NAME];
    int map_width;
    int map_height;
    obstacle_t obstacles[MAX_OBSTACLES];
    int obstacle_count;
    unit_placement_t units[MAX_INITIAL_UNITS];
    int unit_count;
    placement_mode_t placement_mode;
    // ... auto-generation counts
} scenario_t;
```

---

## API Reference

### Command Center API

#### Initialization
```c
ipc_ctx_t* ipc_create(const char *ftok_path);
int ipc_attach(ipc_ctx_t *ctx, const char *ftok_path);
void ipc_detach(ipc_ctx_t *ctx);
void ipc_cleanup(ipc_ctx_t *ctx);
```

#### Unit Spawning
```c
pid_t spawn_unit(unit_type_t type, faction_t faction, 
                 point_t pos, unit_id_t unit_id);
```

#### Tick Synchronization
```c
void tick_barrier(ipc_ctx_t *ctx);
```

### Unit Logic API

#### Combat
```c
float damage_multiplyer(unit_type_t unit, unit_type_t target);
float accuracy_multiplier(weapon_type_t weapon, unit_type_t target);
st_points_t damage_to_target(unit_entity_t *attacker, 
                              unit_entity_t *target,
                              weapon_stats_t *weapon, 
                              float accuracy);
void unit_try_combat(ipc_ctx_t *ctx, unit_id_t unit_id,
                     unit_stats_t *st, unit_id_t *detect_id, 
                     int count);
```

#### Movement
```c
int unit_smart_move(ipc_ctx_t *ctx, unit_id_t unit_id,
                    point_t target, unit_stats_t *st);
```

#### Radar
```c
int unit_radar_scan(ipc_ctx_t *ctx, unit_id_t unit_id,
                    unit_stats_t *st, unit_id_t *detect_id);
```

#### Target Selection
```c
unit_id_t unit_chose_secondary_target(ipc_ctx_t *ctx,
                                       unit_id_t *detect_id,
                                       int count, unit_id_t unit_id,
                                       point_t *pri_target,
                                       int8_t *have_pri_target,
                                       int8_t *have_sec_target);
int8_t unit_chose_patrol_point(ipc_ctx_t *ctx, unit_id_t unit_id,
                                point_t *target, unit_stats_t st);
```

### Unit IPC API

#### Grid Operations
```c
unit_id_t check_if_occupied(ipc_ctx_t *ctx, point_t point);
void unit_change_position(ipc_ctx_t *ctx, unit_id_t unit_id, 
                          point_t new_pos);
point_t get_target_position(ipc_ctx_t *ctx, unit_id_t attacker_id,
                            unit_id_t target_id);
```

#### Damage System
```c
void unit_add_to_dmg_payload(ipc_ctx_t *ctx, unit_id_t target_id,
                              st_points_t dmg);
void compute_dmg_payload(ipc_ctx_t *ctx, unit_id_t unit_id,
                         unit_stats_t *st);
```

#### State Management
```c
void mark_dead(ipc_ctx_t *ctx, unit_id_t unit_id);
int is_alive(ipc_ctx_t *ctx, unit_id_t unit_id);
```

### Scenario API

```c
int scenario_load(const char *filename, scenario_t *out);
void scenario_default(scenario_t *out);
void scenario_generate_placements(scenario_t *scenario);
```

### Stats API

```c
unit_stats_t unit_stats_for_type(unit_type_t type);
weapon_stats_t weapon_stats_for_weapon_type(weapon_type_t type);
weapon_loadout_view_t weapon_loadout_for_unit_type(unit_type_t type);
```

---

## File Organization

```
src/CC/
├── command_center.c      # Main orchestrator
├── battleship.c          # Heavy unit process
├── squadron.c            # Light unit process
├── unit_logic.c          # Combat & movement logic
├── unit_ipc.c            # IPC abstractions
├── unit_stats.c          # Unit statistics
├── unit_size.c           # Size calculations
├── weapon_stats.c        # Weapon statistics
├── scenario.c            # Scenario loader
├── flagship.c            # Flagship-specific logic (if exists)
└── terminal_tee.c        # Terminal output redirection

include/CC/
├── scenario.h            # Scenario structures
├── terminal_tee.h        # Terminal tee interface
├── unit_ipc.h            # IPC function declarations
├── unit_logic.h          # Logic function declarations
├── unit_size.h           # Size function declarations
├── unit_stats.h          # Stats function declarations
└── weapon_stats.h        # Weapon function declarations
```

---

## Build Targets

From project `Makefile`:

```makefile
# Build all binaries
make all

# Build individual components
make command_center
make console_manager
make ui
make battleship
make squadron
```

---

## Usage Examples

### Starting the Simulation

```bash
# Start Command Center with default scenario
./command_center

# Start Command Center with custom scenario
./command_center --scenario fleet_battle

# Start User Interface in another terminal
./ui

# Start Console Manager in another terminal
./console_manager
```

### Console Commands (via Console Manager)

```
spawn <type> <faction> <x> <y>    # Spawn new unit
kill <unit_id>                     # Destroy specific unit
freeze                             # Pause simulation
unfreeze                           # Resume simulation
speed <ms>                         # Set tick speed (milliseconds)
grid on|off                        # Toggle grid display
quit                               # Shutdown simulation
```

---

## Error Handling

All CC components use the centralized error handling system:

```c
#include "error_handler.h"

// Non-fatal error checking
pid_t pid = CHECK_SYS_CALL_NONFATAL(fork(), "spawn_unit:fork");
if (pid == -1) {
    // Handle error
    return -1;
}

// Fatal error (terminates process)
int shm_id = CHECK_SYS_CALL(shmget(...), "ipc_create:shmget");
```

---

## Logging

Each process logs to dedicated log files:

```
logs/run_<timestamp>_pid<pid>/
├── command_center.log
├── battleship_<id>.log
├── squadron_<id>.log
└── ...
```

**Log Levels**:
- `LOGD()`: Debug
- `LOGI()`: Info
- `LOGW()`: Warning
- `LOGE()`: Error

---

## Future Enhancements

1. **Formations**: Squadron formations (wedge, line, box)
2. **Abilities**: Special unit abilities (cloak, shield boost)
3. **Resources**: Energy management and recharge
4. **Diplomacy**: Dynamic faction alliances
5. **Objectives**: Victory conditions and mission goals

---

## See Also

- [IPC Module Documentation](IPC_MODULE.md)
- [UI Module Documentation](UI_MODULE.md)
- [Console Manager Documentation](CM_MODULE.md)
- [Project README](../README.md)
- [Scenarios README](../scenarios/README.md)
