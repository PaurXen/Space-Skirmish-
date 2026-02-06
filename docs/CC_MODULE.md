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
- `main()`: Entry point, initialization, tick loop [\<link\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c?plain=1#L503)
- `check_single_instance()`: PID file locking to prevent multiple instances [\<link\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c?plain=1#L68)
- `spawn_unit()`: Fork and execute unit processes (battleship/squadron) [\<link\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c?plain=1#L194)
- `cm_thread()`: Console Manager command processing thread [\<link\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c?plain=1#L474)
- `handle_cm_command()`: Process commands (SPAWN, KILL, FREEZE, SPEED, GRID) [\<link\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c?plain=1#L362)
- `tick_barrier()`: Synchronize all units on each simulation tick [\<link\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c?plain=1#L800-L821)
- `cleanup_cc()`: Shutdown: signal units, reap processes, cleanup IPC [\<link\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c?plain=1#L863-L955)

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
[\<SQ accept\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/battleship.c?plain=1#L126-L158)
- **Autonomous Combat**: Patrol, attack, and combat logic
[\<Combat logic\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/battleship.c?plain=1#L117-L224)
- **Damage Processing**: Receive and process damage via message queues
[\<Demage processing\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/battleship.c?plain=1#L409-L414)
- **Order Execution**: Support for PATROL, ATTACK, GUARD orders
[\<Order processing\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/battleship.c?plain=1#L180-L196)

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
[\<Link to battleship game loop\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/battleship.c?plain=1#L382-L518)

**Commander System**:
- Accepts commander requests via message queue from squadrons
- Maintains `underlings[]` array of squadron unit IDs
- Issues orders to underlings based on current target:\
  - No target → squadrons GUARD the battleship
  - Target is FIGHTER/ELITE → fighters/elites ATTACK, bombers GUARD
  - Target is BS/FS/CARRIER → bombers ATTACK, others GUARD bombers
- Spawns squadrons from fighter bay when capacity allows\
[\<commander request handling\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/battleship.c?plain=1#L126-L158)\
[\<order sending logic\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/battleship.c?plain=1#L226-L286)\
[\<main game loop\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/battleship.c?plain=1#L382-L518)

---

### 3. squadron.c

**Light unit process** - Represents small fighters (Fighter, Bomber, Elite).

**Location**: `src/CC/squadron.c`

**Key Features**:
- **Commander Assignment**: Seeks and follows battleship commanders
- **Autonomous Behavior**: Independent patrol/combat when no commander
- **Order Following**: Execute
[\<patrol_action\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/squadron.c?plain=1#L58-L95),
[\<attack_action\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/squadron.c?plain=1#L97-L115),
[\<guard_action\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/squadron.c?plain=1#L117-L205) orders from commander
- **Damage Processing**: Same damage system as battleships\
[\<damage processing\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/squadron.c?plain=1#L459-L464)

**Commander Assignment Logic**:
```c
// 1. Check for commander assignment replies
while (mq_try_recv_commander_reply(ctx->q_rep, &cmd_rep) == 1) {
    if (cmd_rep.status == 0) {
        commander = cmd_rep.commander_id;
    }
}

// 2. Check for orders from commander
while (mq_try_recv_order(ctx->q_req, &order_msg) == 1) {
    order = order_msg.order;
    if (order == ATTACK) target_sec = order_msg.target_id;
    else if (order == GUARD) target_ter = order_msg.target_id;
}

// 3. If no commander or commander died, find new one
if (!commander || !ctx->S->units[commander].alive) {
    // Scan for nearby battleships and send commander request
    mq_commander_req_t req = { .mtype = MSG_COMMANDER_REQ, ... };
    mq_send_commander_req(ctx->q_req, &req);
}
```
[\<squadrone_action (main logic)\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/squadron.c?plain=1#L208-L346)\
[\<commander request sending\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/squadron.c?plain=1#L256-L277)\
[\<main game loop\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/squadron.c?plain=1#L432-L520)

---

### 4. unit_logic.c

**Combat and movement logic** - Pure game mechanics implementation.

**Location**: `src/CC/unit_logic.c`

**Key Systems**:

#### Damage Calculation
```c
float damage_multiplier(unit_type_t attacker, unit_type_t target);
float accuracy_multiplier(weapon_type_t weapon, unit_type_t target);
st_points_t damage_to_target(unit_entity_t *attacker, 
                              unit_entity_t *target, 
                              weapon_stats_t *weapon, 
                              float accuracy);
```
[\<damage_multiplier\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L16-L39)\
[\<accuracy_multiplier\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L41-L55)\
[\<damage_to_target\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L57-L67)

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
int unit_radar(unit_id_t unit_id, unit_stats_t u_st,
               unit_entity_t *units, unit_id_t *out,
               faction_t faction);
```
- Scans all units within detection range (`u_st.dr`)
- Filters by faction (ignores allies if faction != FACTION_NONE)
- Returns count of detected enemy unit IDs
- Avoids duplicates from multi-cell units\
[\<unit_radar\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L706-L735)

#### Movement System
```c
void unit_move(ipc_ctx_t *ctx, unit_id_t unit_id, point_t from,
               point_t *target_pri, unit_stats_t *st, int aproach);
```
- **BFS pathfinding** within SP (speed) disk for obstacle avoidance
- Respects unit speed and detection range stats
- Handles multi-cell unit sizes via `can_fit_at_position()`
- Collision detection with grid occupancy check

**Movement Pipeline**:
1. `unit_compute_goal_for_tick_dr()` - Choose goal within detection range closest to target
2. `unit_next_step_towards_dr()` - Find best reachable position within speed disk
3. `unit_change_position()` - Update grid and unit position

[\<unit_move\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_ipc.c?plain=1#L229-L245)\
[\<unit_compute_goal_for_tick_dr\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L565-L620)\
[\<unit_next_step_towards_dr\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L649-L701)

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
Prioritizes enemy with highest damage multiplier. When secondary target is chosen, primary target is set to secondary target's closest cell.\
[\<unit_chose_secondary_target\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_ipc.c?plain=1#L169-L203)\
[\<unit_chose_patrol_point\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_ipc.c?plain=1#L205-L227)\
[\<unit_calculate_aproach\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L737-L746)

### Unit Logic

Each unit has three target variables:
- primary target - point which the unit approaches and move tworwards
- secondary target - id of the enemy unit which is to be attacked
- tertiary target (TBA to BS) - id fo the unit that is to be protected
These three targets are being assigned based on the order that is being executed. But theres an order in which each operation is executed:
1. Detect units
2. Compute commander and order (TBA to BS)
3. Action based on order (SQ - PATROL, GUARD, ATTACK | BS - PATROL)
4. Unit move
5. Detect units
6. Shoot (logic)

PATROL order is the default one, so it provides autonomy to the unit. The rest of them come with an already assigned target.

#### Patrol Logic

Each unit has its own patrol logic:\
[\<battleship patrol_action\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/battleship.c?plain=1#L78-L115)\
[\<squadron patrol_action\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/squadron.c?plain=1#L58-L95)\
Mainly, it comes out to this:
- Try to choose secondary and primary targets via `unit_chose_secondary_target()`
- If it has a primary target within approach range, clear it
- If it doesn't have a primary target, choose a random point on the DR border via `unit_chose_patrol_point()`

#### Attack Logic

Sets the unit's primary and secondary targets to the given target.\
[\<squadron attack_action\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/squadron.c?plain=1#L97-L115)

#### Guard Logic

Sets unit's tertiary target to the ID of the guarded unit.
Primary target is set to a position on a circle around guarded unit (radius = guarded unit's speed + size - 1).
Uses guarded unit's DR for enemy detection. If an enemy enters that DR, it is attacked. If the target leaves the guarded unit's DR, the protecting unit returns to its orbit position.\
[\<squadron guard_action\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/squadron.c?plain=1#L117-L205)

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
- Position updates in shared memory\
[\<check if occupied\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_ipc.c?plain=1#L8-L16)\
[\<unit change position\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_ipc.c?plain=1#L18-L34)

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
- Handles shields and HP reduction\
[\<add to dmg payload\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_ipc.c?plain=1#L47-L65)\
[\<compute dmg payload\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_ipc.c?plain=1#L67-L96)

#### Combat
```c
st_points_t unit_weapon_shoot(ipc_ctx_t *ctx,
                                        unit_id_t unit_id,
                                        unit_stats_t *st,
                                        unit_id_t target_sec,
                                        int count,
                                        unit_id_t *detect_id,
                                        st_points_t *out_dmg
)
```
- Iterates through all weapons
- Checks the range to each detected enemy
- Calculates damage and sends it to targets
- Logs combat events\
[\<weapon shoot\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_ipc.c?plain=1#L98-L167)

#### State Marking
```c
void mark_dead(ipc_ctx_t *ctx, unit_id_t unit_id);
```
- Sets `alive` flag to 0
- Removes unit from all grid cells it occupies (multi-cell aware)
- Uses `remove_unit_from_grid()` for proper cleanup\
[\<mark_dead\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_ipc.c?plain=1#L247-L263)

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
[\<scenario load\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/scenario.c?plain=1#L38-L143)\
[\<scenario default\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/scenario.c?plain=1#L10-L36)\
[\<scenario generate placements\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/scenario.c?plain=1#L145-L239)

---

### 7. unit_stats.c

**Unit statistics database** - Defines characteristics of each unit type.

**Location**: `src/CC/unit_stats.c`

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

**Actual Stats from Code**:

| Unit | HP | SH | EN | SP | SI | DR | Weapons |
|------|-----|-----|-----|-----|-----|-----|----------|
| Flagship | 200 | 100 | -1 | 2 | 3 | M(120) | 2×LR_CANNON, 2×MR_GUN |
| Destroyer | 100 | 100 | -1 | 3 | 2 | 20 | 2×LR_CANNON, 1×MR_GUN |
| Carrier | 100 | 100 | -1 | 6 | 2 | 20 | 1×LR_CANNON, 2×MR_GUN |
| Fighter | 20 | 0 | 20 | 5 | 1 | 10 | 1×SR_GUN |
| Bomber | 30 | 0 | 20 | 4 | 1 | 15 | 1×SR_CANNON |
| Elite | 20 | 20 | 20 | 6 | 1 | 15 | 1×SR_GUN |

[\<unit_stats_for_type\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_stats.c?plain=1#L17-L26)\
[\<k_fighter_types (fighter bay)\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_stats.c?plain=1#L5-L15)

---

### 8. unit_size.c

**Multi-cell unit placement system** - Handles grid occupancy for units of different sizes.

**Location**: `src/CC/unit_size.c`

**Key Concept**: Units occupy multiple grid cells based on their size stat. Each size uses a **diamond-shaped pattern** centered on the unit's position:

**Size Patterns**:
```
Size 1 (1 cell):     Size 2 (5 cells):       Size 3 (13 cells):
                           ·                        ·
      ■                  · ■ ·                    · ■ ·
                           ·                    · ■ ■ ■ ·
                                                  · ■ ·
                                                    ·
```

**Pattern Structure**:
```c
typedef struct {
    int8_t count;                   // number of cells occupied
    point_t cells[MAX_SIZE_CELLS];  // relative offsets from center
} size_pattern_t;
```

**Key Functions**:

| Function | Purpose |
|----------|---------|
| `get_size_pattern(size)` | Returns hardcoded cell pattern for size (1, 2, or 3) |
| `can_fit_at_position(ctx, center, size, ignore_unit)` | Check if all cells are empty (for spawning/movement) |
| `get_occupied_cells(center, size, out_cells, out_count)` | Get list of all grid positions a unit occupies |
| `get_closest_cell_to_attacker(attacker_pos, target_center, target_size)` | Find nearest cell of multi-cell target for range calculation |
| `place_unit_on_grid(ctx, unit_id, center, size)` | Mark all cells as occupied by unit_id |
| `remove_unit_from_grid(ctx, unit_id, center, size)` | Clear all cells occupied by unit |

**Usage in Combat**:
```c
// Calculate range to closest cell of target (not center)
point_t closest = get_closest_cell_to_attacker(attacker_pos, target_center, target_size);
int32_t dx = closest.x - attacker_pos.x;
int32_t dy = closest.y - attacker_pos.y;
int32_t range = sqrt(dx*dx + dy*dy);
```

**Usage in Movement**:
```c
// Check if unit can move to new position
if (can_fit_at_position(ctx, new_pos, unit_size, unit_id)) {
    remove_unit_from_grid(ctx, unit_id, old_pos, unit_size);
    place_unit_on_grid(ctx, unit_id, new_pos, unit_size);
}
```

[\<get_size_pattern\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_size.c?plain=1#L35-L42)\
[\<can_fit_at_position\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_size.c?plain=1#L44-L64)\
[\<place_unit_on_grid\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_size.c?plain=1#L107-L119)\
[\<remove_unit_from_grid\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_size.c?plain=1#L121-L136)


---

### 9. weapon_stats.c

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

**Actual Weapon Stats from Code**:

| Weapon | Damage | Range |
|--------|--------|-------|
| LR_CANNON | 10 | 15 |
| MR_CANNON | 10 | 10 |
| SR_CANNON | 10 | 5 |
| LR_GUN | 10 | 15 |
| MR_GUN | 10 | 10 |
| SR_GUN | 10 | 5 |

[\<weapon_stats_for_weapon_type\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/weapon_stats.c?plain=1#L7-L17)\
[\<weapon_loadout_for_unit_type\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/weapon_stats.c?plain=1#L61-L74)\
[\<k_loadout_types (per unit)\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/weapon_stats.c?plain=1#L21-L42)

---

## Core Processes

### Tick Synchronization Algorithm

**File**: `src/CC/command_center.c`

```c
for (unsigned i=0; i<alive; i++) {
    if (sem_post_retry(ctx.sem_id, SEM_TICK_START, +1) == -1) {
        LOGE("[CC] sem_post_retry(TICK_START) failed: %s", strerror(errno));
        perror("sem_post_retry(TICK_START)");
        g_stop = 1;
        break;
    }
}

for (unsigned i=0; i<alive; i++) {
    if (sem_wait_intr(ctx.sem_id, SEM_TICK_DONE, -1, &g_stop) == -1) {
        if (g_stop) {
            LOGW("[CC] sem_wait_intr interrupted by stop signal");
        } else {
            LOGE("[CC] sem_wait_intr failed: %s", strerror(errno));
        }
        break;
    }
}
```
[\<tick barrier\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c?plain=1#L800-L821)

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
[\<unit spawn\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c?plain=1#L190-L231)

### Shutdown Sequence

**File**: `src/CC/command_center.c`

```c
int status;
pid_t pid;
int waited = 0;
int timeout_count = 0;
const int MAX_WAIT_ATTEMPTS = 100; /* Prevent infinite loop */

for (;;) {
    pid = waitpid(-1, &status, 0);   // BLOCK until a child exits
    if (pid > 0) {
        waited++;
        if (WIFEXITED(status)) {
            LOGD("[CC] reaped child %d, exit status %d", pid, WEXITSTATUS(status));
            printf("[CC] reaped child %d, exit status %d\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            LOGD("[CC] reaped child %d, killed by signal %d", pid, WTERMSIG(status));
            printf("[CC] reaped child %d, killed by signal %d\n", pid, WTERMSIG(status));
        } else {
            printf("[CC] reaped child %d\n", pid);
        }
        continue;
}

    if (pid == -1) {
        if (errno == EINTR) {
            timeout_count++;
            if (timeout_count > MAX_WAIT_ATTEMPTS) {
                LOGW("[CC] waitpid interrupted too many times, giving up");
                break;
            }
            continue;               // interrupted by signal -> retry
        }
        if (errno == ECHILD) {
            LOGD("[CC] no more children to reap");
            break;                  // no more children
        }
        LOGE("[CC] waitpid failed: %s", strerror(errno));
        perror("[CC] waitpid");
        break;
    }
}
```
[\<cleanup\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c?plain=1#L895-L934)

---

## Game Logic

### Combat System

**Weapon Firing Decision**:
1. **Detect enemies** within detection range via `unit_radar()`
2. **For each weapon**:
   - Check if enemy is in weapon range via `in_disk_i()`
   - Calculate accuracy modifier via `accuracy_multiplier()`
   - Roll to hit (random vs accuracy)
   - Calculate damage with type multipliers via `damage_multiplier()`
   - Send damage message to target via `unit_add_to_dmg_payload()`
3. **Target processes damage** on next tick via `compute_dmg_payload()`

**Key Functions**:\
[\<damage_multiplier\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L16-L39)\
[\<accuracy_multiplier\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L41-L55)\
[\<damage_to_target\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L57-L67)\
[\<unit_weapon_shoot\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_ipc.c?plain=1#L98-L167)


### Movement & Pathfinding

**BFS Pathfinding** (unit_logic.c):
- **Cost function**: Euclidean distance to goal
- **Obstacle avoidance**: Check grid for occupied cells via `can_fit_at_position()`
- **Multi-cell units**: Validate all cells in unit's footprint before movement
- **Speed limiting**: Move at most `sp` (speed) cells per tick
- **Detection range planning**: Goal chosen within DR, then step chosen within SP

**Key Functions**:\
[\<bfs_best_reachable_in_sp_disk_prefer_border\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L450-L562)\
[\<unit_compute_goal_for_tick_dr\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L565-L620)\
[\<unit_next_step_towards_dr\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L649-L701)

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
   - Hold position near specified unit
   - Engage enemies within guarded unit range
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
[\<mq_damage_t definition\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/ipc_mesq.h?plain=1#L55-L57)

**Commander Request** (`q_req`):
```c
typedef struct {
    long mtype;           // MSG_COMMANDER_REQ
    pid_t sender;         // Squadron PID
    unit_id_t sender_id;  // Squadron unit ID
    uint32_t req_id;      // Request identifier
} mq_commander_req_t;
```
[\<mq_commander_req_t definition\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/ipc_mesq.h?plain=1#L41-L46)

**Commander Reply** (`q_rep`):
```c
typedef struct {
    long mtype;           // Squadron PID (for filtering)
    uint32_t req_id;      // Matching request ID
    int16_t status;       // 0 = accepted, <0 = rejected
    unit_id_t commander_id; // Battleship unit ID on success
} mq_commander_rep_t;
```
[\<mq_commander_rep_t definition\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/ipc_mesq.h?plain=1#L48-L53)

**Order Messages** (`q_req`):
```c
typedef struct {
    long mtype;           // Target squadron PID
    unit_order_t order;   // PATROL, ATTACK, GUARD
    unit_id_t target_id;  // Target unit ID for ATTACK/GUARD
} mq_order_t;
```
[\<mq_order_t definition\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/ipc_mesq.h?plain=1#L61-L65)

### Semaphore Usage

| Index | Semaphore | Purpose |
|-------|-----------|----------------------------------|
| 0 | `SEM_GLOBAL_LOCK` | Protect shared memory writes (currently commented out in code) |
| 1 | `SEM_TICK_START` | CC posts N permits; each unit waits for one to begin tick |
| 2 | `SEM_TICK_DONE` | Each unit posts when done; CC waits N times to synchronize |

[\<SEM_* enum definition\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/shared.h?plain=1#L111-L121)

### Shared Memory Layout

**File**: `include/ipc/shared.h`

```c
typedef struct {
    uint32_t magic;                       // SHM_MAGIC for sanity check
    uint32_t ticks;                       // Global tick counter
    uint16_t next_unit_id;                // Unit ID allocator
    uint16_t unit_count;                  // Active unit count
    uint16_t tick_expected;               // Units expected this tick
    uint16_t tick_done;                   // Units finished this tick
    uint32_t last_step_tick[MAX_UNITS+1]; // Per-unit last tick performed
    unit_id_t grid[M][N];                 // Grid state (120×40)
    unit_entity_t units[MAX_UNITS+1];     // Unit states (indexed by unit_id)
} shm_state_t;
```
[\<shm_state_t definition\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/shared.h?plain=1#L90-L106)

---

## Data Structures

### unit_entity_t

```c
typedef struct {
    pid_t pid;              // Process ID for signaling
    uint8_t faction;        // faction_t (FACTION_REPUBLIC, FACTION_CIS)
    uint8_t type;           // unit_type_t (FLAGSHIP, DESTROYER, etc.)
    uint8_t alive;          // 1 = alive, 0 = dead
    point_t position;       // Grid coordinates (center)
    uint32_t flags;         // Reserved for status/orders
    st_points_t dmg_payload;// Damage received this tick
} unit_entity_t;
```
[\<unit_entity_t definition\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/shared.h?plain=1#L36-L45)

### scenario_t

```c
typedef struct {
    char name[MAX_SCENARIO_NAME];      // Scenario display name
    int map_width;                     // Map width (default M=120)
    int map_height;                    // Map height (default N=40)
    obstacle_t obstacles[MAX_OBSTACLES]; // Obstacle positions
    int obstacle_count;                // Number of obstacles
    unit_placement_t units[MAX_INITIAL_UNITS]; // Pre-defined unit placements
    int unit_count;                    // Number of pre-defined units
    placement_mode_t placement_mode;   // Auto-generation placement mode
    int republic_carriers;             // Auto-gen: Republic carriers
    int republic_destroyers;           // Auto-gen: Republic destroyers
    int republic_fighters;             // Auto-gen: Republic fighters
    int cis_carriers;                  // Auto-gen: CIS carriers
    int cis_destroyers;                // Auto-gen: CIS destroyers
    int cis_fighters;                  // Auto-gen: CIS fighters
} scenario_t;
```
[\<scenario_t definition\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/CC/scenario.h?plain=1#L30-L53)

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
float damage_multiplier(unit_type_t unit, unit_type_t target);
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
