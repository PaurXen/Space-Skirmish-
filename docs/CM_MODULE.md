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
│                                       │
│  ┌────────────────────────────────┐  │
│  │   Main Loop (select)           │  │
│  │   - stdin (terminal input)     │  │
│  │   - UI FIFO (optional)         │  │
│  └─────────────┬──────────────────┘  │
│                │                      │
│  ┌─────────────▼──────────────────┐  │
│  │   Command Parser               │  │
│  │   parse_command()              │  │
│  └─────────────┬──────────────────┘  │
│                │                      │
│  ┌─────────────▼──────────────────┐  │
│  │   Request Sender               │  │
│  │   send_and_wait()              │  │
│  └─────────────┬──────────────────┘  │
└────────────────┼────────────────────┘
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
UI FIFO (opt) ──►│  Console Manager │
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

**Console Manager uses two message types**:

1. **`MSG_CM_CMD`** (requests to CC)
2. **`MSG_SPAWN`** (spawn requests to CC)

And receives:

1. **`mq_cm_rep_t`** (replies from CC)
2. **`mq_spawn_rep_t`** (spawn replies from CC)

---

### CM Command Protocol

#### Request Structure

```c
typedef struct {
    long mtype;           // MSG_CM_CMD
    cm_command_type_t cmd; // Command type
    pid_t sender;         // CM PID
    uint32_t req_id;      // Correlation ID
    int32_t tick_speed_ms; // For TICKSPEED_SET
    int32_t grid_enabled;  // For GRID: -1=query, 0=off, 1=on
    
    /* Spawn parameters (not used in MSG_CM_CMD) */
    unit_type_t spawn_type;
    faction_t spawn_faction;
    int16_t spawn_x;
    int16_t spawn_y;
} mq_cm_cmd_t;
```

**Command Types**:
```c
typedef enum {
    CM_CMD_FREEZE,           // Pause simulation
    CM_CMD_UNFREEZE,         // Resume simulation
    CM_CMD_TICKSPEED_GET,    // Query tick speed
    CM_CMD_TICKSPEED_SET,    // Set tick speed
    CM_CMD_SPAWN,            // Spawn unit (internal)
    CM_CMD_GRID,             // Grid display control
    CM_CMD_END               // Terminate simulation
} cm_command_type_t;
```

#### Response Structure

```c
typedef struct {
    long mtype;           // = sender PID (for filtering)
    uint32_t req_id;      // Matches request req_id
    int16_t status;       // 0 = success, <0 = error
    char message[128];    // Status message
    int32_t tick_speed_ms; // For TICKSPEED_GET response
    int32_t grid_enabled;  // For GRID query response
} mq_cm_rep_t;
```

---

### Spawn Protocol

**CM reuses battleship spawn protocol**:

#### Request Structure

```c
typedef struct {
    long mtype;          // MSG_SPAWN
    pid_t sender;        // CM pid
    unit_id_t sender_id; // 0 for CM (no unit ID)
    point_t pos;         // Spawn coordinates
    unit_type_t utype;   // Unit type
    faction_t faction;   // Faction
    uint32_t req_id;     // Correlation ID
    unit_id_t commander_id; // 0 for CM spawns
} mq_spawn_req_t;
```

#### Response Structure

```c
typedef struct {
    long mtype;          // = sender pid
    uint32_t req_id;     // Matches request
    int16_t status;      // 0 = success, <0 = error
    pid_t child_pid;     // Spawned process PID
    unit_id_t child_unit_id; // Spawned unit ID
} mq_spawn_rep_t;
```

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

**Function**: `parse_command()`

**Algorithm**:
```c
int parse_command(const char *line, mq_cm_cmd_t *cmd) {
    // 1. Copy and sanitize input
    strncpy(buffer, line, sizeof(buffer) - 1);
    
    // 2. Remove trailing newline
    buffer[strlen(buffer) - 1] = '\0';
    
    // 3. Parse first word
    sscanf(buffer, "%63s", first_word);
    
    // 4. Match command
    if (strcmp(first_word, "freeze") == 0) {
        cmd->cmd = CM_CMD_FREEZE;
        return 0;
    }
    // ... other commands
    
    // 5. Parse arguments for parameterized commands
    if (strcmp(first_word, "spawn") == 0) {
        sscanf(buffer, "%*s %s %s %d %d", 
               type_arg, faction_arg, &x_val, &y_val);
        
        // Validate and convert type
        if (strcmp(type_arg, "carrier") == 0) {
            cmd->spawn_type = TYPE_CARRIER;
        }
        // ... other types
        
        // Validate and convert faction
        if (strcmp(faction_arg, "republic") == 0) {
            cmd->spawn_faction = FACTION_REPUBLIC;
        }
        // ... other factions
        
        cmd->spawn_x = x_val;
        cmd->spawn_y = y_val;
        cmd->cmd = CM_CMD_SPAWN;
        return 0;
    }
    
    // 6. Return -1 for invalid/local commands
    return -1;
}
```

---

### Sender Implementation

**Function**: `send_and_wait()`

**Algorithm**:
```c
int send_and_wait(ipc_ctx_t *ctx, mq_cm_cmd_t *cmd) {
    uint32_t req_id = g_next_req_id++;
    
    // Special handling for spawn
    if (cmd->cmd == CM_CMD_SPAWN) {
        // 1. Create spawn request
        mq_spawn_req_t spawn_req;
        spawn_req.mtype = MSG_SPAWN;
        spawn_req.sender = getpid();
        spawn_req.sender_id = 0;
        spawn_req.pos.x = cmd->spawn_x;
        spawn_req.pos.y = cmd->spawn_y;
        spawn_req.utype = cmd->spawn_type;
        spawn_req.faction = cmd->spawn_faction;
        spawn_req.req_id = req_id;
        spawn_req.commander_id = 0;
        
        // 2. Send request
        mq_send_spawn(ctx->q_req, &spawn_req);
        
        // 3. Wait for reply (blocking)
        mq_spawn_rep_t reply;
        while (mq_try_recv_reply(ctx->q_rep, &reply) == 0) {
            // Poll until reply received
        }
        
        // 4. Validate correlation ID
        if (reply.req_id != req_id) {
            return -1;
        }
        
        // 5. Display result
        if (reply.status == 0) {
            printf("✓ Success: Spawned unit %u at (%d,%d) pid=%d\n",
                   reply.child_unit_id, spawn_req.pos.x, 
                   spawn_req.pos.y, reply.child_pid);
        }
        
        return reply.status;
    }
    
    // Regular command handling
    cmd->mtype = MSG_CM_CMD;
    cmd->sender = getpid();
    cmd->req_id = req_id;
    
    // 1. Send command
    mq_send_cm_cmd(ctx->q_req, cmd);
    
    // 2. Wait for reply (blocking)
    mq_cm_rep_t reply;
    mq_recv_cm_reply_blocking(ctx->q_rep, &reply);
    
    // 3. Validate correlation ID
    if (reply.req_id != cmd->req_id) {
        return -1;
    }
    
    // 4. Display result
    if (reply.status == 0) {
        printf("✓ Success: %s\n", reply.message);
    } else {
        printf("✗ Error: %s\n", reply.message);
    }
    
    return reply.status;
}
```

---

### Main Loop

**Pattern**: `select()` multiplexing between stdin and UI FIFO

```c
int main() {
    // 1. Attach to IPC
    ipc_attach(&ctx, ftok_path);
    
    // 2. Create UI FIFOs (optional)
    mkfifo("/tmp/skirmish_cm_to_ui.fifo", 0600);
    mkfifo("/tmp/skirmish_ui_to_cm.fifo", 0600);
    
    // 3. Try to open UI FIFOs (non-blocking)
    ui_output_fd = open(cm_to_ui, O_WRONLY | O_NONBLOCK);
    if (ui_output_fd >= 0) {
        ui_input_fd = open(ui_to_cm, O_RDONLY | O_NONBLOCK);
    }
    
    // 4. Main loop
    while (!g_stop) {
        // Setup file descriptor set
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        if (ui_input_fd >= 0) {
            FD_SET(ui_input_fd, &readfds);
        }
        
        // Wait for input (1 second timeout)
        struct timeval tv = {1, 0};
        int ret = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        
        // Check stdin
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            fgets(line, sizeof(line), stdin);
            if (parse_command(line, &cmd) == 0) {
                send_and_wait(&ctx, &cmd);
            }
            printf("CM> ");
        }
        
        // Check UI FIFO
        if (ui_input_fd >= 0 && FD_ISSET(ui_input_fd, &readfds)) {
            ssize_t n = read(ui_input_fd, line, sizeof(line));
            if (n > 0) {
                line[n] = '\0';
                if (parse_command(line, &cmd) == 0) {
                    send_and_wait(&ctx, &cmd);
                }
            }
        }
    }
    
    // 5. Cleanup
    ipc_detach(&ctx);
    return 0;
}
```

---

## Integration

### UI Integration (Optional)

**FIFOs**:
- `/tmp/skirmish_cm_to_ui.fifo`: CM → UI (output relay)
- `/tmp/skirmish_ui_to_cm.fifo`: UI → CM (commands)

**Relay Output**:
```c
static void relay_printf(const char *format, ...) {
    va_list args1, args2;
    va_start(args1, format);
    
    // Write to stdout
    vprintf(format, args1);
    fflush(stdout);
    
    // Also write to UI if connected
    if (g_ui_output_fd >= 0) {
        char buffer[4096];
        va_copy(args2, args1);
        vsnprintf(buffer, sizeof(buffer), format, args2);
        write(g_ui_output_fd, buffer, strlen(buffer));
    }
    
    va_end(args1);
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
- **Purpose**: Console Manager entry point
- **Arguments**: 
  - `argv[1]`: ftok path (optional, default: `./ipc.key`)
- **Returns**: 0 on success, 1 on error
- **Behavior**: Attach to IPC, enter command loop, cleanup on exit

---

### Command Processing

```c
static int parse_command(const char *line, mq_cm_cmd_t *cmd);
```
- **Purpose**: Parse user input into command structure
- **Parameters**:
  - `line`: Input string (null-terminated)
  - `cmd`: Output command structure
- **Returns**: 0 if valid command, -1 if invalid or local-only
- **Side Effects**: May print error/help messages

```c
static int send_and_wait(ipc_ctx_t *ctx, mq_cm_cmd_t *cmd);
```
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
- **Purpose**: Print to stdout and UI FIFO (if connected)
- **Parameters**: printf-style format and arguments
- **Thread Safe**: No (single-threaded CM)

---

### Signal Handling

```c
static void on_term(int sig);
```
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
└── console_manager.c    # Main CM implementation

include/CM/
└── console_manager.h    # CM interface
```

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

---

## References

- `man 2 select` - I/O multiplexing
- `man 3 mkfifo` - Create FIFO (named pipe)
- `man 2 open` - Open file/FIFO
- `man 2 read` - Read from file descriptor
- `man 3 vprintf` - Variadic printf
