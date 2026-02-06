# Console Manager (CM) Module Documentation

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Command Interface](#command-interface)
4. [Communication Protocols](#communication-protocols)
5. [Command Processing](#command-processing)
6. [Integration](#integration)
7. [API Reference](#api-reference)
8. [Usage Examples](#usage-examples)

---

## Overview

The **Console Manager (CM)** module provides an interactive command-line interface for controlling the Space Skirmish simulation in real-time. It allows users to spawn units, adjust simulation parameters, and monitor system status through a request-response message queue protocol.

### Key Responsibilities
- **Interactive CLI**: Accept and parse user commands from terminal
- **Command Validation**: Validate input and parameters before sending
- **Request-Response**: Send commands to Command Center via message queues
- **UI Integration**: Optional FIFO-based communication with UI process
- **Real-time Control**: Freeze/unfreeze simulation, adjust tick speed
- **Unit Spawning**: Dynamically spawn new units during simulation

---

## Architecture

### Process Model

```
┌──────────────────────────────────────┐
│     Console Manager Process          │
│                                      │
│  ┌────────────────────────────────┐  │
│  │   Main Loop (select)           │  │
│  │   - stdin (terminal input)     │  │
│  │   - UI FIFO (optional)         │  │
│  └─────────────┬──────────────────┘  │
│                │                     │
│  ┌─────────────▼──────────────────┐  │
│  │   Command Parser               │  │
│  │   parse_command()              │  │
│  └─────────────┬──────────────────┘  │
│                │                     │
│  ┌─────────────▼──────────────────┐  │
│  │   Request Sender               │  │
│  │   send_and_wait()              │  │
│  └─────────────┬──────────────────┘  │
└────────────────┼─────────────────────┘
                 │ Message Queue
            ┌────▼─────┐
            │ Command  │
            │ Center   │
            └──────────┘
```

### Communication Channels

```
Terminal (stdin)  ────┐
                      │
                 ┌────▼─────────────┐
                 │  Console Manager │
                 └────┬─────────────┘
                      │
                      │ q_req (MSG_CM_CMD)
                      ▼
                 ┌──────────────┐
                 │ Command      │
                 │ Center       │
                 └────┬─────────┘
                      │
                      │ q_rep (mq_cm_rep_t)
                      ▼
                 ┌──────────────┐
                 │  Console     │
                 │  Manager     │
                 └──────────────┘
```

---

## Command Interface

### Available Commands

| Command | Aliases | Arguments | Description |
|---------|---------|-----------|-------------|
| `freeze` | `f` | None | Pause simulation |
| `unfreeze` | `uf` | None | Resume simulation |
| `tickspeed` | `ts` | `[ms]` | Get/set tick speed (ms) |
| `grid` | `g` | `[on\|off]` | Toggle/set grid display |
| `spawn` | `sp` | `<type> <faction> <x> <y>` | Spawn unit |
| `end` | - | None | Terminate simulation |
| `help` | - | None | Show help message |
| `quit` | `exit` | None | Exit Console Manager |

---

### Command Details

#### `freeze` / `f`
**Purpose**: Pause simulation (stop tick progression)

**Usage**:
```
CM> freeze
[CM] Command sent, waiting for response...
[CM] ✓ Success: Simulation frozen
```

**Effect**: Command Center stops posting `SEM_TICK_START`, units block indefinitely

---

#### `unfreeze` / `uf`
**Purpose**: Resume simulation from frozen state

**Usage**:
```
CM> unfreeze
[CM] Command sent, waiting for response...
[CM] ✓ Success: Simulation resumed
```

**Effect**: Command Center resumes tick barrier loop

---

#### `tickspeed` / `ts`
**Purpose**: Query or set tick duration (milliseconds)

**Usage**:
```
# Query current tick speed
CM> tickspeed
[CM] Command sent, waiting for response...
[CM] ✓ Success: Tick speed retrieved
[CM] Tick speed: 1000 ms

# Set tick speed to 500ms
CM> tickspeed 500
[CM] Command sent, waiting for response...
[CM] ✓ Success: Tick speed updated
```

**Constraints**:
- Valid range: 0 - 1,000,000 ms
- 0 ms = fastest possible (no delay)
- Higher values slow down simulation for observation

---

#### `grid` / `g`
**Purpose**: Toggle grid display in UI (if UI is running)

**Usage**:
```
# Query current state
CM> grid
[CM] Command sent, waiting for response...
[CM] ✓ Success: Grid display status

# Enable grid
CM> grid on
[CM] Command sent, waiting for response...
[CM] ✓ Success: Grid display enabled

# Disable grid
CM> grid off
[CM] Command sent, waiting for response...
[CM] ✓ Success: Grid display disabled
```

**Aliases**: `on`, `1`, `T`, `true` for enable; `off`, `0`, `F`, `false` for disable

[\<grid command parsing\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c?plain=1#L137-L160)

---

#### `spawn` / `sp`
**Purpose**: Dynamically spawn new unit during simulation

**Syntax**:
```
spawn <type> <faction> <x> <y>
```

**Parameters**:
- **type**: Unit type (name or number)
  - `flagship` / `1`: Heavy capital ship
  - `destroyer` / `2`: Anti-capital warship
  - `carrier` / `3`: Fighter carrier
  - `fighter` / `4`: Light interceptor
  - `bomber` / `5`: Anti-capital bomber
  - `elite` / `6`: Advanced fighter
  
- **faction**: Faction allegiance (name or number)
  - `republic` / `1`: Republic forces (blue)
  - `cis` / `2`: CIS forces (red)
  
- **x, y**: Grid coordinates (0 to M-1, 0 to N-1)

**Usage**:
```
# Spawn Republic Destroyer at (50, 20)
CM> spawn destroyer republic 50 20
[CM] Sending spawn request: type=2 faction=1 pos=(50,20)
[CM] Spawn request sent, waiting for response...
[CM] ✓ Success: Spawned unit 5 at (50,20) pid=12345

# Spawn CIS Fighter at (90, 30) using numeric codes
CM> spawn 4 2 90 30
[CM] Sending spawn request: type=4 faction=2 pos=(90,30)
[CM] Spawn request sent, waiting for response...
[CM] ✓ Success: Spawned unit 6 at (90,30) pid=12346
```

**Error Handling**:
```
# Invalid coordinates (out of bounds)
CM> spawn fighter republic 200 200
[CM] Sending spawn request: type=4 faction=1 pos=(200,200)
[CM] Spawn request sent, waiting for response...
[CM] ✗ Error: Spawn failed (status=-1)

# Invalid type
CM> spawn tank republic 50 20
Invalid type: tank
Usage: spawn <type> <faction> <x> <y>
Types: carrier/3, destroyer/2, flagship/1, fighter/4, bomber/5, elite/6
Factions: republic/1, cis/2
```

---

#### `end`
**Purpose**: Gracefully terminate simulation

**Usage**:
```
CM> end
[CM] Command sent, waiting for response...
[CM] ✓ Success: Simulation shutting down
[CM] Simulation ended. Exiting...
```

**Effect**: 
- Command Center signals all units with `SIGTERM`
- Waits for all processes to exit
- Cleans up IPC resources
- Console Manager exits

---

#### `help`
**Purpose**: Display available commands

**Usage**:
```
CM> help

Available commands:
  freeze / f                      - Pause simulation
  unfreeze / uf                   - Resume simulation
  tickspeed [ms] / ts             - Get/set tick speed (0-1000000 ms)
  grid [on|off] / g               - Toggle/set grid display
  spawn <type> <faction> <x> <y>  - Spawn unit at position
  sp <type> <faction> <x> <y>     - Alias for spawn
    Types: carrier, destroyer, flagship, fighter, bomber, elite (or 1-6)
    Factions: republic, cis (or 1-2)
  end                             - End simulation
  help                            - Show this help
  quit                            - Exit console manager
```

---

#### `quit` / `exit`
**Purpose**: Exit Console Manager without ending simulation

**Usage**:
```
CM> quit
[CM] Console Manager exiting.
```

**Note**: Simulation continues running; you can start another CM instance to reconnect.

---

## Communication Protocols

### Message Queue Protocol

**Message Types** (defined in [ipc_mesq.h](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/ipc_mesq.h?plain=1#L10)):
```c
enum { MSG_SPAWN = 1, MSG_COMMANDER_REQ = 2, MSG_COMMANDER_REP = 3, 
       MSG_DAMAGE = 4, MSG_ORDER = 5, MSG_CM_CMD = 6, 
       MSG_UI_MAP_REQ = 7, MSG_UI_MAP_REP = 8 };
```

**Console Manager uses**:

1. **`MSG_CM_CMD`** (= 6) - Regular commands to CC
2. **`MSG_SPAWN`** (= 1) - Spawn requests to CC

**And receives**:

1. **`mq_cm_rep_t`** - Replies from CC (filtered by sender PID)
2. **`mq_spawn_rep_t`** - Spawn replies from CC (filtered by sender PID)

---

### CM Command Protocol

#### Request Structure

```c
typedef struct {
    long mtype;           // MSG_CM_CMD (= 6)
    cm_command_type_t cmd; // command type
    pid_t sender;         // CM pid
    uint32_t req_id;      // correlation id
    int32_t tick_speed_ms; // for TICKSPEED_SET command
    int32_t grid_enabled;  // for GRID command: -1=query, 0=off, 1=on
    /* Spawn parameters */
    unit_type_t spawn_type;   // unit type to spawn
    faction_t spawn_faction;  // faction
    int16_t spawn_x;          // x coordinate
    int16_t spawn_y;          // y coordinate
} mq_cm_cmd_t;
```
[\<mq_cm_cmd_t\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/ipc_mesq.h?plain=1#L67-L79)

**Command Types**:
```c
typedef enum {
    CM_CMD_FREEZE,        // 0 - Pause simulation
    CM_CMD_UNFREEZE,      // 1 - Resume simulation
    CM_CMD_TICKSPEED_GET, // 2 - Query tick speed
    CM_CMD_TICKSPEED_SET, // 3 - Set tick speed
    CM_CMD_SPAWN,         // 4 - Spawn unit (internal)
    CM_CMD_GRID,          // 5 - Grid display control
    CM_CMD_END            // 6 - Terminate simulation
} cm_command_type_t;
```
[\<cm_command_type_t\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/ipc_mesq.h?plain=1#L12-L20)

#### Response Structure

```c
typedef struct {
    long mtype;           // = sender pid (so CM can filter)
    uint32_t req_id;      // correlation id
    int16_t status;       // 0 ok, <0 fail
    char message[128];    // status message
    int32_t tick_speed_ms; // for TICKSPEED_GET response
    int32_t grid_enabled;  // for GRID query response
} mq_cm_rep_t;
```
[\<mq_cm_rep_t\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/ipc_mesq.h?plain=1#L81-L88)

---

### Spawn Protocol

**CM reuses battleship spawn protocol**:

#### Request Structure

```c
typedef struct {
    long mtype;          // MSG_SPAWN (= 1)
    pid_t sender;        // BS/CM pid
    unit_id_t sender_id;   // BS unit_id (0 for CM)
    point_t pos;        // desired spawn coords
    unit_type_t utype;       // unit_type_t to spawn
    faction_t faction;   // faction for spawned unit (for CM requests)
    uint32_t req_id;     // optional: correlate replies
    unit_id_t commander_id;  // BS unit_id to assign as commander (0 for CM)
} mq_spawn_req_t;
```
[\<mq_spawn_req_t\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/ipc_mesq.h?plain=1#L22-L31)

#### Response Structure

```c
typedef struct {
    long mtype;          // = sender pid (so BS can filter)
    uint32_t req_id;
    int16_t status;      // 0 ok, <0 fail
    pid_t child_pid;     // spawned squadron pid on success
    unit_id_t child_unit_id; // spawned squadron unit_id on success
} mq_spawn_rep_t;
```
[\<mq_spawn_rep_t\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/ipc/ipc_mesq.h?plain=1#L33-L39)

---

### Request-Response Flow

```
Console Manager                 Command Center
      │                               │
      │  1. Parse command             │
      │     (user input)              │
      │                               │
      │  2. Send request              │
      │     mq_send_cm_cmd()          │
      ├──────────────────────────────►│
      │                               │
      │                          3. Receive request
      │                             mq_try_recv_cm_cmd()
      │                               │
      │                          4. Process command
      │                             handle_cm_command()
      │                               │
      │                          5. Send reply
      │     mq_cm_rep_t              mq_send_cm_reply()
      │◄──────────────────────────────┤
      │                               │
 6. Receive reply                     │
    mq_recv_cm_reply_blocking()       │
      │                               │
 7. Display result                    │
    printf()                          │
      │                               │
 8. Prompt for next                   │
    "CM> "                            │
```

---

## Command Processing

### Parser Implementation

**File**: `src/CM/console_manager.c`

**Function**: `parse_command()`\
[\<parse_command\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c?plain=1#L61-L185)

**Algorithm**:
```c
static int parse_command(const char *line, mq_cm_cmd_t *cmd) {
    char buffer[256];
    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    /* Remove trailing newline */
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = '\0';
    }
    
    /* Parse first word */
    char first_word[64], type_arg[64], faction_arg[64];
    int tick_val = -1;
    int x_val = 0, y_val = 0;
    
    int args = sscanf(buffer, "%63s", first_word);
    if (args < 1) return -1;  /* Empty line */
    
    /* Match command */
    if (strcmp(first_word, "freeze") == 0 || strcmp(first_word, "f") == 0) {
        cmd->cmd = CM_CMD_FREEZE;
        return 0;
    } else if (strcmp(first_word, "unfreeze") == 0 || strcmp(first_word, "uf") == 0) {
        cmd->cmd = CM_CMD_UNFREEZE;
        return 0;
    } else if (strcmp(first_word, "tickspeed") == 0 || strcmp(first_word, "ts") == 0) {
        if (sscanf(buffer, "%*s %d", &tick_val) == 1) {
            cmd->cmd = CM_CMD_TICKSPEED_SET;
            cmd->tick_speed_ms = tick_val;
        } else {
            cmd->cmd = CM_CMD_TICKSPEED_GET;
        }
        return 0;
    } else if (strcmp(first_word, "spawn") == 0 || strcmp(first_word, "sp") == 0) {
        args = sscanf(buffer, "%*s %63s %63s %d %d", type_arg, faction_arg, &x_val, &y_val);
        if (args != 4) {
            printf("Usage: spawn <type> <faction> <x> <y>\n");
            return -1;
        }
        /* Parse type (name or number) */
        if (strcmp(type_arg, "carrier") == 0 || strcmp(type_arg, "3") == 0) {
            cmd->spawn_type = 3;  // TYPE_CARRIER
        } else if (strcmp(type_arg, "destroyer") == 0 || strcmp(type_arg, "2") == 0) {
            cmd->spawn_type = 2;  // TYPE_DESTROYER
        }
        // ... other types
        
        /* Parse faction */
        if (strcmp(faction_arg, "republic") == 0 || strcmp(faction_arg, "1") == 0) {
            cmd->spawn_faction = 1;  // FACTION_REPUBLIC
        } else if (strcmp(faction_arg, "cis") == 0 || strcmp(faction_arg, "2") == 0) {
            cmd->spawn_faction = 2;  // FACTION_CIS
        }
        
        cmd->spawn_x = x_val;
        cmd->spawn_y = y_val;
        cmd->cmd = CM_CMD_SPAWN;
        return 0;
    } else if (strcmp(first_word, "help") == 0) {
        relay_printf("\nAvailable commands:\n");
        // ... print help text
        return -1;  /* Don't send */
    } else if (strcmp(first_word, "quit") == 0 || strcmp(first_word, "exit") == 0) {
        g_stop = 1;
        return -1;  /* Don't send */
    }
    
    relay_printf("Unknown command: %s\n", first_word);
    return -1;
}
```

---

### Sender Implementation

**Function**: `send_and_wait()`\
[\<send_and_wait\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c?plain=1#L187-L277)

**Algorithm**:
```c
static int send_and_wait(ipc_ctx_t *ctx, mq_cm_cmd_t *cmd) {
    uint32_t req_id = g_next_req_id++;
    
    /* Handle spawn command specially - use MSG_SPAWN like BS does */
    if (cmd->cmd == CM_CMD_SPAWN) {
        mq_spawn_req_t spawn_req;
        spawn_req.mtype = MSG_SPAWN;
        spawn_req.sender = getpid();
        spawn_req.sender_id = 0;  // CM has no unit_id
        spawn_req.pos.x = cmd->spawn_x;
        spawn_req.pos.y = cmd->spawn_y;
        spawn_req.utype = cmd->spawn_type;
        spawn_req.faction = cmd->spawn_faction;
        spawn_req.req_id = req_id;
        spawn_req.commander_id = 0;  // No commander for CM spawns
        
        mq_send_spawn(ctx->q_req, &spawn_req);
        
        /* Wait for spawn reply (polling) */
        mq_spawn_rep_t spawn_reply;
        int ret = mq_try_recv_reply(ctx->q_rep, &spawn_reply);
        while (ret == 0 && !g_stop) {
            ret = mq_try_recv_reply(ctx->q_rep, &spawn_reply);
        }
        
        /* Check correlation ID */
        if (spawn_reply.req_id != req_id) {
            return -1;
        }
        
        /* Display result */
        if (spawn_reply.status == 0) {
            printf("[CM] ✓ Success: Spawned unit %u at (%d,%d) pid=%d\n",
                   spawn_reply.child_unit_id, spawn_req.pos.x, 
                   spawn_req.pos.y, spawn_reply.child_pid);
        }
        
        return spawn_reply.status;
    }
    
    /* For non-spawn commands, use regular CM command protocol */
    mq_cm_rep_t reply;
    
    cmd->mtype = MSG_CM_CMD;
    cmd->sender = getpid();
    cmd->req_id = req_id;
    
    /* Send command */
    mq_send_cm_cmd(ctx->q_req, cmd);
    
    relay_printf("[CM] Command sent, waiting for response...\n");
    
    /* Wait for response (blocking) */
    mq_recv_cm_reply_blocking(ctx->q_rep, &reply);
    
    /* Check correlation ID */
    if (reply.req_id != cmd->req_id) {
        return -1;
    }
    
    /* Display result */
    if (reply.status == 0) {
        relay_printf("[CM] ✓ Success: %s\n", reply.message);
        if (cmd->cmd == CM_CMD_TICKSPEED_GET) {
            relay_printf("[CM] Tick speed: %d ms\n", reply.tick_speed_ms);
        }
    } else {
        relay_printf("[CM] ✗ Error: %s (status=%d)\n", reply.message, reply.status);
    }
    
    return reply.status;
}
```

---

### Main Loop

**Pattern**: `select()` multiplexing between stdin and UI FIFO\
[\<main\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c?plain=1#L295-L453)

```c
int main(int argc, char **argv) {
    ipc_ctx_t ctx;
    
    /* Initialize logging */
    log_init("CM", 0);
    
    /* Set up signal handlers */
    signal(SIGINT, on_term);
    signal(SIGTERM, on_term);
    
    printf("[CM] Console Manager starting (PID %d)...\n", getpid());
    
    /* Attach to existing IPC */
    const char *ftok_path = (argc > 1) ? argv[1] : "./ipc.key";
    
    if (ipc_attach(&ctx, ftok_path) < 0) {
        fprintf(stderr, "[CM] Failed to attach to IPC (is CC running?)\n");
        return 1;
    }
    
    printf("[CM] Connected to IPC (qreq=%d, qrep=%d)\n", ctx.q_req, ctx.q_rep);
    
    /* Create FIFOs for UI communication */
    const char *cm_to_ui = "/tmp/skirmish_cm_to_ui.fifo";
    const char *ui_to_cm = "/tmp/skirmish_ui_to_cm.fifo";
    
    unlink(cm_to_ui);
    unlink(ui_to_cm);
    mkfifo(cm_to_ui, 0600);
    mkfifo(ui_to_cm, 0600);
    
    int ui_input_fd = -1;
    int ui_output_fd = -1;
    
    /* Try to open UI FIFOs in non-blocking mode */
    ui_output_fd = open(cm_to_ui, O_WRONLY | O_NONBLOCK);
    if (ui_output_fd >= 0) {
        printf("[CM] UI connected!\n");
        g_ui_output_fd = ui_output_fd;
        ui_input_fd = open(ui_to_cm, O_RDONLY | O_NONBLOCK);
    } else if (errno == ENXIO) {
        printf("[CM] No UI detected, using terminal mode\n");
    }
    
    relay_printf("\n=== Space Skirmish Console Manager ===\n");
    relay_printf("Type 'help' for available commands\n\n");
    relay_printf("CM> ");
    
    /* Main command loop - select between stdin and UI */
    char line[256];
    while (!g_stop) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        
        int maxfd = STDIN_FILENO;
        if (ui_input_fd >= 0) {
            FD_SET(ui_input_fd, &readfds);
            if (ui_input_fd > maxfd) maxfd = ui_input_fd;
        }
        
        struct timeval tv = {1, 0};  // 1 second timeout
        int ret = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        if (ret == 0) {
            /* Timeout - try to reconnect UI if disconnected */
            if (ui_output_fd < 0) {
                ui_output_fd = open(cm_to_ui, O_WRONLY | O_NONBLOCK);
                if (ui_output_fd >= 0) {
                    g_ui_output_fd = ui_output_fd;
                    ui_input_fd = open(ui_to_cm, O_RDONLY | O_NONBLOCK);
                }
            }
            continue;
        }
        
        /* Check stdin */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(line, sizeof(line), stdin) == NULL) break;
            
            mq_cm_cmd_t cmd;
            memset(&cmd, 0, sizeof(cmd));
            
            if (parse_command(line, &cmd) == 0) {
                send_and_wait(&ctx, &cmd);
                
                if (cmd.cmd == CM_CMD_END) {
                    relay_printf("[CM] Simulation ended. Exiting...\n");
                    break;
                }
            }
            relay_printf("CM> ");
        }
        
        /* Check UI FIFO */
        if (ui_input_fd >= 0 && FD_ISSET(ui_input_fd, &readfds)) {
            ssize_t n = read(ui_input_fd, line, sizeof(line) - 1);
            if (n <= 0) {
                relay_printf("[CM] UI disconnected\n");
                close(ui_input_fd);
                close(ui_output_fd);
                ui_input_fd = -1;
                ui_output_fd = -1;
                g_ui_output_fd = -1;
            } else {
                line[n] = '\0';
                line[strcspn(line, "\n")] = '\0';
                
                if (line[0] != '\0') {
                    mq_cm_cmd_t cmd;
                    memset(&cmd, 0, sizeof(cmd));
                    
                    if (parse_command(line, &cmd) == 0) {
                        send_and_wait(&ctx, &cmd);
                        if (cmd.cmd == CM_CMD_END) break;
                    }
                }
            }
        }
    }
    
    /* Cleanup */
    if (ui_output_fd >= 0) close(ui_output_fd);
    if (ui_input_fd >= 0) close(ui_input_fd);
    unlink(cm_to_ui);
    unlink(ui_to_cm);
    
    ipc_detach(&ctx);
    printf("[CM] Console Manager exiting.\n");
    
    return 0;
}
```

---

## Integration

### UI Integration (Optional)

**FIFOs**:
- `/tmp/skirmish_cm_to_ui.fifo`: CM → UI (output relay)
- `/tmp/skirmish_ui_to_cm.fifo`: UI → CM (commands)

**Relay Output**:\
[\<relay_printf\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c?plain=1#L39-L58)
```c
/* Relay printf - write to both stdout and UI if connected */
static void relay_printf(const char *format, ...) {
    va_list args1, args2;
    va_start(args1, format);
    
    /* Always write to stdout */
    va_copy(args2, args1);
    vprintf(format, args1);
    fflush(stdout);
    va_end(args1);
    
    /* Also write to UI if connected */
    if (g_ui_output_fd >= 0) {
        char buffer[4096];
        vsnprintf(buffer, sizeof(buffer), format, args2);
        write(g_ui_output_fd, buffer, strlen(buffer));
    }
    va_end(args2);
}
```

**Use relay_printf()** instead of printf() to ensure UI receives output.

---

### Command Center Integration

**CC Thread**: `cm_thread()` in `command_center.c`

**Responsibilities**:
1. Poll for CM commands via `mq_try_recv_cm_cmd()`
2. Execute commands (freeze, tickspeed, spawn, etc.)
3. Send reply via `mq_send_cm_reply()`

**Example Handler**:
```c
void cm_thread(void *arg) {
    ipc_ctx_t *ctx = (ipc_ctx_t *)arg;
    
    while (!g_stop) {
        mq_cm_cmd_t cmd;
        if (mq_try_recv_cm_cmd(ctx->q_req, &cmd) == 1) {
            mq_cm_rep_t reply;
            reply.mtype = cmd.sender;
            reply.req_id = cmd.req_id;
            
            switch (cmd.cmd) {
                case CM_CMD_FREEZE:
                    g_frozen = 1;
                    reply.status = 0;
                    strcpy(reply.message, "Simulation frozen");
                    break;
                
                case CM_CMD_TICKSPEED_SET:
                    g_tick_speed_ms = cmd.tick_speed_ms;
                    reply.status = 0;
                    strcpy(reply.message, "Tick speed updated");
                    break;
                
                // ... other commands
            }
            
            mq_send_cm_reply(ctx->q_rep, &reply);
        }
        
        usleep(10000);  // 10ms poll
    }
}
```

---

## API Reference

### Initialization

```c
int main(int argc, char **argv);
```
[\<main\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c?plain=1#L295-L453)
- **Purpose**: Console Manager entry point
- **Arguments**: 
  - `argv[1]`: ftok path (optional, default: `./ipc.key`)
- **Returns**: 0 on success, 1 on error
- **Behavior**: Initialize logging, attach to IPC, enter command loop, cleanup on exit

---

### Command Processing

```c
static int parse_command(const char *line, mq_cm_cmd_t *cmd);
```
[\<parse_command\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c?plain=1#L60-L185)
- **Purpose**: Parse user input into command structure
- **Parameters**:
  - `line`: Input string (null-terminated)
  - `cmd`: Output command structure
- **Returns**: 0 if valid command, -1 if invalid or local-only
- **Side Effects**: May print error/help messages via `relay_printf()`

```c
static int send_and_wait(ipc_ctx_t *ctx, mq_cm_cmd_t *cmd);
```
[\<send_and_wait\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c?plain=1#L187-L277)
- **Purpose**: Send command to CC and wait for response
- **Parameters**:
  - `ctx`: IPC context
  - `cmd`: Command to send
- **Returns**: 0 on success, <0 on error
- **Blocking**: Yes (waits for CC reply)

---

### Output Relay

```c
static void relay_printf(const char *format, ...);
```
[\<relay_printf\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c?plain=1#L39-L58)
- **Purpose**: Print to stdout and UI FIFO (if connected)
- **Parameters**: printf-style format and arguments
- **Thread Safe**: No (single-threaded CM)

---

### Signal Handling

```c
static void on_term(int sig);
```
[\<on_term\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c?plain=1#L32-L37)
- **Purpose**: Handle SIGINT/SIGTERM for graceful shutdown
- **Side Effects**: Sets `g_stop = 1`

---

## Usage Examples

### Example 1: Starting Console Manager

```bash
# First, start Command Center in one terminal
./command_center

# Then, start Console Manager in another terminal
./console_manager

# Optional: Start UI in a third terminal
./ui
```

**Output**:
```
[CM] Console Manager starting (PID 12345)...
[CM] Connected to IPC (qreq=3, qrep=4)
[CM] No UI detected, using terminal mode

=== Space Skirmish Console Manager ===
Type 'help' for available commands

CM> 
```

---

### Example 2: Interactive Session

```
CM> help

Available commands:
  freeze / f                      - Pause simulation
  unfreeze / uf                   - Resume simulation
  tickspeed [ms] / ts             - Get/set tick speed (0-1000000 ms)
  grid [on|off] / g               - Toggle/set grid display
  spawn <type> <faction> <x> <y>  - Spawn unit at position
  ...

CM> tickspeed
[CM] Command sent, waiting for response...
[CM] ✓ Success: Tick speed retrieved
[CM] Tick speed: 1000 ms

CM> tickspeed 500
[CM] Command sent, waiting for response...
[CM] ✓ Success: Tick speed updated

CM> spawn destroyer republic 50 20
[CM] Sending spawn request: type=2 faction=1 pos=(50,20)
[CM] Spawn request sent, waiting for response...
[CM] ✓ Success: Spawned unit 5 at (50,20) pid=23456

CM> freeze
[CM] Command sent, waiting for response...
[CM] ✓ Success: Simulation frozen

CM> unfreeze
[CM] Command sent, waiting for response...
[CM] ✓ Success: Simulation resumed

CM> quit
[CM] Console Manager exiting.
```

---

### Example 3: Scripted Commands

**Create script** `commands.txt`:
```
tickspeed 500
spawn destroyer republic 10 10
spawn fighter cis 90 30
spawn carrier republic 50 20
freeze
```

**Execute**:
```bash
./console_manager < commands.txt
```

**Or with heredoc**:
```bash
./console_manager <<EOF
tickspeed 200
spawn fighter republic 20 20
spawn fighter cis 80 20
unfreeze
EOF
```

---

### Example 4: Error Handling

```
CM> spawn invalid_type republic 50 20
Invalid type: invalid_type
Usage: spawn <type> <faction> <x> <y>
Types: carrier/3, destroyer/2, flagship/1, fighter/4, bomber/5, elite/6
Factions: republic/1, cis/2

CM> tickspeed 2000000
[CM] Command sent, waiting for response...
[CM] ✗ Error: Invalid tick speed (valid range: 0-1000000 ms) (status=-1)

CM> spawn destroyer republic 200 200
[CM] Sending spawn request: type=2 faction=1 pos=(200,200)
[CM] Spawn request sent, waiting for response...
[CM] ✗ Error: Spawn failed (status=-1)
```

---

## File Organization

```
src/CM/
└── console_manager.c    # Main CM implementation (454 lines)

include/CM/
└── console_manager.h    # CM interface
```

[\<console_manager.c\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c)\
[\<console_manager.h\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/CM/console_manager.h)

---

## Build Integration

From project `Makefile`:

```makefile
# Build console_manager binary
console_manager: console_manager.o $(IPC_OBJS) error_handler.o utils.o
	$(CC) $(CFLAGS) -o $@ $^

console_manager.o: src/CM/console_manager.c include/CM/console_manager.h
	$(CC) $(CFLAGS) -c $< -o $@
```

**Dependencies**:
- IPC module (message queues)
- Error handler
- Standard C library (stdio, stdlib, string, unistd)

---

## Error Handling

### Common Errors

#### IPC Attachment Failure
**Symptom**: `[CM] Failed to attach to IPC (is CC running?)`

**Cause**: Command Center not running or IPC not created

**Solution**: Start Command Center first
```bash
# Terminal 1: Start Command Center
./command_center

# Terminal 2: Start Console Manager
./console_manager

# Terminal 3 (optional): Start UI
./ui
```

---

#### Command Timeout
**Symptom**: CM hangs after sending command

**Cause**: Command Center not processing CM messages (CM thread not running)

**Debug**:
```bash
# Check if CC cm_thread is active
ps -L -p $(pgrep command_center)

# Check message queue status
ipcs -q
```

---

#### Spawn Failure
**Symptom**: `[CM] ✗ Error: Spawn failed (status=-1)`

**Causes**:
1. **Position out of bounds**: x >= M or y >= N
2. **Position occupied**: Cell already has a unit
3. **Max units reached**: MAX_UNITS (64) limit exceeded
4. **Fork failure**: System resource exhaustion

**Solution**:
- Verify coordinates: 0 ≤ x < M (120), 0 ≤ y < N (40)
- Check unit count: `ps aux | grep -E "battleship|squadron" | wc -l`
- Free system resources

---

#### UI Connection Loss
**Symptom**: `[CM] UI disconnected`

**Cause**: UI process exited or closed FIFO

**Behavior**: CM continues in terminal mode; output to stdout only

**Reconnection**: CM attempts to reconnect every second (timeout in select loop)

---

## Best Practices

1. **Always Check CC is Running**
   ```bash
   # Terminal 1: Start CC first
   ./command_center
   
   # Terminal 2: Then start CM
   ./console_manager
   
   # Terminal 3 (optional): Start UI
   ./ui
   ```

2. **Use Aliases for Speed**
   ```
   f          instead of freeze
   uf         instead of unfreeze
   ts 500     instead of tickspeed 500
   sp 4 1 50 20  instead of spawn fighter republic 50 20
   ```

3. **Validate Coordinates Before Spawning**
   - Max X: M - 1 = 119
   - Max Y: N - 1 = 39
   - Use `grid` command to visualize available space

4. **Monitor Unit Count**
   ```
   # Check current units
   ps aux | grep -E "battleship|squadron" | wc -l
   
   # Don't exceed MAX_UNITS (64)
   ```

5. **Use `help` for Reference**
   ```
   CM> help
   # Shows all commands and syntax
   ```

---

## Advanced Usage

### Batch Spawning

```bash
# Spawn line of defenders
for x in {10..30}; do
    echo "spawn fighter republic $x 20"
done | ./console_manager
```

### Monitoring Mode

```bash
# Periodic status queries
while true; do
    echo "tickspeed"
    sleep 5
done | ./console_manager
```

### Remote Control (via named pipes)

```bash
# Terminal 1: CM with input FIFO
mkfifo /tmp/cm_commands
./console_manager < /tmp/cm_commands

# Terminal 2: Send commands
echo "freeze" > /tmp/cm_commands
echo "tickspeed 100" > /tmp/cm_commands
echo "unfreeze" > /tmp/cm_commands
```

---

## Debugging

### Enable Verbose Logging

```c
// In console_manager.c
#define DEBUG_CM 1

#ifdef DEBUG_CM
#define CM_DEBUG(...) fprintf(stderr, "[CM-DEBUG] " __VA_ARGS__)
#else
#define CM_DEBUG(...)
#endif

// Usage
CM_DEBUG("Parsed command: %d\n", cmd.cmd);
CM_DEBUG("Sending request with ID %u\n", req_id);
```

### Trace Message Queue Activity

```bash
# Monitor message queue statistics
watch -n 1 'ipcs -q -i <queue_id>'

# Count pending messages
ipcs -q | grep skirmish
```

### Test Parser

```c
// Unit test for parse_command
void test_parser() {
    mq_cm_cmd_t cmd;
    
    assert(parse_command("freeze\n", &cmd) == 0);
    assert(cmd.cmd == CM_CMD_FREEZE);
    
    assert(parse_command("tickspeed 500\n", &cmd) == 0);
    assert(cmd.cmd == CM_CMD_TICKSPEED_SET);
    assert(cmd.tick_speed_ms == 500);
    
    assert(parse_command("spawn destroyer republic 50 20\n", &cmd) == 0);
    assert(cmd.spawn_type == TYPE_DESTROYER);
    assert(cmd.spawn_x == 50);
    assert(cmd.spawn_y == 20);
}
```

---

## Performance Considerations

### Blocking Operations

**CM uses blocking receive**:
```c
mq_recv_cm_reply_blocking(ctx->q_rep, &reply);
```

**Advantage**: No CPU waste on polling

**Disadvantage**: Hangs if CC doesn't respond

**Mitigation**: Use timeout in `select()` loop (1 second)

---

### Memory Usage

**Static Buffers**:
- Command buffer: 256 bytes
- Message structures: ~256 bytes each

**Total Memory**: <1 KB (negligible)

---

## See Also

- [CC Module Documentation](CC_MODULE.md) - Command Center CM thread implementation
- [IPC Module Documentation](IPC_MODULE.md) - Message queue protocol
- [UI Module Documentation](UI_MODULE.md) - UI FIFO integration
- [Project README](../README.md) - Build and run instructions

### Message Queue API Functions

| Function | Line | Purpose |
|----------|------|---------|
| [mq_send_cm_cmd](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_mesq.c?plain=1#L90-L92) | L90 | Send CM command to request queue |
| [mq_try_recv_cm_cmd](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_mesq.c?plain=1#L94-L98) | L94 | Non-blocking receive CM command |
| [mq_send_cm_reply](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_mesq.c?plain=1#L100-L102) | L100 | Send CM reply to reply queue |
| [mq_try_recv_cm_reply](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_mesq.c?plain=1#L104-L109) | L104 | Non-blocking receive CM reply (filtered by PID) |
| [mq_recv_cm_reply_blocking](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_mesq.c?plain=1#L111-L115) | L111 | Blocking receive CM reply |

---

## References

- `man 2 select` - I/O multiplexing
- `man 3 mkfifo` - Create FIFO (named pipe)
- `man 2 open` - Open file/FIFO
- `man 2 read` - Read from file descriptor
- `man 3 vprintf` - Variadic printf
