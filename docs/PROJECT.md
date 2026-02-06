# Space Skirmish - Project Documentation

## Table of Contents

1. [Project Overview](#project-overview)
2. [System Architecture](#system-architecture)
3. [Module Documentation](#module-documentation)
4. [Implementation Details](#implementation-details)
5. [Testing Methodology](#testing-methodology)
6. [Test Propositions](#test-propositions)
7. [Build and Deployment](#build-and-deployment)
8. [Usage Guide](#usage-guide)
9. [Performance Analysis](#performance-analysis)
10. [Future Enhancements](#future-enhancements)
11. [References](#references)

---

## Project Overview

### Introduction

**Space Skirmish** is a multi-process, IPC-based real-time space battle simulation inspired by "Empire at War" (RTS game). The simulation models combat between two factions - **Republic** and **CIS** - using autonomous unit processes that communicate through System V IPC mechanisms.

### Project Goals

1. **Demonstrate IPC Mastery**: Showcase advanced usage of shared memory, semaphores, and message queues
2. **Multi-Process Architecture**: Implement complex process hierarchy with 60+ concurrent processes
3. **Real-Time Simulation**: Tick-based synchronization with configurable speed
4. **Interactive Control**: User interface and console manager for runtime control
5. **Robust Error Handling**: Comprehensive logging and error recovery mechanisms

### Key Features

#### Core Simulation
- **2 Factions**: Republic and CIS with distinct unit sets ([`faction_t`](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/shared.h?plain=1#L25))
- **6 Unit Types**: Flagship, Destroyer, Carrier, Fighter, Bomber, Elite Fighter ([`unit_type_t`](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/shared.h?plain=1#L26))
- **Autonomous Behavior**: Units operate independently with AI decision-making
- **Commander System**: Hierarchical command structure (Flagship → Battleship → Squadron)
- **Combat Mechanics**: Weapon systems, damage calculation, type effectiveness

#### Technical Features
- **64 Unit Limit**: Configurable maximum concurrent units ([`MAX_UNITS`](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/shared.h?plain=1#L13))
- **Tick Synchronization**: Barrier-based coordination across all processes
- **Shared Memory Grid**: 120×40 spatial representation with multi-cell units ([`M`, `N` constants](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/shared.h?plain=1#L7-L9))
- **Message Queue Protocol**: Asynchronous damage, orders, and commander requests ([`ipc_mesq.h`](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/ipc_mesq.h))
- **Semaphore Locks**: Thread-safe access to shared resources ([`semaphores.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/semaphores.c))
- **Terminal Tee**: Output redirection for logging and UI integration

#### User Interaction
- **ncurses UI**: Real-time visualization with MAP, UST (Unit Status Table), STD windows ([`ui_main.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_main.c))
- **Console Manager**: Interactive CLI for spawning units, controlling simulation ([`console_manager.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c))
- **Scenario System**: Pre-configured battle scenarios with auto-generation ([`scenario.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/scenario.c))
- **Logging Infrastructure**: Per-process and combined logs with millisecond timestamps ([`utils.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/utils.c))

---

## System Architecture

### Process Hierarchy

```
┌─────────────────────────────────────────────────────────────────┐
│                      Command Center (CC)                        │
│          - IPC initialization and cleanup                       │
│          - Tick barrier synchronization                         │
│          - Scenario loading and unit spawning                   │
│          - Console Manager thread                               │
└────────────┬────────────────────────────────┬───────────────────┘
             │                                │
             │                                │
    ┌────────▼─────────┐           ┌──────────▼──────────┐
    │  Battleship      │           │   Squadron          │
    │  Processes       │           │   Processes         │
    │  (Flagship,      │           │   (Fighter,         │
    │   Destroyer,     │           │    Bomber,          │
    │   Carrier)       │           │    Elite)           │
    └────────┬─────────┘           └─────────────────────┘
             │
             │ Commander System
             └────────────┐
                          │
             ┌────────────▼─────────────┐
             │  Squadron Underlings     │
             │  Follow commander orders │
             └──────────────────────────┘

External Processes:
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│ User Interface  │  │ Console Manager │  │ Terminal Tee    │
│ (ui_main.c)     │  │ (console_      │  │ (terminal_      │
│ - ncurses       │  │  manager.c)    │  │  tee.c)         │
│ - 3 windows     │  │ - Interactive   │  │ - Output        │
│ - 3 threads     │  │   CLI           │  │   capture       │
└─────────────────┘  │ - Commands      │  │ - Multi-dest    │
                     └─────────────────┘  └─────────────────┘
```

Source files: [`ui_main.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_main.c), [`console_manager.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c), [`terminal_tee.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c)

### IPC Architecture

```
┌───────────────────────────────────────────────────────────────┐
│                    Shared Memory Segment                      │
│               (shm_state_t - shared.h L87-102)                 │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐     │
│  │  Grid [120][40]: unit_id_t (shared.h L7-8)            │     │
│  │  - Spatial representation                            │     │
│  │  - Multi-cell unit occupancy                         │     │
│  └──────────────────────────────────────────────────────┘     │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐     │
│  │  Units [64]: unit_entity_t (shared.h L37-47)          │     │
│  │  - PID, type, faction, position                      │     │
│  │  - alive flag, dmg_payload                           │     │
│  └──────────────────────────────────────────────────────┘     │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐     │
│  │  Global State:                                       │     │
│  │  - magic, ticks, next_unit_id, unit_count            │     │
│  │  - tick_expected, tick_done, last_step_tick[]        │     │
│  └──────────────────────────────────────────────────────┘     │
└───────────────────────────────────────────────────────────────┘

┌───────────────────────────────────────────────────────────────┐
│           Semaphore Array (shared.h L105-112)                 │
│                                                               │
│  [0] SEM_GLOBAL_LOCK   - Protect shared memory writes         │
│  [1] SEM_TICK_START    - Signal tick start to units           │
│  [2] SEM_TICK_DONE     - Signal tick completion to CC         │
└───────────────────────────────────────────────────────────────┘

┌───────────────────────────────────────────────────────────────┐
│           Message Queues (ipc_mesq.h)                         │
│                                                                │
│  q_req (Request Queue):                                       │
│    - Damage messages (mq_damage_t L55-59)                     │
│    - Spawn requests (mq_spawn_req_t L22-31)                   │
│    - Console commands (mq_cm_cmd_t L67-79)                    │
│                                                                │
│  q_rep (Reply Queue):                                          │
│    - Commander replies (squadron_pid ← battleship)            │
│    - Spawn replies (sender_pid ← CC)                          │
│    - Console command replies (CM_pid ← CC)                    │
│                                                                │
│  q_order (Order Queue):                                        │
│    - Orders from commander to underlings (mq_order_t L61-65)  │
│    - PATROL, ATTACK, GUARD, MOVE commands                     │
└───────────────────────────────────────────────────────────────┘
```

See [`shared.h`](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/shared.h) and [`ipc_mesq.h`](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/ipc_mesq.h) for complete structure definitions.

### Synchronization Model

**Tick-Based Barrier Pattern:**

```
Tick N Start
    │
    ├─ CC: sem_post(SEM_TICK_START) × alive_count
    │
    ├─ All Units: sem_wait(SEM_TICK_START)
    │
    ├─ Units Process Logic:
    │    1. Receive and process damage
    │    2. Radar scan for enemies
    │    3. Execute orders (patrol/attack/guard)
    │    4. Movement toward target
    │    5. Combat (fire weapons)
    │
    ├─ All Units: sem_post(SEM_TICK_DONE)
    │
    ├─ CC: sem_wait(SEM_TICK_DONE) × alive_count
    │
Tick N Complete
    │
    ├─ usleep(tick_speed_ms * 1000)
    │
Tick N+1 Start
```

---

## Module Documentation

The project is organized into specialized modules, each documented in detail:

### 1. Command Center Module ([CC_MODULE.md](CC_MODULE.md))

**Purpose**: Core orchestrator of the simulation

**Components**:
- [`command_center.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c) - Main process, tick loop, IPC initialization
- [`battleship.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/battleship.c) - Heavy unit process (Flagship, Destroyer, Carrier)
- [`squadron.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/squadron.c) - Light unit process (Fighter, Bomber, Elite)
- [`unit_logic.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c) - Combat mechanics, movement, pathfinding
- [`unit_ipc.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_ipc.c) - IPC abstractions for units
- [`unit_stats.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_stats.c) - Unit statistics database
- [`weapon_stats.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/weapon_stats.c) - Weapon characteristics
- [`scenario.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/scenario.c) - Configuration file loader

**Key Responsibilities**:
- Process spawning and lifecycle management
- Tick synchronization barrier
- Scenario loading and unit placement
- Console Manager command processing
- Graceful shutdown and cleanup

### 2. IPC Module ([IPC_MODULE.md](IPC_MODULE.md))

**Purpose**: System V IPC infrastructure

**Components**:
- [`ipc_context.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c) - IPC resource management
- [`semaphores.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/semaphores.c) - Semaphore operations
- [`ipc_mesq.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_mesq.c) - Message queue protocols
- [`shared.h`](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/shared.h) - Shared memory structure definitions

**Key Responsibilities**:
- Shared memory creation, attachment, cleanup
- Semaphore array initialization and operations
- Message queue protocols for damage, orders, commands
- Thread-safe access patterns

### 3. User Interface Module ([UI_MODULE.md](UI_MODULE.md))

**Purpose**: ncurses-based real-time visualization

**Components**:
- [`ui_main.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_main.c) - Main loop, window management
- [`ui_map.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_map.c) - MAP window (grid visualization)
- [`ui_ust.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_ust.c) - UST window (unit status table)
- [`ui_std.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_std.c) - STD window (terminal output)

**Key Responsibilities**:
- Multi-threaded rendering (MAP, UST, STD threads)
- Real-time grid visualization with color-coded factions
- Unit status table with HP, shields, position
- Terminal output integration via FIFO
- Keyboard input for UI control

### 4. Console Manager Module ([CM_MODULE.md](CM_MODULE.md))

**Purpose**: Interactive command-line interface

**Components**:
- [`console_manager.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c) - CLI loop, command parsing

**Key Responsibilities**:
- Command parsing and validation
- Request-response protocol with CC
- Unit spawning interface
- Simulation control (freeze/unfreeze, tick speed)
- Grid toggle, simulation termination

### 5. Error Handling, Logging, and Terminal Tee ([ERROR_LOG_TEE.md](ERROR_LOG_TEE.md))

**Purpose**: Diagnostic and reliability infrastructure

**Components**:
- [`error_handler.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/error_handler.c) - Centralized error handling
- [`utils.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/utils.c) - Logging backend
- [`terminal_tee.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c) - Output redirection

**Key Responsibilities**:
- 3-level error severity (FATAL, ERROR, WARNING) [\ <error_level_t>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/error_handler.h?plain=1#L9-L14)
- 20 application-specific error codes [\ <app_error_t>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/error_handler.h?plain=1#L16-L40)
- Dual logging (per-process + combined ALL.log)
- Terminal tee for simultaneous output to terminal, log, and UI
- Atomic append with O_APPEND for multi-process safety

---

## Implementation Details

### Unit Types and Statistics

Unit statistics are defined in [`unit_stats.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_stats.c?plain=1#L17-L28) with weapon loadouts from [`weapon_stats.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/weapon_stats.c?plain=1#L21-L42):

| Unit Type | HP | Shields | Speed | Size | Detection Range | Weapons |
|-----------|-----|---------|-------|------|-----------------|---------|  
| **Flagship** | 200 | 100 | 2 | 3 (13 cells) | 120 (full map) | 2× LR Cannon, 2× MR Gun |
| **Destroyer** | 100 | 100 | 3 | 2 (5 cells) | 20 | 2× LR Cannon, 1× MR Gun |
| **Carrier** | 100 | 100 | 6 | 2 (5 cells) | 20 | 1× LR Cannon, 2× MR Gun |
| **Fighter** | 20 | 0 | 5 | 1 (1 cell) | 10 | 1× SR Gun |
| **Bomber** | 30 | 0 | 4 | 1 (1 cell) | 15 | 1× SR Cannon |
| **Elite** | 20 | 20 | 6 | 1 (1 cell) | 15 | 1× SR Gun |

**Type Effectiveness Multipliers** (from [`damage_multiplier()`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L16-L39)):

| Attacker | Strong Against | Multiplier |
|----------|----------------|------------|
| Flagship | Carrier | 1.5× |
| Destroyer | Flagship, Destroyer, Carrier | 1.5× |
| Carrier | Fighter, Bomber, Elite | 1.5× |
| Fighter | Fighter, Bomber | 1.5× |
| Bomber | Flagship, Destroyer, Carrier | 3.0× |
| Elite | Fighter, Bomber, Elite | 2.0× |

**Weapon Accuracy** (from [`accuracy_multiplier()`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L41-L55)):

| Weapon Type | vs Large Ships | vs Small Ships |
|-------------|----------------|----------------|
| Cannons (LR/MR/SR) | 75% | 25% |
| Guns (LR/MR/SR) | 0% | 75% |

**Damage Formula** (from [`damage_to_target()`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_logic.c?plain=1#L57-L67)):
```
base_damage = weapon.damage
type_modifier = damage_multiplier(attacker_type, target_type)
accuracy_modifier = accuracy_multiplier(weapon_type, target_type)
hit_roll = rand(0.0, 1.0) < accuracy_modifier

if (hit_roll):
    final_damage = base_damage * type_modifier
    apply_damage(target, final_damage)
else:
    miss
```

### Commander System

The commander system manages hierarchical relationships between units. See [`mq_commander_req_t`](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/ipc_mesq.h?plain=1#L41-L46) for request protocol.

**Hierarchy:**
```
Flagship (up to 1 per faction)
    ├─ Battleship (multiple)
    │   └─ Squadron (underlings)
    └─ Squadron (direct underlings)
```

**Commander Assignment:**
1. Squadron spawned → sends commander request to nearest Battleship
2. Battleship evaluates → accepts/rejects based on capacity
3. If accepted → squadron follows commander orders
4. Commander sends orders via `q_order` message queue
5. Squadron checks for orders each tick

**Orders** (from [`unit_order_t`](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/shared.h?plain=1#L24)):
- `DO_NOTHING`: Idle state
- `PATROL`: Move to random points, engage enemies opportunistically
- `ATTACK <x,y>`: Move to coordinates, engage enemies en route
- `MOVE <x,y>`: Move to coordinates without engaging
- `MOVE_ATTACK <x,y>`: Move and attack along the way
- `GUARD <x,y>`: Hold position, engage enemies within range

**Commander Death:**
- Commander broadcasts nearest friendly Battleship location before dying
- Squadrons receive location → seek new commander
- If no commander found → autonomous patrol mode

### Scenario System

Scenarios are loaded and parsed by [`scenario.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/scenario.c). Configuration files are stored in [`scenarios/`](https://github.com/PaurXen/Space-Skirmish-/tree/main/scenarios).

**Configuration Format:**
```ini
[scenario]
name = Fleet Battle

[map]
width = 120
height = 40

[units]
# type faction x y
CARRIER REPUBLIC 10 10
DESTROYER CIS 110 30

[obstacles]
# x y
60 20
61 20

[autogenerate]
placement_mode = CORNERS
republic_carriers = 1
cis_destroyers = 2
```

**Placement Modes:**
- `CORNERS`: Units at map corners
- `EDGES`: Units along edges
- `RANDOM`: Random positions
- `LINE`: Linear formation
- `SCATTERED`: Distributed placement
- `MANUAL`: Explicit coordinates in `[units]` section

---

## Testing Methodology

### Testing Objectives

The testing strategy for Space Skirmish aims to validate:

1. **Functional Correctness**: Units behave according to specifications
2. **IPC Robustness**: Shared memory, semaphores, and message queues work reliably
3. **Performance**: Simulation runs efficiently with 64 units
4. **Synchronization**: Tick barrier ensures consistent state
5. **Error Handling**: System degrades gracefully on failures
6. **Edge Cases**: Boundary conditions and race conditions handled

### Testing Levels

#### 1. Unit Testing
- **Scope**: Individual functions in isolation
- **Tools**: Custom test harnesses (`tests/test_*.c`)
- **Coverage**: Combat mechanics, pathfinding, damage calculation

#### 2. Integration Testing
- **Scope**: Module interactions (CC ↔ IPC ↔ Units)
- **Tools**: Scenario-based test runs with logging
- **Coverage**: IPC protocols, message passing, synchronization

#### 3. System Testing
- **Scope**: End-to-end simulation scenarios
- **Tools**: Predefined scenarios, manual observation
- **Coverage**: Full battle simulations, UI interaction, CM commands

#### 4. Performance Testing
- **Scope**: Scalability and resource usage
- **Tools**: `time`, `top`, `strace`, custom profiling
- **Coverage**: Tick latency, memory usage, CPU utilization

#### 5. Stress Testing
- **Scope**: Limit testing and failure modes
- **Tools**: Max unit spawning, resource exhaustion
- **Coverage**: 64-unit limit, message queue overflow, semaphore deadlocks

### Existing Test Files

Located in [`tests/`](https://github.com/PaurXen/Space-Skirmish-/tree/main/tests) directory:

1. **[`test_accuracy.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/tests/test_accuracy.c)**
   - Tests weapon accuracy calculations
   - Validates accuracy modifiers for cannon/gun vs large/small targets

2. **[`test_size_mechanic.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/tests/test_size_mechanic.c)**
   - Tests multi-cell unit placement
   - Validates grid occupancy for size 1, 2, 3 units

3. **[`test_unit_logic.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/tests/test_unit_logic.c)**
   - Tests combat logic functions
   - Validates damage multipliers, target selection

4. **[`test_unit_radar.c`](https://github.com/PaurXen/Space-Skirmish-/blob/main/tests/test_unit_radar.c)**
   - Tests radar detection system
   - Validates detection range, enemy filtering

5. **[`test_unit_radar.py`](https://github.com/PaurXen/Space-Skirmish-/blob/main/tests/test_unit_radar.py)**
   - Python-based radar visualization
   - Generates test scenarios for radar coverage

---

## Test Propositions

Below are comprehensive test scenarios to be implemented and documented. For each test, fill in the following sections:

- **Test Objective**: What is being tested
- **Test Setup**: Initial conditions, scenario configuration
- **Test Procedure**: Step-by-step execution
- **Expected Results**: What should happen
- **Actual Results**: What actually happened (to be filled during testing)
- **Pass/Fail Criteria**: Conditions for success
- **Notes**: Observations, issues, edge cases

---

### Test Category 1: Unit Behavior

#### Test 1.1: Carrier Squadron Spawning and GUARD Orders

**Test Objective**:
_Validate that Carrier spawns squadrons from fighter bay and issues GUARD orders to underlings_

**Test Setup**:
```
Scenario: default.conf
- Map: 120×40
- Units:
  - 1 Republic Carrier (type=3) at (5,5)
  - No enemy units
- Duration: 8 ticks
- Log: logs/run_2026-02-05_17-47-12_pid21753/ALL.log
```

**Test Procedure**:
```
1. Start Command Center with default scenario (1 Carrier)
2. Observe tick 1: Carrier patrols and requests squadron spawn
3. Use CM to freeze simulation after tick 1
4. Use CM to unfreeze simulation
5. Observe ticks 2-7: Carrier spawns squadrons and issues orders
6. Use CM 'end' command to terminate simulation
7. Verify graceful shutdown of all processes
```

**Expected Results**:
```
- Carrier should patrol autonomously (order=1 PATROL)
- Carrier should spawn squadrons up to fighter bay capacity (5 units)
- Spawned squadrons should receive GUARD orders (order=5) with target=commander_id
- Squadrons should follow commander and guard its position
- CM freeze/unfreeze should pause/resume simulation correctly
- Graceful shutdown: all units receive SIGTERM and exit cleanly
```

**Actual Results**:
```
✅ PASSED - Test executed on 2026-02-05

Timeline:
- 17:47:12.346: CC spawned Carrier (unit_id=1, pid=21756) at (5,5), SP=6, DR=20
- 17:47:13.351: Tick 1 - Carrier patrols, picks target (16,21)
- 17:47:13.353: First spawn request at (8,11) REJECTED (insufficient space/OOB)
- 17:47:13.478: CM freeze command received, simulation frozen

[~10 hour pause]

- 03:32:42.050: CM unfreeze command, simulation resumed
- 03:32:42.389: Tick 2-3 - Carrier spawned SQ 2 (Bomber, type=5) at (9,16)
- 03:32:43.396: Tick 4 - Carrier spawned SQ 3 (Bomber, type=5) at (15,23)
- 03:32:44.417: Tick 5 - Carrier spawned SQ 4 (Fighter, type=4) at (12,21)
- 03:32:45.431: Tick 6 - Carrier spawned SQ 5 (Bomber, type=5) at (16,13)
- 03:32:46.450: Tick 7 - Carrier spawned SQ 6 (Fighter, type=4) at (15,7)

Squadron behavior:
- All squadrons received GUARD order (order=5) with target=1 (commander)
- Squadrons tracked commander position and moved toward guard positions
- Movement logged: SQ positions converging toward Carrier patrol path

Shutdown sequence:
- 03:32:48.437: CM 'end' command received
- 03:32:48.497: CC sent SIGTERM to all 6 alive units
- 03:32:48.500-503: All units logged "terminating, cleaning registry/grid"
- 03:32:48.511: All 6 children reaped with exit status 0
- 03:32:48.514: IPC objects detached and destroyed, CC logger closed
```

**Pass/Fail Criteria**:
```
✅ Carrier spawns squadrons correctly (5/5 spawned after initial rejection)
✅ Fighter bay capacity enforced (capacity=5, current=5 at tick 7)
✅ GUARD orders (order=5) sent to all underlings each tick
✅ Squadrons acknowledge orders and track commander state
✅ CM freeze/unfreeze works correctly
✅ Graceful shutdown: all processes exit with status 0
✅ No orphan processes, all children reaped
```

**Notes**:
```
1. First spawn at (8,11) rejected - likely due to position being too close
   to Carrier's movement path (Carrier moved to (8,10) same tick)
2. Spawn pattern follows fighter_bay_view_t: Bomber, Bomber, Fighter, Bomber, Fighter
   matching k_fighter_types for TYPE_CARRIER in unit_stats.c
3. Order=5 corresponds to GUARD in unit_order_t enum
4. Long pause between freeze/unfreeze (10+ hours) demonstrates simulation
   state persistence during freeze
5. Warning "[CC] Could not acquire lock for shutdown" is expected -
   shutdown proceeds with SIGTERM regardless
```

---

#### Test 1.2: Flagship Autonomous Behavior (TBA)

**Test Objective**:
_Validate that Flagship operates autonomously according to personality (Passive/Active/Aggressive)_

**Test Setup**:
```
Scenario: 
- Map: 120×40
- Units:
  - 1 Republic Flagship at (60, 20) - Passive personality
  - 1 CIS Flagship at (80, 20) - Aggressive personality
  - No other units
- Duration: 100 ticks
```

**Test Procedure**:
```
1. [TO FILL IN]
2. 
3. 
```

**Expected Results**:
```
[TO FILL IN]
- Passive Flagship should:
  
- Aggressive Flagship should:
  
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 1.2: Battleship Patrol Behavior

**Test Objective**:
_Verify Battleship patrol logic when no commander exists (autonomous patrol)_

**Test Setup**:
```
Scenario: default.conf (Default Battle)
- Map: 120×40
- Units: 1 Republic Carrier (BS 1) at (5,5)
- Carrier type=3 with fighter bay capacity=5
- Log: logs/run_2026-02-06_04-24-53_pid145812/ALL.log
- Console Manager: freeze, set speed to 3000ms, resume
- UI: running for visual verification
```

**Test Procedure**:
```
1. Start simulation: ./command_center --scenario default &
2. Start console_manager and immediately freeze simulation
3. Start UI: ./ui
4. Set tick speed to 3000ms for easier observation
5. Resume simulation and observe:
   a. BS 1 picks random patrol target within patrol radius
   b. BS 1 moves toward target (speed=6 per tick)
   c. When target reached, picks new patrol target
   d. BS 1 spawns squadrons every tick until capacity full
6. Run for 46 ticks and issue 'end' command
```

**Expected Results**:
```
- BS 1 patrols autonomously (no commander, order=1 PATROL)
- Patrol targets within configured patrol radius (20)
- Movement: 6 cells per tick (Carrier speed)
- Squadron spawning: 1 per tick until capacity=5 reached
- Squadrons receive GUARD orders (order=5) to escort BS 1
```

**Actual Results**:
```
✅ PASSED - Test executed 2026-02-06

Battleship Patrol Sequence:
  Tick 1: pos=(5,5) → picked patrol target (24,1), moved to (11,5)
  Tick 2: pos=(11,5) → target (24,1), moved to (17,5)
  Tick 3: pos=(17,5) → target (24,1), moved to (22,2)
  Tick 4: pos=(22,2) → target (24,1), arrived at (24,1) dt2=0
  ...
  Tick 46: pos=(25,7) → target (25,7), dt2=0 (at patrol point)

Patrol Target Selection Observed:
  - (24,1), (25,7), and other random targets within radius
  - New target picked when dt2=0 (destination reached)

Squadron Spawning:
  Tick 1: Spawned SQ 2 at (12,2) - type=5 (light fighter)
  Tick 2: Spawned SQ 3 at (21,5) - type=5 (light fighter)
  Tick 3: Spawned SQ 4 at (24,3) - type=4 (fighter)
  Tick 4: Spawned SQ 5 at (26,3) - type=4 (fighter)
  Tick 5: Spawned SQ 6 at (28,5) - type=4 (fighter)
  Fighter bay: capacity=5, current=5 (full)

Squadron Orders:
  - All squadrons received order=5 (GUARD) with target=1 (BS 1)
  - Squadrons follow BS 1 maintaining escort positions

UI Observations:
  - MAP showed BS 1 (symbol C) moving across grid
  - UST showed BS 1 stats: hp=100, sp=6
  - All 5 squadrons visible and tracking BS 1

Shutdown (tick 46):
  - 'end' command via console_manager
  - 6 units reaped (BS 1 + 5 squadrons)
  - UI detected IPC destruction and exited cleanly
```

**Pass/Fail Criteria**:
```
✅ PASS - Battleship patrols autonomously without commander
✅ PASS - Random patrol targets picked within radius
✅ PASS - Movement speed matches Carrier spec (6/tick)
✅ PASS - Squadrons spawned until capacity reached
✅ PASS - Squadrons receive GUARD orders
✅ PASS - UI shutdown graceful (BUG-001 fix verified)
```

**Notes**:
```
- This test also validates BUG-001 fix: UI exited cleanly after 'end' command
- Console Manager freeze/resume commands working correctly
- Tick speed command (3000ms) allowed detailed observation
- dt2 = squared distance to target (dt2=0 means at destination)
```

---

#### Test 1.3: Squadron Commander Assignment

**Test Objective**:
_Test squadron seeking and accepting commander from Battleship_

**Test Setup**:
```
Scenario: Custom test scenario
- Map: 120×40
- Units:
  - BS 1 (Destroyer, type=2) at (8,8) - SP=3, DR=20
  - SQ 2 (Fighter, type=4) at (12,12) - no initial commander
  - SQ 3 (Fighter, type=4) at (14,14) - no initial commander
- Log: logs/run_2026-02-06_04-31-31_pid147247/
- Duration: 13 ticks
```

**Test Procedure**:
```
1. Start simulation with 1 Destroyer + 2 independent Fighters
2. Observe tick 1: Squadrons have no commander (commander=0, state=0)
3. Observe tick 1: Squadrons detect BS 1 and send commander requests
4. Observe tick 2: BS 1 accepts squadrons as underlings
5. Observe tick 2: Squadrons receive assignment confirmation
6. Observe tick 2+: Squadrons receive GUARD orders and follow BS 1
7. Verify squadrons track commander position throughout simulation
```

**Expected Results**:
```
- Tick 1: SQ 2 and SQ 3 start with commander=0 (no commander)
- Tick 1: Squadrons send commander request to nearest Battleship
- Tick 1: Squadrons patrol independently while awaiting response
- Tick 2: BS 1 accepts both squadrons as underlings
- Tick 2: Squadrons receive "assigned to commander 1" confirmation
- Tick 2+: Squadrons receive GUARD orders (order=5) with target=1
- Tick 2+: Squadrons move toward commander's position
```

**Actual Results**:
```
✅ PASSED - Test executed 2026-02-06

Squadron 2 (SQ_u2_pid_147252.log):
  Tick 1: [SQ 2] current commander 0 state 0
          [SQ 2] sent commander request to potential BS 1
          [SQ 2] picked new patrol target (20,7) (patrolling while waiting)
          pos=(12,12) → (16,9)
  
  Tick 2: [SQ 2] assigned to commander 1
          [SQ 2] received order 5 with target 1
          [SQ 2] current commander 1 state 1
          Started tracking BS 1's position

Squadron 3 (SQ_u3_pid_147253.log):
  Tick 1: [SQ 3] current commander 0 state 0
          [SQ 3] sent commander request to potential BS 1
          [SQ 3] picked new patrol target (5,17) (patrolling while waiting)
          pos=(14,14) → (9,14)
  
  Tick 2: [SQ 3] assigned to commander 1
          [SQ 3] received order 5 with target 1
          [SQ 3] current commander 1 state 1
          Started tracking BS 1's position

Battleship 1 (BS_u1_pid_147251.log):
  Tick 1: [BS 1] accepted squadron 3 as underling
          [BS 1] sent order 5 with target 1 to SQ 3
  
  Tick 2: [BS 1] added squadron 4 to underlings (spawned)
          [BS 1] accepted squadron 2 as underling
          [BS 1] sent order 5 with target 1 to SQ 3
          [BS 1] sent order 5 with target 1 to SQ 4
          [BS 1] sent order 5 with target 1 to SQ 2

Post-Assignment Behavior (ticks 3-13):
  - SQ 2 and SQ 3 continuously receive GUARD orders from BS 1
  - Both squadrons track commander position each tick
  - Squadrons move toward BS 1's current position
  - BS 1 also spawned SQ 4, SQ 5, SQ 6 (fighter bay capacity=3)
  - All 5 squadrons following BS 1 by tick 4

Shutdown (tick 13):
  - All units terminated cleanly
  - 6 children reaped (BS 1 + 5 squadrons)
```

**Pass/Fail Criteria**:
```
✅ PASS - Squadrons start with no commander (commander=0)
✅ PASS - Squadrons detect and request commander from BS
✅ PASS - BS accepts squadron commander requests
✅ PASS - Squadrons receive assignment confirmation
✅ PASS - GUARD orders (order=5) sent to all underlings
✅ PASS - Squadrons track commander state (state=1 = alive)
✅ PASS - Squadrons move toward commander position
```

**Notes**:
```
Commander Assignment Protocol:
1. Squadron checks registry for nearby Battleships (DR=20 detection)
2. Squadron sends commander request via message queue
3. Battleship accepts squadron, adds to underlings list
4. Battleship sends GUARD order with target=self (unit_id=1)
5. Squadron receives assignment, updates commander field
6. Squadron switches from PATROL (order=1) to GUARD (order=5)
7. Squadron targets commander's position for escort behavior

Key observations:
- SQ 3 was accepted before SQ 2 (closer to BS 1 initially)
- Spawned squadrons (SQ 4-6) automatically assigned to spawning BS
- "commander 1 state 1" means commander unit 1 is alive (state=1)
- All squadrons maintain GUARD behavior until commander dies
```

---

#### Test 1.4: Commander Death and Reassignment

**Test Objective**:
_Verify squadrons reassign to new commander when current commander is destroyed_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

### Test Category 2: Combat Mechanics

#### Test 2.1: Type Effectiveness Multipliers

**Test Objective**:
_Validate damage multipliers for unit type matchups (e.g., Bomber +3× vs Carrier)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 2.2: Weapon Accuracy System

**Test Objective**:
_Test cannon accuracy (75% vs large, 25% vs small) and gun accuracy (0% vs large, 75% vs small)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 2.3: Damage Application (Shields then HP)

**Test Objective**:
_Verify damage reduces shields first, then HP_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 2.4: Carrier vs Destroyer Effectiveness

**Test Objective**:
_Test specific matchup: Carrier (+1.5× vs small ships) vs Destroyer (+1.5× vs large ships)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

### Test Category 3: Synchronization

#### Test 3.1: Tick Barrier Synchronization

**Test Objective**:
_Verify all units process exactly one tick before CC advances to next tick_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 3.2: Unit Death During Tick

**Test Objective**:
_Test behavior when unit dies mid-tick (should complete current tick, then mark dead)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 3.3: Freeze/Unfreeze Simulation

**Test Objective**:
_Validate freeze command pauses all units, unfreeze resumes correctly_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 3.4: Tick Speed Adjustment

**Test Objective**:
_Test changing tick speed during simulation (e.g., 1000ms → 500ms)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

### Test Category 4: IPC Mechanisms

#### Test 4.1: Shared Memory Consistency

**Test Objective**:
_Verify shared memory updates are visible to all processes (grid updates, unit state)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 4.2: Message Queue Overflow Handling

**Test Objective**:
_Test behavior when message queue fills up (e.g., 64 damage messages pending)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 4.3: Semaphore Deadlock Prevention

**Test Objective**:
_Verify no deadlocks occur under heavy contention (64 units accessing shared memory)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 4.4: Commander Request-Reply Protocol

**Test Objective**:
_Test message queue request-reply protocol for commander assignment_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

### Test Category 5: Grid and Spatial Logic

#### Test 5.1: Multi-Cell Unit Placement

**Test Objective**:
_Verify 1×1, 2×2, 3×3 units occupy correct grid cells_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 5.2: Collision Detection

**Test Objective**:
_Test units cannot occupy same grid cells (spawn fails if occupied)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 5.3: Pathfinding Around Obstacles

**Test Objective**:
_Verify A* pathfinding navigates around obstacles and other units_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 5.4: Radar Detection Range

**Test Objective**:
_Validate radar scan detects enemies within detection range, ignores beyond_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

### Test Category 6: User Interface

#### Test 6.1: MAP Window Real-Time Updates

**Test Objective**:
_Verify MAP window updates in real-time as units move and die_

**Test Setup**:
```
Scenario: skirmish.conf (fighters only, high movement)
- Map: 120×40
- Units: 2 squadrons per faction (4 total)
- Terminal: At least 120×40 characters
```

**Test Procedure**:
```
1. Start simulation: ./command_center --scenario skirmish &
2. Start UI: ./ui
3. Observe MAP window (top-left panel)
4. Watch for 30+ ticks:
   a. Units should move each tick
   b. Unit symbols (F, B, C) should update positions
   c. Faction colors (green/red) should be correct
5. Wait for combat - observe unit removal on death
6. Press 'q' to exit UI
```

**Expected Results**:
```
- MAP updates every ~300ms (UI_REFRESH_MS)
- Unit positions match log output
- Dead units removed from display within 1 refresh cycle
- No flickering or partial renders
- Grid boundaries (borders) remain stable
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
✅ PASS if: Units move smoothly, deaths reflected, no visual glitches
❌ FAIL if: Stale positions, ghost units, screen corruption
```

**Notes**:
```
MAP thread uses message queue to request grid updates from CC.
See ui_map.c for refresh logic.
```

---

#### Test 6.2: UST Window Accuracy

**Test Objective**:
_Validate Unit Status Table displays correct HP, shields, position for all units_

**Test Setup**:
```
Scenario: default.conf
- Map: 120×40
- Units: 1 Carrier (spawns squadrons over time)
- Log file: logs/run_*/ALL.log for comparison
```

**Test Procedure**:
```
1. Start simulation: ./command_center --scenario default &
2. Start UI: ./ui
3. Focus on UST window (right panel)
4. Compare displayed values with log output:
   a. Unit ID matches
   b. HP value matches log "[BS X] tick=Y ... hp=Z"
   c. Shield value matches "sp=N"
   d. Position matches "pos=(X,Y)"
   e. Faction indicator (R/S) correct
5. Wait for damage events - verify HP/shield decrements
6. Verify newly spawned squadrons appear in UST
```

**Expected Results**:
```
- All alive units listed in UST
- HP/SP values within 1 tick of accuracy
- Position format: "(X,Y)" matches log
- Faction: R=Republic, S=Separatist
- Dead units removed from list
- Scroll works if >10 units (Page Up/Down)
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
✅ PASS if: All stats accurate within 1 tick, new units appear, dead units removed
❌ FAIL if: Wrong values, missing units, stale data
```

**Notes**:
```
UST reads directly from shared memory (shm_state_t).
Uses SEM_GLOBAL_LOCK for synchronization.
```

---

#### Test 6.3: STD Window Terminal Output

**Test Objective**:
_Test STD window displays terminal output from CC via FIFO_

**Test Setup**:
```
Scenario: default.conf
- FIFO: /tmp/skirmish_std.fifo (created by CC)
- Terminal: Ensure STD window visible (bottom panel)
```

**Test Procedure**:
```
1. Start simulation: ./command_center --scenario default &
2. Start UI: ./ui
3. Observe STD window (bottom panel)
4. Verify output appears:
   a. CC startup messages
   b. Unit spawn notifications
   c. Tick progress messages
   d. Combat events (if any)
5. Compare with ALL.log - should match stdout portions
6. Scroll STD window if supported (arrow keys)
```

**Expected Results**:
```
- STD shows CC stdout in real-time
- Messages appear within 1 second of generation
- Text wraps correctly at window boundary
- Scrollback buffer retains recent messages
- No garbled/corrupted text
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
✅ PASS if: Output matches CC stdout, real-time updates, readable
❌ FAIL if: Missing output, garbled text, significant delay
```

**Notes**:
```
STD thread reads from FIFO at /tmp/skirmish_std.fifo.
CC uses terminal_tee to redirect stdout to both log and FIFO.
```

---

#### Test 6.4: UI Thread Synchronization

**Test Objective**:
_Verify MAP, UST, STD threads don't interfere (no screen corruption)_

**Test Setup**:
```
Scenario: fleet_battle.conf (high unit count, lots of activity)
- Map: 150×50
- Units: Multiple battleships + squadrons (20+ units)
- Duration: 200+ ticks
```

**Test Procedure**:
```
1. Start simulation: ./command_center --scenario fleet_battle &
2. Start UI: ./ui
3. Observe all three windows simultaneously for 60+ seconds:
   a. MAP window - grid updates
   b. UST window - stats updates
   c. STD window - log output
4. Look for visual artifacts:
   - Overlapping text between windows
   - Partial line renders
   - Color bleeding across windows
   - Cursor jumping/flickering
5. Resize terminal during test (if possible)
6. Press 'q' to exit cleanly
```

**Expected Results**:
```
- All three windows update independently
- No cross-window corruption
- Window borders remain intact
- Colors stay within window boundaries
- No deadlocks (UI remains responsive)
- Terminal resize handled gracefully (or ignored)
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
✅ PASS if: All windows update cleanly, no visual artifacts, responsive
❌ FAIL if: Screen corruption, frozen updates, deadlock
```

**Notes**:
```
UI uses pthread_mutex for ncurses access (not thread-safe by default).
Each thread: MAP, UST, STD runs independently.
Main thread handles input (getch) and coordinates refresh.
```

---

#### Test 6.5: UI Graceful Shutdown on IPC Destruction

**Test Objective**:
_Verify UI exits cleanly when CC destroys IPC resources (BUG-001 fix validation)_

**Test Setup**:
```
Scenario: default.conf
- External processes: UI, Console Manager
- Log: Observe UI log messages during shutdown
```

**Test Procedure**:
```
1. Start simulation: ./command_center --scenario default &
2. Start UI: ./ui
3. Start Console Manager: ./console_manager
4. Let simulation run for 20+ ticks
5. In console_manager, type: end
6. Observe UI behavior:
   a. Should NOT spam EINVAL errors
   b. Should detect IPC destruction
   c. Should log "Message queue destroyed" or "IPC resources destroyed"
   d. Should exit within 2 seconds
7. Verify with: ps aux | grep ui (should show no ui process)
```

**Expected Results**:
```
- UI detects sem_lock() EINVAL or msgrcv() EIDRM
- UI logs graceful shutdown message
- All UI threads exit cleanly
- UI process terminates with exit code 0
- No zombie processes
```

**Actual Results**:
```
✅ PASSED - Verified 2026-02-06

Log output (from logs/run_2026-02-06_04-00-35_pid138879/ALL.log):
  [UI-MAP] Message queue destroyed, exiting
  [UI-MAP] Thread exiting
  [UI] Main loop exited, waiting for threads...
  [UI-UST] IPC resources destroyed, exiting
  [UI-UST] Thread exiting
  [UI] All threads joined
  [UI] Shutdown complete

No EINVAL error spam observed.
```

**Pass/Fail Criteria**:
```
✅ PASS if: UI exits cleanly, no error spam, exit code 0
❌ FAIL if: UI hangs, spams errors, requires manual kill
```

**Notes**:
```
This test validates the BUG-001 fix implemented in:
- ui_map.c: EIDRM detection from msgrcv()
- ui_ust.c: EINVAL detection from sem_lock()
```

---

#### Test 6.6: UI Started Before CC

**Test Objective**:
_Test UI behavior when started before Command Center is running_

**Test Setup**:
```
- No CC process running
- No IPC resources exist
```

**Test Procedure**:
```
1. Ensure no CC running: pkill command_center
2. Clean IPC: ipcrm --all (if needed)
3. Start UI: ./ui
4. Observe behavior:
   a. Should fail to connect to IPC
   b. Should display error message
   c. Should exit gracefully (not hang)
5. Check exit code: echo $?
```

**Expected Results**:
```
- UI fails to attach to shared memory (shmget fails)
- Error message displayed: "Failed to connect to simulation"
- UI exits with non-zero exit code
- No crash or segfault
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
✅ PASS if: Clear error message, graceful exit
❌ FAIL if: Hang, crash, or unclear error
```

**Notes**:
```
UI should validate IPC existence before entering main loop.
```

---

### Test Category 7: Console Manager

#### Test 7.1: Spawn Command Validation

**Test Objective**:
_Test CM spawn command with valid and invalid parameters_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 7.2: CM Request-Reply Timeout

**Test Objective**:
_Test CM behavior when CC doesn't respond (timeout handling)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 7.3: End Simulation Command

**Test Objective**:
_Verify 'end' command gracefully shuts down all processes_

**Test Setup**:
```
Scenario: default.conf
- Map: 120×40
- Units: 1 Republic Carrier at (5,5) + spawned squadrons
- External Processes: UI (./ui), Console Manager (./console_manager)
- Log: logs/run_2026-02-06_03-41-53_pid134875/ALL.log
```

**Test Procedure**:
```
1. Start Command Center in background: ./command_center --scenario default &
2. Start UI in separate terminal: ./ui
3. Start Console Manager in separate terminal: ./console_manager
4. Wait for simulation to run (~90 ticks, squadrons spawned)
5. Type 'end' in console_manager
6. Observe shutdown behavior of all processes
```

**Expected Results**:
```
- CC should receive CM_CMD_END command
- CC should send SIGTERM to all unit processes
- All units should terminate cleanly
- UI should detect shutdown and exit gracefully
- All IPC resources should be cleaned up
- No orphan processes
```

**Actual Results**:
```
✅ PASSED - Verified 2026-02-06 (after BUG-001 fix)

Timeline (from logs/run_2026-02-06_04-00-35_pid138879/ALL.log):
- 04:00:57.290: CC received CM_CMD_END command type=6 from console_manager
- 04:00:57.291: CC sent "Shutdown initiated" response
- 04:00:57.375: CC started sending SIGTERM to 9 alive units
- 04:00:57.376-418: All units received SIGTERM and terminated cleanly
- 04:00:57.419: CC detached and destroyed IPC objects
- 04:00:57.420: UI-MAP detected message queue destroyed, set stop flag
- 04:00:57.422: UI-MAP thread exited, UI main loop exited
- 04:00:57.422: UI-UST detected IPC resources destroyed, set stop flag  
- 04:00:57.427: All UI threads joined
- 04:00:57.429: UI shutdown complete - no errors

All processes terminated cleanly:
  - 9 units reaped with exit status 0
  - UI detected IPC destruction and exited gracefully
  - No EINVAL error spam
  - No manual intervention required
```

**Pass/Fail Criteria**:
```
✅ PASS - UI detects shutdown and exits gracefully
✅ PASS - Unit processes terminated correctly
✅ PASS - CC cleanup sequence executed
✅ PASS - No IPC error spam
✅ PASS - CM received shutdown confirmation
❌ FAIL - Graceful multi-process shutdown not achieved
```

**Notes**:
```
BUG #1: UI Not Notified During Shutdown
========================================
Severity: MEDIUM
Component: Command Center / UI coordination

Problem:
  The UI process runs independently and is NOT in CC's child process list.
  When CC receives 'end' command, it:
  1. Sends SIGTERM to unit processes (battleship, squadron)
  2. Destroys IPC resources (semaphores, shared memory, message queues)
  3. Does NOT notify UI process

  UI continues running and tries to:
  - Acquire SEM_GLOBAL_LOCK → fails with EINVAL (semaphore destroyed)
  - Send message queue requests → fails (queue destroyed)

  Result: UI spams thousands of error messages until manually killed.

Root Cause:
  - CC only tracks unit PIDs, not UI PID
  - IPC destruction happens before UI can detect shutdown
  - UI does not handle EINVAL from sem_lock gracefully

Proposed Fixes:
  1. UI should detect EINVAL from sem_lock and exit gracefully
  2. CC should send shutdown message to UI via message queue BEFORE destroying IPC
  3. CC could broadcast SIGTERM to process group (if UI in same group)
  4. UI could poll a "shutdown" flag in shared memory

Workaround:
  - User must manually Ctrl+C the UI after using 'end' command
  - Or: Close UI before issuing 'end' command
```

---

### Test Category 8: Error Handling and Logging

#### Test 8.1: Fatal Error Termination

**Test Objective**:
_Test fatal errors cause immediate process termination with logging_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 8.2: Log File Consistency

**Test Objective**:
_Verify per-process logs and ALL.log are consistent and complete_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 8.3: Terminal Tee Output Redirection

**Test Objective**:
_Test terminal tee captures output to both terminal and log files_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

### Test Category 9: Scenarios

#### Test 9.1: Default Scenario

**Test Objective**:
_Run default scenario (4 carriers at corners) to completion_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 9.2: Asteroid Field Scenario

**Test Objective**:
_Test pathfinding around obstacles in asteroid field scenario_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 9.3: Fleet Battle Scenario

**Test Objective**:
_Test large-scale engagement (carriers, destroyers, fighters)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 9.4: Skirmish Scenario (Fighters Only)

**Test Objective**:
_Test fast-paced fighter-only combat with high unit count_

**Test Setup**:
```
Scenario: skirmish.conf (Fighter Skirmish)
- Map: 80×40
- Units: 16 fighters (8 Republic, 8 Separatist)
- Republic: Units 1-8 at (10,10) to (24,24) diagonal
- Separatist: Units 9-16 at (67,27) to (53,13) diagonal
- Log: logs/run_2026-02-06_04-14-59_pid142562/ALL.log
```

**Test Procedure**:
```
1. Start simulation: ./command_center --scenario skirmish &
2. Let battle run until most units destroyed or 200+ ticks
3. Monitor alive_units count in logs
4. Issue 'end' command via console_manager
5. Verify clean shutdown
```

**Expected Results**:
```
- All 16 fighters spawn correctly
- Fighters patrol and engage enemies within detection range (15)
- Combat produces casualties on both sides
- Battle may end with survivors or total annihilation
- Shutdown cleans up all remaining units
```

**Actual Results**:
```
✅ PASSED - Test executed 2026-02-06

Startup (04:14:59):
  - 16 units spawned successfully (8 Republic, 8 Separatist)
  - All fighters type=4 with faction colors
  - IPC: shm_id=11 sem_id=11

Combat Timeline (216 ticks, ~111 seconds):
  Tick ~14:  SQ 12 (Sep) + SQ 4 (Rep) destroyed
  Tick ~21:  SQ 16 (Sep) + SQ 5 (Rep) destroyed
  Tick ~31:  SQ 15 (Sep) + SQ 3 (Rep) destroyed
  Tick ~49:  SQ 10 (Sep) destroyed
  Tick ~53:  SQ 11 (Sep) + SQ 1 (Rep) destroyed
  Tick ~60:  SQ 9 (Sep) destroyed
  Tick ~76:  SQ 2 (Rep) destroyed
  Tick ~184: SQ 6 (Rep) destroyed
  Tick ~198: SQ 14 (Sep) destroyed

Final State (tick 216):
  - 3 survivors: SQ 7 (Rep, hp=5), SQ 8 (Rep, hp=20), SQ 13 (Sep, hp=5)
  - Republic: 2 survivors, Separatist: 1 survivor
  - Total casualties: 13/16 (81%)

Shutdown (04:16:50):
  - 'end' command received from console_manager
  - 3 children reaped with exit status 0
  - IPC resources cleaned up
```

**Pass/Fail Criteria**:
```
✅ PASS - All fighters spawned correctly
✅ PASS - Combat mechanics working (13 casualties)
✅ PASS - Clean shutdown with end command
✅ PASS - No orphan processes or IPC leaks
```

**Notes**:
```
- Fast-paced combat: first deaths at tick ~14 (7 seconds)
- Battle naturally slows after initial clash (survivors separated)
- Final 3 survivors too far apart to detect each other (dt2 > 225)
- Republic won 2-1 but both factions took heavy losses
```

---

### Test Category 10: Performance and Stress Testing

#### Test 10.1: Maximum Unit Count (64 units)

**Test Objective**:
_Spawn 64 units and verify simulation remains stable_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 10.2: Tick Latency Measurement

**Test Objective**:
_Measure average tick processing time with varying unit counts (10, 30, 50, 64 units)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 10.3: Memory Usage Analysis

**Test Objective**:
_Monitor shared memory and process memory usage over 1000 ticks_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 10.4: Long-Running Simulation Stability

**Test Objective**:
_Run simulation for 10,000 ticks and verify no memory leaks or crashes_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

### Test Category 11: Edge Cases and Failure Modes

#### Test 11.1: Flagship Destruction Impact

**Test Objective**:
_Verify system behavior when Flagship is destroyed (units become autonomous)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 11.2: Spawn on Occupied Cell

**Test Objective**:
_Test spawn command rejects placement on occupied grid cells_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 11.3: Out of Bounds Movement

**Test Objective**:
_Verify units cannot move outside map boundaries (0-119, 0-39)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 11.4: No Conflict Simulation

**Test Objective**:
_Run simulation with no enemies (units should patrol without combat)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

### Test Category 12: Regression Testing

#### Test 12.1: Same Units Both Sides

**Test Objective**:
_Battle with identical units on both factions (symmetry test)_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

#### Test 12.2: Rapid Spawn and Kill

**Test Objective**:
_Stress test by rapidly spawning and killing units via CM_

**Test Setup**:
```
[TO FILL IN]
```

**Test Procedure**:
```
[TO FILL IN]
```

**Expected Results**:
```
[TO FILL IN]
```

**Actual Results**:
```
[TO FILL IN DURING TESTING]
```

**Pass/Fail Criteria**:
```
[TO FILL IN]
```

**Notes**:
```
[TO FILL IN]
```

---

## Build and Deployment

### Build System

**[Makefile](https://github.com/PaurXen/Space-Skirmish-/blob/main/Makefile) Targets:**

```makefile
# Build all binaries
make all

# Clean build artifacts
make clean

# Build individual components
make command_center
make console_manager
make ui
make battleship
make squadron
```

**Dependencies:**
- GCC compiler (C11 standard)
- ncurses library (for UI)
- POSIX-compliant system (Linux)
- System V IPC support

**Compilation Flags:**
```
-O2              # Optimization level 2
-Wall -Wextra    # Enable warnings
-std=c11         # C11 standard
-Iinclude        # Include directory
-lpthread        # Link pthread (for UI and CC)
-lncurses        # Link ncurses (for UI)
-lm              # Link math library (for squadron)
```

### Deployment Structure

```
Space-Skirmish/
├── command_center       # Main orchestrator binary
├── console_manager      # Interactive CLI binary
├── ui                   # ncurses UI binary
├── battleship           # Heavy unit binary
├── squadron             # Light unit binary
├── scenarios/           # Configuration files
│   ├── default.conf
│   ├── fleet_battle.conf
│   └── ...
├── logs/                # Log output directory
│   └── run_YYYY-MM-DD_HH-MM-SS_pidXXXXX/
│       ├── ALL.log
│       ├── ALL.term.log
│       ├── CC.log
│       ├── UI.log
│       └── ...
├── include/             # Header files
├── src/                 # Source files
└── docs/                # Documentation
```

---

## Usage Guide

### Quick Start

**1. Build the project:**
```bash
make all
```

**2. Start Command Center (Terminal 1):**
```bash
./command_center
# Or with specific scenario:
./command_center --scenario fleet_battle
```

**3. Start User Interface (Terminal 2 - optional):**
```bash
./ui
```

**4. Start Console Manager (Terminal 3 - optional):**
```bash
./console_manager
```

### Command Center Options

```bash
./command_center [OPTIONS]

Options:
  --scenario <name>    Load scenario from scenarios/<name>.conf
                       Default: scenarios/default.conf
  --help               Show help message
```

### Console Manager Commands

```
freeze / f                      - Pause simulation
unfreeze / uf                   - Resume simulation
tickspeed [ms] / ts             - Get/set tick speed
grid [on|off] / g               - Toggle grid display
spawn <type> <faction> <x> <y>  - Spawn unit
  Types: carrier, destroyer, flagship, fighter, bomber, elite
  Factions: republic, cis
end                             - End simulation
quit / exit                     - Exit console manager
help                            - Show help
```

### UI Keyboard Controls

```
q       - Quit UI
r       - Refresh display
g       - Toggle grid
↑↓←→    - Scroll MAP window (if implemented)
```

### Log Files

Logs are stored in timestamped directories:

```bash
logs/run_2026-02-05_14-30-00_pid12345/
```

**View logs:**
```bash
# Real-time monitoring
tail -f logs/run_*/ALL.log

# View specific process log
cat logs/run_*/CC.log

# View terminal output
cat logs/run_*/ALL.term.log
```

---

## Performance Analysis

### Expected Performance Metrics

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| Tick Latency (10 units) | < 10ms | `LOGD` timestamps |
| Tick Latency (64 units) | < 50ms | `LOGD` timestamps |
| Memory Usage (shared) | ~1MB | `ipcs -m` |
| Memory Usage (per process) | < 10MB | `ps aux` |
| Message Queue Depth | < 20 | `ipcs -q` |
| Semaphore Operations/tick | ~200 | `strace -c` |

### Profiling Tools

**1. Tick Timing:**
```bash
grep "Tick.*completed in" logs/run_*/CC.log | awk '{print $NF}' | statistics.py
```

**2. Memory Usage:**
```bash
watch -n 1 "ps aux | grep -E 'command_center|battleship|squadron'"
```

**3. IPC Status:**
```bash
watch -n 1 "ipcs -a"
```

**4. System Call Tracing:**
```bash
strace -c -p $(pgrep command_center)
```

### Bottleneck Analysis

**Common Bottlenecks:**
1. Semaphore contention (SEM_GLOBAL_LOCK)
2. Message queue send/receive latency
3. A* pathfinding computation
4. ncurses rendering in UI

**Optimization Strategies:**
- Fine-grained locking (per-region instead of global)
- Message batching for damage
- Pathfinding caching
- UI throttling (render every N ticks)

---

## Future Enhancements

### Planned Features

1. **Enhanced Commander System**
   - Formation flying (wedge, line, box)
   - Group tactics and flanking maneuvers

2. **Resource Management**
   - Energy depletion and recharge for squadrons
   - Repair and resupply mechanics

3. **Advanced AI**
   - Threat assessment and retreat logic
   - Dynamic target prioritization
   - Evasive maneuvers

4. **Expanded Factions**
   - Add Neutral, Mercenary factions
   - Faction-specific unit abilities

5. **Special Abilities**
   - Cloaking for stealth units
   - Shield boost for defensive play
   - EMP weapons to disable enemies

6. **Victory Conditions**
   - Time-based objectives
   - Capture-the-flag zones
   - Resource accumulation goals

7. **Multiplayer Support**
   - Network communication for remote players
   - Spectator mode

8. **Enhanced UI**
   - Minimap with fog of war
   - Detailed unit inspection panel
   - Combat statistics and graphs

### Technical Improvements

1. **Performance Optimization**
   - Lock-free data structures for grid
   - Message queue pooling
   - GPU-accelerated pathfinding (OpenCL)

2. **Testing Infrastructure**
   - Automated test suite with CI/CD
   - Performance regression testing
   - Fuzzing for IPC robustness

3. **Documentation**
   - Interactive API documentation (Doxygen)
   - Video tutorials and demos
   - Scenario creation guide

---

## References

### Internal Documentation

- [Command Center Module](CC_MODULE.md) | [GitHub](https://github.com/PaurXen/Space-Skirmish-/blob/main/docs/CC_MODULE.md)
- [IPC Module](IPC_MODULE.md) | [GitHub](https://github.com/PaurXen/Space-Skirmish-/blob/main/docs/IPC_MODULE.md)
- [User Interface Module](UI_MODULE.md) | [GitHub](https://github.com/PaurXen/Space-Skirmish-/blob/main/docs/UI_MODULE.md)
- [Console Manager Module](CM_MODULE.md) | [GitHub](https://github.com/PaurXen/Space-Skirmish-/blob/main/docs/CM_MODULE.md)
- [Error Handling, Logging, and Terminal Tee](ERROR_LOG_TEE.md) | [GitHub](https://github.com/PaurXen/Space-Skirmish-/blob/main/docs/ERROR_LOG_TEE.md)
- [Scenarios README](../scenarios/README.md) | [GitHub](https://github.com/PaurXen/Space-Skirmish-/blob/main/scenarios/README.md)
- [Project README (Polish)](../README.md) | [GitHub](https://github.com/PaurXen/Space-Skirmish-/blob/main/README.md)

### External Resources

**System V IPC:**
- [shmget(2)](https://man7.org/linux/man-pages/man2/shmget.2.html) - Shared memory allocation
- [semget(2)](https://man7.org/linux/man-pages/man2/semget.2.html) - Semaphore allocation
- [msgget(2)](https://man7.org/linux/man-pages/man2/msgget.2.html) - Message queue allocation
- [ipcs(1)](https://man7.org/linux/man-pages/man1/ipcs.1.html) - IPC resource reporting

**Process Management:**
- [fork(2)](https://man7.org/linux/man-pages/man2/fork.2.html) - Process creation
- [execl(3)](https://man7.org/linux/man-pages/man3/exec.3.html) - Process execution
- [waitpid(2)](https://man7.org/linux/man-pages/man2/waitpid.2.html) - Process synchronization
- [kill(2)](https://man7.org/linux/man-pages/man2/kill.2.html) - Signal sending

**Threading:**
- [pthread_create(3)](https://man7.org/linux/man-pages/man3/pthread_create.3.html)
- [pthread_mutex(3)](https://man7.org/linux/man-pages/man3/pthread_mutex_lock.3p.html)

**ncurses:**
- [ncurses(3X)](https://linux.die.net/man/3/ncurses) - Terminal UI library
- [ncurses Programming Guide](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/)

**Algorithms:**
- [A* Pathfinding](https://en.wikipedia.org/wiki/A*_search_algorithm)
- [Barrier Synchronization](https://en.wikipedia.org/wiki/Barrier_(computer_science))

---

## Appendices

### Appendix A: Error Codes Reference

See [ERROR_LOG_TEE.md](ERROR_LOG_TEE.md) for complete error code listing.

### Appendix B: Message Queue Protocol Specifications

See [IPC_MODULE.md](IPC_MODULE.md) for detailed message formats.

### Appendix C: Unit Statistics Tables

See [CC_MODULE.md](CC_MODULE.md) for complete unit and weapon statistics.

### Appendix D: Scenario Configuration Format

See [scenarios/README.md](../scenarios/README.md) for configuration syntax.

### Appendix E: Known Bugs

#### BUG-001: UI Not Notified During Shutdown (RESOLVED)

| Field | Value |
|-------|-------|
| **Severity** | MEDIUM |
| **Component** | Command Center / UI coordination |
| **Discovered** | 2026-02-06 |
| **Resolved** | 2026-02-06 |
| **Test Reference** | Test 7.3: End Simulation Command |
| **Log Reference** | `logs/run_2026-02-06_03-41-53_pid134875/ALL.log` |

**Description:**
When the `end` command is issued via Console Manager, the Command Center destroys IPC resources (semaphores, shared memory, message queues) before notifying the UI process. The UI continues running and attempts to access the destroyed IPC resources, resulting in thousands of `EINVAL` (errno=22) errors.

**Symptoms (before fix):**
- UI spams error messages: `sem_lock(...) - Invalid argument (errno=22)`
- UI spams warnings: `[UI-MAP] Failed to send request`
- UI does not exit automatically
- User must manually Ctrl+C to kill UI

**Root Cause:**
1. CC only tracks unit process PIDs in its child list, not UI PID
2. CC destroys IPC resources immediately after sending SIGTERM to units
3. UI had no mechanism to detect that IPC resources are gone
4. UI's `sem_lock()` returned EINVAL but UI didn't exit on this error

**Resolution:**
Implemented "UI graceful EINVAL/EIDRM handling" in both UI threads:

1. **[ui_map.c](../src/UI/ui_map.c)**: 
   - Detect `EINVAL` from `sem_lock()` → set `stop = 1` and exit loop
   - Detect `EIDRM` from `msgrcv()` → log "Message queue destroyed" and exit

2. **[ui_ust.c](../src/UI/ui_ust.c)**:
   - Detect `EINVAL` from `sem_lock()` → set `stop = 1` and exit loop

**Verified shutdown sequence (from logs):**
```
[UI-MAP] Message queue destroyed, exiting
[UI-MAP] Thread exiting
[UI] Main loop exited, waiting for threads...
[UI-UST] IPC resources destroyed, exiting
[UI-UST] Thread exiting
[UI] All threads joined
[UI] Shutdown complete
```

**Test Result:** Test 7.3 now passes - UI exits gracefully when `end` command is issued

---

**Document Version:** 1.1  
**Last Updated:** 2026-02-06  
**Authors:** Space Skirmish Development Team  
**Project Repository:** [GitHub - Space Skirmish](https://github.com/PaurXen/Space-Skirmish-)

---

## Test Summary Template

After completing all tests, use this template to summarize results:

### Overall Test Results

**Total Tests:** [TO FILL IN]  
**Passed:** [TO FILL IN]  
**Failed:** [TO FILL IN]  
**Pass Rate:** [TO FILL IN]%

### Critical Issues Found

1. [TO FILL IN]
2. [TO FILL IN]
3. [TO FILL IN]

### Recommended Fixes

1. [TO FILL IN]
2. [TO FILL IN]
3. [TO FILL IN]

### Performance Summary

| Metric | Target | Achieved | Status |
|--------|--------|----------|--------|
| Tick Latency (10 units) | < 10ms | [TO FILL IN] | [TO FILL IN] |
| Tick Latency (64 units) | < 50ms | [TO FILL IN] | [TO FILL IN] |
| Memory Usage | < 1MB (shm) | [TO FILL IN] | [TO FILL IN] |
| Long-run Stability | 10000 ticks | [TO FILL IN] | [TO FILL IN] |

### Conclusion

[TO FILL IN: Overall assessment of project quality, readiness for deployment, and remaining work]

---

**End of Document**
