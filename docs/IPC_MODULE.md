# IPC Module Documentation

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Components](#components)
4. [IPC Mechanisms](#ipc-mechanisms)
5. [Message Protocol](#message-protocol)
6. [Synchronization](#synchronization)
7. [Data Structures](#data-structures)
8. [API Reference](#api-reference)
9. [Usage Examples](#usage-examples)

---

## Overview

The **IPC (Inter-Process Communication) Module** provides the foundation for multi-process communication in the Space Skirmish simulation. It implements a comprehensive System V IPC framework using **shared memory**, **semaphores**, and **message queues** to coordinate between the Command Center, unit processes, Console Manager, and UI.

### Key Responsibilities
- **Shared Memory Management**: Central game state accessible by all processes
- **Synchronization**: Semaphore-based mutual exclusion and barrier synchronization
- **Message Passing**: Asynchronous communication via message queues
- **Context Management**: Create, attach, detach, and cleanup IPC resources
- **Error Handling**: Robust EINTR handling and cooperative cancellation

---

## Architecture

### IPC Resource Hierarchy

```
IPC Resources (System V)
├── Shared Memory Segment
│   ├── Grid State (M×N unit_id_t array)
│   ├── Unit Entities (MAX_UNITS)
│   ├── Tick Counter
│   └── Synchronization Bookkeeping
│
├── Semaphore Set (3 semaphores)
│   ├── SEM_GLOBAL_LOCK (mutex for shared memory)
│   ├── SEM_TICK_START (barrier: CC → units)
│   └── SEM_TICK_DONE (barrier: units → CC)
│
└── Message Queues (2 queues)
    ├── q_req (requests: damage, commands, spawn, orders)
    └── q_rep (replies: spawn results, command responses)
```

### Process Communication Model

```
┌──────────────────┐
│ Command Center   │
│   (CC Owner)     │
└────────┬─────────┘
         │ Creates IPC
         │
    ┌────▼─────────────────────┐
    │  Shared Memory Segment   │
    │  - Grid: M×N             │
    │  - Units: MAX_UNITS      │
    │  - Tick state            │
    └────┬─────────────────────┘
         │ Attached by all
    ┌────┴────┬────────┬────────┬────────┐
    │         │        │        │        │
┌───▼───┐ ┌──▼──┐ ┌───▼───┐ ┌──▼──┐ ┌───▼───┐
│ BS #1 │ │BS #2│ │ SQ #1 │ │ CM  │ │  UI   │
└───┬───┘ └──┬──┘ └───┬───┘ └──┬──┘ └───┬───┘
    │        │        │        │        │
    └────────┴────────┴────────┴────────┘
              Message Queues
          (damage, orders, commands)
```

---

## Components

### 1. ipc_context.c

**IPC resource lifecycle management** - Create, attach, and destroy IPC objects.

**Location**: `src/ipc/ipc_context.c`

**Key Functions**:

#### `ipc_create()`
Creates or resets IPC resources for a fresh simulation run.

**Responsibilities**:
- Create ftok key file if missing
- Generate keys via `ftok(3)` with project IDs: 'S' (SHM), 'M' (SEM), 'Q'/'R' (MQ)
- Create semaphore set with 3 semaphores
- Initialize semaphore values: `GLOBAL_LOCK=1`, `TICK_START=0`, `TICK_DONE=0`
- Create shared memory segment (`sizeof(shm_state_t)`)
- Attach shared memory
- Reset shared memory under `SEM_GLOBAL_LOCK`
- Create/reset message queues

**Error Handling**: Returns 0 on success, -1 on error with errno set.

```c
ipc_ctx_t ctx;
if (ipc_create(&ctx, "./ipc.key") == -1) {
    perror("ipc_create");
    exit(1);
}
// ctx.S now points to initialized shared state
```

#### `ipc_attach()`
Attach to existing IPC objects created by Command Center.

**Responsibilities**:
- Generate same keys via ftok
- Attach to existing semaphore set
- Attach to existing shared memory
- Verify magic number (`SHM_MAGIC`)
- Attach to message queues

**Error Handling**: Returns -1 if IPC not initialized or magic mismatch.

```c
ipc_ctx_t ctx;
if (ipc_attach(&ctx, "./ipc.key") == -1) {
    fprintf(stderr, "Failed to attach to IPC\n");
    exit(1);
}
```

#### `ipc_detach()`
Detach from shared memory without removing IPC objects.

**Safe for**: All processes (CC, units, CM, UI)

```c
ipc_detach(&ctx);
// ctx.S is now invalid, but IPC objects remain for other processes
```

#### `ipc_destroy()`
Remove IPC objects from the system.

**⚠️ Owner Only**: Should only be called by Command Center during cleanup.

**Removes**:
- Shared memory segment (`shmctl IPC_RMID`)
- Semaphore set (`semctl IPC_RMID`)
- Message queues (`msgctl IPC_RMID`)

```c
if (ctx.owner) {
    ipc_destroy(&ctx);
}
```

---

### 2. semaphores.c

**Semaphore operations** - Wrappers around System V `semop(2)` with EINTR handling.

**Location**: `src/ipc/semaphores.c`

**Key Functions**:

#### `sem_op_retry()`
Perform semaphore operation, retrying on `EINTR`.

**Use Case**: When interruption is not a concern (e.g., quick unlock operations).

```c
struct sembuf op = {.sem_num = SEM_GLOBAL_LOCK, .sem_op = -1, .sem_flg = 0};
if (sem_op_retry(sem_id, &op, 1) == -1) {
    perror("sem_op_retry");
}
```

#### `sem_op_intr()`
Perform semaphore operation with **cooperative cancellation** support.

**Use Case**: Tick barrier synchronization where units need to exit cleanly on `SIGTERM`.

**Cooperative Cancellation**:
- If `stop_flag` is non-NULL and `*stop_flag != 0`, returns -1 with `errno=EINTR`
- Allows units to exit cleanly from blocking `semop()` calls
- **Critical**: Checks `stop_flag` BEFORE blocking to avoid race conditions

```c
volatile sig_atomic_t g_stop = 0;  // Set by SIGTERM handler

// In tick loop
if (sem_wait_intr(sem_id, SEM_TICK_START, -1, &g_stop) == -1) {
    if (errno == EINTR && g_stop) {
        // Clean exit
        break;
    }
    perror("sem_wait_intr");
}
```

#### `sem_lock()` / `sem_unlock()`
Convenience wrappers for mutex-style locking.

```c
// Acquire global lock
if (sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK) == -1) {
    perror("sem_lock");
    return -1;
}

// Critical section: modify shared memory
ctx->S->units[unit_id].hp -= damage;

// Release lock
if (sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK) == -1) {
    perror("sem_unlock");
}
```

#### `sem_lock_intr()`
Interruptible lock acquisition.

**Use Case**: Acquire lock with ability to cancel on shutdown signal.

```c
if (sem_lock_intr(ctx->sem_id, SEM_GLOBAL_LOCK, &g_stop) == -1) {
    if (errno == EINTR && g_stop) {
        return;  // Exit cleanly
    }
}
```

#### `sem_post_retry()`
Post (increment) semaphore, retrying on EINTR.

```c
// Signal tick done
sem_post_retry(ctx->sem_id, SEM_TICK_DONE, 1);
```

---

### 3. ipc_mesq.c

**Message queue operations** - Asynchronous message passing between processes.

**Location**: `src/ipc/ipc_mesq.c`

**Message Types**:

| Message | Direction | Purpose |
|---------|-----------|---------|
| `MSG_SPAWN` | BS/CM → CC | Request new unit spawn |
| `MSG_COMMANDER_REQ` | SQ → BS | Squadron requests commander |
| `MSG_COMMANDER_REP` | BS → SQ | Commander assignment response |
| `MSG_DAMAGE` | Unit → Unit | Combat damage notification |
| `MSG_ORDER` | BS → SQ | Commander orders to squadron |
| `MSG_CM_CMD` | CM → CC | Console Manager commands |
| `MSG_UI_MAP_REQ` | UI → CC | Request map snapshot |
| `MSG_UI_MAP_REP` | CC → UI | Map snapshot response |

**Key Functions**:

#### Damage Messages
```c
// Send damage to target
mq_damage_t dmg_msg = {
    .mtype = target_pid,
    .target_id = target_unit_id,
    .damage = 150
};
mq_send_damage(ctx->q_req, &dmg_msg);

// Receive damage (non-blocking)
mq_damage_t dmg;
while (mq_try_recv_damage(ctx->q_req, &dmg) == 1) {
    total_damage += dmg.damage;
}
```

#### Commander Request/Reply
```c
// Squadron requests commander
mq_commander_req_t req = {
    .mtype = MSG_COMMANDER_REQ,
    .sender = getpid(),
    .sender_id = unit_id,
    .req_id = request_counter++
};
mq_send_commander_req(ctx->q_req, &req);

// Battleship receives and responds
mq_commander_req_t cmd_req;
if (mq_try_recv_commander_req(ctx->q_req, &cmd_req) == 1) {
    mq_commander_rep_t reply = {
        .mtype = cmd_req.sender,
        .req_id = cmd_req.req_id,
        .status = 0,  // 0 = accepted, -1 = rejected
        .commander_id = my_unit_id
    };
    mq_send_commander_reply(ctx->q_rep, &reply);
}
```

#### Order Messages
```c
// Battleship sends order to squadron
mq_order_t order_msg = {
    .mtype = squadron_pid,
    .order = ATTACK,
    .target_id = enemy_unit_id
};
mq_send_order(ctx->q_req, &order_msg);

// Squadron receives order
mq_order_t order;
if (mq_try_recv_order(ctx->q_req, &order) == 1) {
    current_order = order.order;
    target = order.target_id;
}
```

#### Console Manager Commands
```c
// CM sends command to CC
mq_cm_cmd_t cmd = {
    .mtype = MSG_CM_CMD,
    .cmd = CM_CMD_SPAWN,
    .sender = getpid(),
    .req_id = req_counter++,
    .spawn_type = TYPE_FIGHTER,
    .spawn_faction = FACTION_REPUBLIC,
    .spawn_x = 50,
    .spawn_y = 50
};
mq_send_cm_cmd(ctx->q_req, &cmd);

// CC receives and processes
mq_cm_cmd_t cmd;
if (mq_try_recv_cm_cmd(ctx->q_req, &cmd) == 1) {
    handle_cm_command(&cmd);
    
    // Send reply
    mq_cm_rep_t reply = {
        .mtype = cmd.sender,
        .req_id = cmd.req_id,
        .status = 0,
        .message = "Unit spawned successfully"
    };
    mq_send_cm_reply(ctx->q_rep, &reply);
}
```

---

### 4. shared.h

**Shared memory data structures** - Central game state definition.

**Location**: `include/ipc/shared.h`

**Key Definitions**:

#### Grid Configuration
```c
#define M 120           // Grid width
#define N 40            // Grid height
#define OBSTACLE_MARKER -2
#define MAX_UNITS 64
```

#### Enumerations
```c
typedef enum {
    NONE = 0, LR_CANNON = 1, MR_CANNON = 2, SR_CANNON = 3,
    LR_GUN = 4, MR_GUN = 5, SR_GUN = 6
} weapon_type_t;

typedef enum {
    DO_NOTHING = 0, PATROL = 1, ATTACK = 2,
    MOVE = 3, MOVE_ATTACK = 4, GUARD = 5
} unit_order_t;

typedef enum {
    FACTION_NONE = 0, FACTION_REPUBLIC = 1, FACTION_CIS = 2
} faction_t;

typedef enum {
    DUMMY = 0, TYPE_FLAGSHIP = 1, TYPE_DESTROYER = 2,
    TYPE_CARRIER = 3, TYPE_FIGHTER = 4,
    TYPE_BOMBER = 5, TYPE_ELITE = 6
} unit_type_t;
```

#### Unit Entity
```c
typedef struct {
    pid_t pid;              // Process ID for signaling
    uint8_t faction;        // faction_t
    uint8_t type;           // unit_type_t
    uint8_t alive;          // 1 = alive, 0 = dead
    point_t position;       // Grid coordinates (center)
    uint32_t flags;         // Reserved for status/orders
    st_points_t dmg_payload; // Accumulated damage (deprecated)
} unit_entity_t;
```

#### Global Shared State
```c
typedef struct {
    uint32_t magic;         // SHM_MAGIC (0x53504143 "SPAC")
    uint32_t ticks;         // Global tick counter
    uint16_t next_unit_id;  // Next available unit ID
    uint16_t unit_count;    // Active unit count
    
    // Tick barrier bookkeeping
    uint16_t tick_expected; // Expected units this tick
    uint16_t tick_done;     // Completed units this tick
    uint32_t last_step_tick[MAX_UNITS+1]; // Per-unit last tick
    
    unit_id_t grid[M][N];   // Grid state (0 = empty, -2 = obstacle)
    unit_entity_t units[MAX_UNITS+1]; // Unit entities (index 0 unused)
} shm_state_t;
```

**Magic Number**: `0x53504143u` ("SPAC" in ASCII)
- Used to verify shared memory is properly initialized
- Checked by `ipc_attach()` before use

---

## IPC Mechanisms

### Shared Memory

**Purpose**: Central game state accessible by all processes.

**Lifecycle**:
1. **Creation** (CC): `shmget()` with `IPC_CREAT | 0600`
2. **Attachment** (All): `shmat()` to map into process address space
3. **Access**: Direct read/write to `ctx->S->...` (protected by semaphores)
4. **Detachment**: `shmdt()` when process exits
5. **Removal** (CC): `shmctl(IPC_RMID)` during cleanup

**Key Operations**:
```c
// Read grid cell
unit_id_t occupant = ctx->S->grid[x][y];

// Update unit HP (requires lock)
sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
ctx->S->units[unit_id].hp -= damage;
sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);

// Increment tick counter
ctx->S->ticks++;
```

---

### Semaphores

**Purpose**: Synchronization and mutual exclusion.

**Semaphore Set** (3 semaphores):

#### `SEM_GLOBAL_LOCK` (Index 0)
- **Type**: Binary semaphore (mutex)
- **Initial Value**: 1
- **Purpose**: Protect shared memory modifications
- **Usage**: Acquire before writing to `ctx->S`, release after

**Critical Sections**:
- Grid updates (movement)
- Unit stat changes (HP, shields)
- Unit spawn/death
- Tick counter increment

```c
sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
// Critical section: modify shared state
ctx->S->units[id].position = new_pos;
ctx->S->grid[new_x][new_y] = id;
sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
```

#### `SEM_TICK_START` (Index 1)
- **Type**: Counting semaphore
- **Initial Value**: 0
- **Purpose**: Barrier synchronization - CC signals units to start tick
- **Protocol**:
  1. CC posts N permits (N = alive units)
  2. Each unit waits once (`sem_wait`)
  3. Unit executes tick logic

```c
// CC side
for (int i = 0; i < alive_count; i++) {
    sem_post_retry(ctx->sem_id, SEM_TICK_START, 1);
}

// Unit side
sem_wait_intr(ctx->sem_id, SEM_TICK_START, -1, &g_stop);
// Execute tick logic
```

#### `SEM_TICK_DONE` (Index 2)
- **Type**: Counting semaphore
- **Initial Value**: 0
- **Purpose**: Barrier synchronization - units signal tick completion to CC
- **Protocol**:
  1. Each unit posts when done (`sem_post`)
  2. CC waits N times to collect all signals
  3. CC proceeds to next tick

```c
// Unit side
sem_post_retry(ctx->sem_id, SEM_TICK_DONE, 1);

// CC side
for (int i = 0; i < alive_count; i++) {
    sem_wait_intr(ctx->sem_id, SEM_TICK_DONE, -1, &g_stop);
}
```

---

### Message Queues

**Purpose**: Asynchronous inter-process communication.

**Two Queues**:
1. **`q_req`** (Requests): Commands, damage, orders, spawn requests
2. **`q_rep`** (Replies): Responses to requests

**Message Routing via `mtype`**:
- **Target PID**: Messages routed to specific process (e.g., damage to target)
- **Message Type**: Broadcast messages (e.g., `MSG_CM_CMD`)
- **Offset**: Orders use `PID + MQ_ORDER_MTYPE_OFFSET` to avoid collisions

**Non-Blocking Receive**:
```c
int result = mq_try_recv_damage(ctx->q_req, &dmg);
// result:  1 = message received
//          0 = no message (ENOMSG)
//         -1 = error
```

**Blocking Receive**:
```c
mq_recv_cm_reply_blocking(ctx->q_rep, &reply);
// Blocks until reply received
```

---

## Message Protocol

### Damage Protocol

**Flow**: Attacker → Target

```
1. Attacker calculates damage
2. Attacker sends damage message to target's PID
3. Attacker signals target: kill(target_pid, SIGRTMAX)
4. Target's signal handler sets g_damage_pending flag
5. Target processes damage on next tick
6. Target applies damage to shields/HP
```

**Code**:
```c
// Attacker
mq_damage_t dmg = {
    .mtype = target_pid,
    .target_id = target_unit_id,
    .damage = calculated_damage
};
mq_send_damage(ctx->q_req, &dmg);
kill(target_pid, SIGRTMAX);

// Target (in tick loop)
if (g_damage_pending) {
    g_damage_pending = 0;
    compute_dmg_payload(ctx, unit_id, &st);
}
```

---

### Commander Protocol

**Flow**: Squadron → Battleship → Squadron

```
1. Squadron sends MSG_COMMANDER_REQ with its PID and unit_id
2. Battleship receives request
3. Battleship checks capacity (underlings array)
4. Battleship sends MSG_COMMANDER_REP (status 0=accept, -1=reject)
5. Squadron receives reply and updates commander ID
```

**Code**:
```c
// Squadron
static uint32_t req_counter = 0;
mq_commander_req_t req = {
    .mtype = MSG_COMMANDER_REQ,
    .sender = getpid(),
    .sender_id = my_unit_id,
    .req_id = req_counter++
};
mq_send_commander_req(ctx->q_req, &req);

// Wait for reply
mq_commander_rep_t reply;
while (mq_try_recv_commander_reply(ctx->q_rep, &reply) == 1) {
    if (reply.req_id == req.req_id && reply.status == 0) {
        commander = reply.commander_id;
        break;
    }
}
```

---

### Order Protocol

**Flow**: Battleship (Commander) → Squadron (Underling)

```
1. Battleship decides on order (PATROL, ATTACK, GUARD)
2. Battleship sends MSG_ORDER with target coordinates
3. Squadron receives order
4. Squadron updates current_order and target
```

**Message**:
```c
typedef struct {
    long mtype;           // Squadron PID + MQ_ORDER_MTYPE_OFFSET
    unit_order_t order;   // PATROL, ATTACK, GUARD
    unit_id_t target_id;  // Target for ATTACK/GUARD
} mq_order_t;
```

---

### Console Manager Protocol

**Flow**: CM → CC → CM

**Commands**:
- `CM_CMD_SPAWN`: Spawn new unit
- `CM_CMD_FREEZE`: Pause simulation
- `CM_CMD_UNFREEZE`: Resume simulation
- `CM_CMD_TICKSPEED_SET`: Change tick speed
- `CM_CMD_TICKSPEED_GET`: Query tick speed
- `CM_CMD_GRID`: Toggle grid display

**Request/Reply**:
```c
// CM sends command
mq_cm_cmd_t cmd = {
    .mtype = MSG_CM_CMD,
    .cmd = CM_CMD_TICKSPEED_SET,
    .sender = getpid(),
    .req_id = req_id++,
    .tick_speed_ms = 500
};
mq_send_cm_cmd(ctx->q_req, &cmd);

// CC receives and processes
mq_cm_cmd_t cmd;
if (mq_try_recv_cm_cmd(ctx->q_req, &cmd) == 1) {
    switch (cmd.cmd) {
        case CM_CMD_TICKSPEED_SET:
            g_tick_speed_ms = cmd.tick_speed_ms;
            // Send success reply
            break;
        // ... other commands
    }
}

// CM receives reply (blocking)
mq_cm_rep_t reply;
mq_recv_cm_reply_blocking(ctx->q_rep, &reply);
printf("Result: %s\n", reply.message);
```

---

## Synchronization

### Tick Barrier Algorithm

**Goal**: Synchronize all units to advance simulation in lockstep.

**Algorithm**:

```
repeat:
    1. CC counts alive units → N
    2. CC posts SEM_TICK_START N times
    3. Each unit waits on SEM_TICK_START (consumes 1 permit)
    4. Each unit executes tick logic
    5. Each unit posts SEM_TICK_DONE
    6. CC waits on SEM_TICK_DONE N times (collects N permits)
    7. CC increments tick counter
    goto repeat
```

**Cooperative Cancellation**:
- Units use `sem_wait_intr()` with `&g_stop` flag
- On `SIGTERM`, handler sets `g_stop = 1`
- Next `sem_wait_intr()` returns -1 with `errno=EINTR`
- Unit breaks tick loop and cleans up

**Code**:
```c
// Command Center tick barrier
void tick_barrier(ipc_ctx_t *ctx) {
    int alive = count_alive_units(ctx);
    
    // Signal all units to start
    for (int i = 0; i < alive; i++) {
        sem_post_retry(ctx->sem_id, SEM_TICK_START, 1);
    }
    
    // Wait for all to complete
    for (int i = 0; i < alive; i++) {
        if (sem_wait_intr(ctx->sem_id, SEM_TICK_DONE, -1, &g_stop) == -1) {
            if (errno == EINTR && g_stop) break;
        }
    }
    
    ctx->S->ticks++;
}

// Unit tick loop
while (!g_stop) {
    if (sem_wait_intr(ctx->sem_id, SEM_TICK_START, -1, &g_stop) == -1) {
        if (errno == EINTR && g_stop) break;
        perror("sem_wait_intr");
        break;
    }
    
    // Execute tick logic
    process_damage();
    scan_radar();
    move_toward_target();
    fire_weapons();
    
    sem_post_retry(ctx->sem_id, SEM_TICK_DONE, 1);
}
```

---

### Critical Section Protection

**Pattern**:
```c
sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);

// Critical section: ONLY shared memory modifications
ctx->S->units[id].hp -= damage;
if (ctx->S->units[id].hp <= 0) {
    ctx->S->units[id].alive = 0;
    ctx->S->grid[x][y] = 0;
}

sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
```

**Guidelines**:
- ✅ **Lock**: Grid updates, unit stat changes, spawn/death
- ❌ **Don't Lock**: Message queue operations (already atomic)
- ❌ **Don't Lock**: Local variables, file I/O, computation
- ⚠️ **Minimize**: Hold lock as briefly as possible

---

## Data Structures

### `ipc_ctx_t`

**Purpose**: Runtime context for IPC resources.

```c
typedef struct {
    int shm_id;           // Shared memory ID
    int sem_id;           // Semaphore set ID
    int q_req;            // Request queue ID
    int q_rep;            // Reply queue ID
    shm_state_t *S;       // Attached shared memory
    int owner;            // 1 if creator (CC), 0 otherwise
    char ftok_path[256];  // ftok key file path
} ipc_ctx_t;
```

**Lifecycle**:
```c
ipc_ctx_t ctx;

// CC: Create
ipc_create(&ctx, "./ipc.key");
ctx.owner == 1;  // true

// Units: Attach
ipc_attach(&ctx, "./ipc.key");
ctx.owner == 0;  // true

// All: Detach
ipc_detach(&ctx);

// CC only: Destroy
if (ctx.owner) {
    ipc_destroy(&ctx);
}
```

---

### `shm_state_t`

**Purpose**: Global simulation state in shared memory.

**Size**: ~40KB (120×40 grid + 64 units)

**Layout**:
```c
typedef struct {
    uint32_t magic;         // 0x53504143 verification
    uint32_t ticks;         // Simulation tick counter
    uint16_t next_unit_id;  // Allocator (starts at 1)
    uint16_t unit_count;    // Active units
    
    // Barrier bookkeeping (optional)
    uint16_t tick_expected;
    uint16_t tick_done;
    uint32_t last_step_tick[MAX_UNITS+1];
    
    unit_id_t grid[M][N];   // Grid: 120×40 = 4800 cells
    unit_entity_t units[MAX_UNITS+1]; // 65 unit slots
} shm_state_t;
```

**Access Patterns**:
```c
// Read grid (no lock needed for reads in many cases)
unit_id_t occupant = ctx->S->grid[x][y];

// Write grid (requires lock)
sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
ctx->S->grid[old_x][old_y] = 0;
ctx->S->grid[new_x][new_y] = unit_id;
sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);

// Iterate units
for (int i = 1; i <= MAX_UNITS; i++) {
    if (ctx->S->units[i].pid > 0 && ctx->S->units[i].alive) {
        // Process unit
    }
}
```

---

## API Reference

### IPC Context API

```c
int ipc_create(ipc_ctx_t *ctx, const char *ftok_path);
```
- **Purpose**: Create and initialize IPC resources
- **Caller**: Command Center only
- **Returns**: 0 on success, -1 on error
- **Side Effects**: Creates SHM, semaphores, message queues; resets state

```c
int ipc_attach(ipc_ctx_t *ctx, const char *ftok_path);
```
- **Purpose**: Attach to existing IPC resources
- **Caller**: Units, UI, CM
- **Returns**: 0 on success, -1 on error
- **Validates**: Magic number check

```c
int ipc_detach(ipc_ctx_t *ctx);
```
- **Purpose**: Detach from shared memory
- **Caller**: All processes before exit
- **Returns**: 0 on success, -1 on error
- **Note**: Does NOT remove IPC objects

```c
int ipc_destroy(ipc_ctx_t *ctx);
```
- **Purpose**: Remove IPC objects from system
- **Caller**: Command Center only (if `ctx->owner == 1`)
- **Returns**: 0 on success, -1 on error
- **Side Effects**: `shmctl/semctl/msgctl IPC_RMID`

---

### Semaphore API

```c
int sem_op_retry(int semid, struct sembuf *ops, size_t nops);
```
- **Purpose**: Execute semaphore operations, retry on EINTR
- **Use**: Uninterruptible operations
- **Returns**: 0 on success, -1 on error

```c
int sem_op_intr(int semid, struct sembuf *ops, size_t nops, 
                volatile sig_atomic_t *stop_flag);
```
- **Purpose**: Execute semaphore operations with cooperative cancellation
- **Use**: Tick barrier synchronization
- **Returns**: 0 on success, -1 on error/interruption

```c
int sem_lock(int semid, unsigned short semnum);
int sem_unlock(int semid, unsigned short semnum);
```
- **Purpose**: Mutex-style lock/unlock
- **Use**: Protect shared memory critical sections
- **Returns**: 0 on success, -1 on error

```c
int sem_lock_intr(int semid, unsigned short semnum, 
                  volatile sig_atomic_t *stop_flag);
```
- **Purpose**: Interruptible lock acquisition
- **Use**: Long critical sections with shutdown support
- **Returns**: 0 on success, -1 on error/interruption

```c
int sem_wait_intr(int semid, unsigned short semnum, short delta,
                  volatile sig_atomic_t *stop_flag);
int sem_post_retry(int semid, unsigned short semnum, short delta);
```
- **Purpose**: Wait/post with customizable delta
- **Use**: Barrier synchronization (delta = -1 for wait, +1 for post)
- **Returns**: 0 on success, -1 on error

---

### Message Queue API

#### Damage Messages
```c
int mq_send_damage(int qreq, const mq_damage_t *dmg);
int mq_try_recv_damage(int qreq, mq_damage_t *out);
```

#### Commander Messages
```c
int mq_send_commander_req(int qreq, const mq_commander_req_t *req);
int mq_try_recv_commander_req(int qreq, mq_commander_req_t *out);
int mq_send_commander_reply(int qrep, const mq_commander_rep_t *rep);
int mq_try_recv_commander_reply(int qrep, mq_commander_rep_t *out);
```

#### Order Messages
```c
int mq_send_order(int qreq, const mq_order_t *order);
int mq_try_recv_order(int qreq, mq_order_t *out);
```

#### Console Manager Messages
```c
int mq_send_cm_cmd(int qreq, const mq_cm_cmd_t *cmd);
int mq_try_recv_cm_cmd(int qreq, mq_cm_cmd_t *out);
int mq_send_cm_reply(int qrep, const mq_cm_rep_t *rep);
int mq_try_recv_cm_reply(int qrep, mq_cm_rep_t *out);
int mq_recv_cm_reply_blocking(int qrep, mq_cm_rep_t *out);
```

**Return Values**:
- `1`: Message received
- `0`: No message available (`ENOMSG`)
- `-1`: Error

---

## Usage Examples

### Example 1: Command Center Initialization

```c
#include "ipc/ipc_context.h"
#include "ipc/semaphores.h"

int main() {
    ipc_ctx_t ctx;
    
    // Create IPC resources
    if (ipc_create(&ctx, "./ipc.key") == -1) {
        perror("ipc_create");
        exit(1);
    }
    
    printf("IPC created: shm_id=%d, sem_id=%d\n", 
           ctx.shm_id, ctx.sem_id);
    printf("Magic: 0x%x\n", ctx.S->magic);
    
    // Initialize grid
    sem_lock(ctx.sem_id, SEM_GLOBAL_LOCK);
    for (int x = 0; x < M; x++) {
        for (int y = 0; y < N; y++) {
            ctx.S->grid[x][y] = 0;  // Empty
        }
    }
    sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);
    
    // ... simulation logic ...
    
    // Cleanup
    ipc_destroy(&ctx);
    return 0;
}
```

---

### Example 2: Unit Process Attachment

```c
#include "ipc/ipc_context.h"

int main(int argc, char **argv) {
    ipc_ctx_t ctx;
    
    // Attach to existing IPC
    if (ipc_attach(&ctx, "./ipc.key") == -1) {
        fprintf(stderr, "Failed to attach to IPC\n");
        exit(1);
    }
    
    unit_id_t my_id = atoi(argv[1]);
    
    // Verify unit slot
    if (ctx.S->units[my_id].pid != getpid()) {
        fprintf(stderr, "Unit ID mismatch\n");
        ipc_detach(&ctx);
        exit(1);
    }
    
    // ... unit logic ...
    
    // Detach before exit
    ipc_detach(&ctx);
    return 0;
}
```

---

### Example 3: Sending Damage

```c
void attack_enemy(ipc_ctx_t *ctx, unit_id_t attacker_id, 
                  unit_id_t target_id, st_points_t damage) {
    pid_t target_pid = ctx->S->units[target_id].pid;
    
    if (target_pid <= 0) {
        return;  // Target dead or invalid
    }
    
    // Send damage message
    mq_damage_t dmg_msg = {
        .mtype = target_pid,
        .target_id = target_id,
        .damage = damage
    };
    
    if (mq_send_damage(ctx->q_req, &dmg_msg) == -1) {
        perror("mq_send_damage");
        return;
    }
    
    // Signal target to process damage
    kill(target_pid, SIGRTMAX);
    
    printf("[Unit %u] Dealt %d damage to unit %u\n", 
           attacker_id, damage, target_id);
}
```

---

### Example 4: Processing Damage

```c
volatile sig_atomic_t g_damage_pending = 0;

void on_damage_signal(int sig) {
    (void)sig;
    g_damage_pending = 1;
}

void process_damage_if_pending(ipc_ctx_t *ctx, unit_id_t unit_id,
                                 unit_stats_t *stats) {
    if (!g_damage_pending) return;
    g_damage_pending = 0;
    
    st_points_t total_damage = 0;
    mq_damage_t dmg;
    
    // Collect all pending damage messages
    while (mq_try_recv_damage(ctx->q_req, &dmg) == 1) {
        if (dmg.target_id == unit_id) {
            total_damage += dmg.damage;
        }
    }
    
    // Apply damage: shields first, then HP
    if (stats->sh >= total_damage) {
        stats->sh -= total_damage;
    } else {
        total_damage -= stats->sh;
        stats->sh = 0;
        stats->hp -= total_damage;
    }
    
    // Update shared memory
    sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
    ctx->S->units[unit_id].hp = stats->hp;
    ctx->S->units[unit_id].sh = stats->sh;
    if (stats->hp <= 0) {
        mark_dead(ctx, unit_id);
    }
    sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
}
```

---

## File Organization

```
src/ipc/
├── ipc_context.c         # Create/attach/destroy IPC
├── semaphores.c          # Semaphore operations
└── ipc_mesq.c            # Message queue operations

include/ipc/
├── ipc_context.h         # IPC context structure & API
├── semaphores.h          # Semaphore function declarations
├── ipc_mesq.h            # Message queue structures & API
└── shared.h              # Shared memory data structures
```

---

## Build Integration

From project `Makefile`:

```makefile
# IPC object files
IPC_OBJS = ipc_context.o semaphores.o ipc_mesq.o

# Build IPC components
ipc_context.o: src/ipc/ipc_context.c include/ipc/ipc_context.h
	$(CC) $(CFLAGS) -c $< -o $@

semaphores.o: src/ipc/semaphores.c include/ipc/semaphores.h
	$(CC) $(CFLAGS) -c $< -o $@

ipc_mesq.o: src/ipc/ipc_mesq.c include/ipc/ipc_mesq.h
	$(CC) $(CFLAGS) -c $< -o $@
```

---

## Error Handling

All IPC functions follow POSIX conventions:
- **Return**: 0 on success, -1 on error
- **errno**: Set to indicate error type
- **Error Checking Pattern**:

```c
if (ipc_create(&ctx, "./ipc.key") == -1) {
    perror("ipc_create");
    fprintf(stderr, "Failed to create IPC: %s\n", strerror(errno));
    exit(1);
}
```

**Common Errors**:
- `EEXIST`: IPC object already exists (use `IPC_CREAT | IPC_EXCL` to detect)
- `ENOENT`: IPC object doesn't exist (unit attached before CC created)
- `EINTR`: Interrupted by signal (handled by `_retry` functions)
- `EINVAL`: Invalid semaphore number or shared memory size
- `ENOMSG`: No message available (expected for `IPC_NOWAIT`)

---

## Debugging & Monitoring

### View IPC Resources

```bash
# List shared memory segments
ipcs -m

# List semaphore sets
ipcs -s

# List message queues
ipcs -q

# All IPC resources
ipcs -a
```

### Remove Stale IPC Resources

```bash
# Remove specific shared memory segment
ipcrm -m <shm_id>

# Remove specific semaphore set
ipcrm -s <sem_id>

# Remove specific message queue
ipcrm -q <queue_id>

# Remove all IPC for current user (DANGER!)
ipcs -m | awk '{print $2}' | xargs -I {} ipcrm -m {}
```

### Monitor IPC Usage

```bash
# Watch IPC resources in real-time
watch -n 1 'ipcs -a'

# Check semaphore values
ipcs -s -i <sem_id>

# Check message queue status
ipcs -q -i <queue_id>
```

---

## Performance Considerations

### Shared Memory
- ✅ **Fast**: Direct memory access, no system calls after `shmat()`
- ⚠️ **Contention**: Lock granularity matters (minimize critical sections)
- 💡 **Tip**: Read operations often don't need locks if single-writer

### Semaphores
- ⚠️ **Blocking**: `sem_wait()` can block indefinitely
- ✅ **Fast**: Kernel-optimized for contention
- 💡 **Tip**: Use `sem_lock()` briefly, prefer message queues for async

### Message Queues
- ✅ **Asynchronous**: Non-blocking with `IPC_NOWAIT`
- ⚠️ **Buffered**: Limited kernel buffer (check `/proc/sys/kernel/msgmax`)
- 💡 **Tip**: Use `mtype` routing to avoid scanning all messages

---

## Best Practices

1. **Always Check Return Values**
   ```c
   if (sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK) == -1) {
       perror("sem_lock");
       return -1;
   }
   ```

2. **Minimize Critical Sections**
   ```c
   // ❌ Bad: Long critical section
   sem_lock(...);
   calculate_pathfinding();  // Expensive!
   ctx->S->grid[x][y] = id;
   sem_unlock(...);
   
   // ✅ Good: Calculate outside lock
   calculate_pathfinding();
   sem_lock(...);
   ctx->S->grid[x][y] = id;
   sem_unlock(...);
   ```

3. **Use Cooperative Cancellation**
   ```c
   volatile sig_atomic_t g_stop = 0;
   
   signal(SIGTERM, on_term);  // Sets g_stop = 1
   
   while (!g_stop) {
       if (sem_wait_intr(..., &g_stop) == -1) {
           if (errno == EINTR && g_stop) break;
       }
       // Process tick
   }
   ```

4. **Detach IPC Before Exit**
   ```c
   void cleanup_on_exit() {
       if (ctx.S != (void*)-1) {
           ipc_detach(&ctx);
       }
   }
   
   atexit(cleanup_on_exit);
   ```

5. **Only Owner Destroys IPC**
   ```c
   if (ctx.owner) {
       ipc_destroy(&ctx);
   } else {
       ipc_detach(&ctx);
   }
   ```

---

## Troubleshooting

### Problem: Unit can't attach to IPC
**Symptoms**: `ipc_attach()` returns -1, errno = `ENOENT`

**Cause**: CC hasn't created IPC yet, or IPC was cleaned up

**Solution**: Ensure CC runs first and creates IPC before spawning units

---

### Problem: Deadlock in tick barrier
**Symptoms**: Simulation hangs, units stuck in `sem_wait()`

**Cause**: Mismatch in alive unit count vs. semaphore posts

**Debug**:
```c
printf("Alive count: %d\n", alive_count);
printf("Posted SEM_TICK_START: %d times\n", post_count);
```

**Solution**: Ensure accurate `alive_count` and post exactly N times

---

### Problem: Stale IPC resources after crash
**Symptoms**: `ipc_create()` fails or attaches to old state

**Solution**: Manually remove stale IPC:
```bash
ipcs -a  # Find IDs
ipcrm -m <shm_id>
ipcrm -s <sem_id>
ipcrm -q <queue_id>
```

---

## See Also

- [CC Module Documentation](CC_MODULE.md) - Command Center usage of IPC
- [UI Module Documentation](UI_MODULE.md) - UI IPC integration
- [Console Manager Documentation](CM_MODULE.md) - CM command protocol
- [Project README](../README.md) - Build and run instructions

---

## References

- `man 2 shmget` - Shared memory creation
- `man 2 shmat` - Shared memory attachment
- `man 2 semget` - Semaphore set creation
- `man 2 semop` - Semaphore operations
- `man 2 msgget` - Message queue creation
- `man 2 msgsnd` - Send message
- `man 2 msgrcv` - Receive message
- `man 3 ftok` - Generate IPC key
