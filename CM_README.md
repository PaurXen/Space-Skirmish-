# Console Manager (CM) Integration

## Architecture

The Console Manager (CM) is now a separate process that runs at the same level as Command Center (CC):

```
├─CC (Command Center)
│  ├─BS (Battleship)
│  └─SQ (Squadron)
└─CM (Console Manager)
```

## Communication

CM and CC communicate via **message queues** using the existing IPC infrastructure in `src/ipc/`:

- **Request Queue** (`q_req`): CM sends commands to CC
- **Reply Queue** (`q_rep`): CC sends responses back to CM

### Architecture - Threaded CM Handler

CC now uses a **dedicated thread** for handling CM commands:

```
CC Process
├── Main Thread (simulation loop)
│   ├── Tick management
│   ├── Spawn requests
│   └── Unit synchronization
│
└── CM Handler Thread (independent)
    ├── Receives CM commands
    ├── Updates shared variables (thread-safe)
    └── Sends replies
```

**Benefits:**
- ✅ CM commands processed **without blocking** main simulation loop
- ✅ More responsive to user input
- ✅ Main simulation continues smoothly even during CM operations
- ✅ Thread-safe access via `pthread_mutex`

### Message Flow

1. CM sends command via `mq_send_cm_cmd()` with type `MSG_CM_CMD`
2. CM **blocks** waiting for reply via `mq_recv_cm_reply_blocking()`
3. CC processes command in main loop via `handle_cm_command()`
4. CC sends reply via `mq_send_cm_reply()` with sender's PID as mtype
5. CM receives reply and displays result
6. CM is ready for next command

## Usage

### Starting the System

1. Start Command Center first:
   ```bash
   ./command_center
   ```

2. In a separate terminal, start Console Manager:
   ```bash
   ./console_manager
   ```

### Available Commands

```
CM> freeze / f                      - Pause simulation
CM> unfreeze / uf                   - Resume simulation
CM> tickspeed [ms] / ts             - Get or set tick speed (0-1000000 ms)
CM> spawn <type> <faction> <x> <y>  - Spawn unit at position
CM> sp <type> <faction> <x> <y>     - Alias for spawn
CM> end                             - Shutdown simulation gracefully
CM> help                            - Show available commands
CM> quit                            - Exit console manager
```

**Examples:**
```
CM> ts              # Query current tick speed
CM> ts 500          # Set tick speed to 500ms
CM> f               # Freeze (short form)
CM> uf              # Unfreeze (short form)
CM> tickspeed 2000  # Set tick speed to 2 seconds
CM> spawn carrier republic 50 50    # Spawn Republic carrier at (50,50)
CM> sp fighter cis 30 30            # Spawn CIS fighter at (30,30)
CM> spawn 3 1 40 40                 # Spawn type 3 (carrier), faction 1 (republic) at (40,40)
```

**Spawn Command Details:**
- **Types**: carrier/3, destroyer/2, flagship/1, fighter/4, bomber/5, elite/6
- **Factions**: republic/1, cis/2
- Both names and numeric IDs are accepted
- Position must be within grid bounds
- Unit will be assigned next available unit ID

## Implementation Details

### Files Modified/Created

1. **`include/ipc/ipc_mesq.h`**
   - Added `MSG_CM_CMD` message type
   - Added `cm_command_type_t` enum (FREEZE, UNFREEZE, TICKSPEED_GET, TICKSPEED_SET, END)
   - Added `mq_cm_cmd_t` structure with `tick_speed_ms` field
   - Added `mq_cm_rep_t` structure with `tick_speed_ms` field for responses
   - Added CM message queue function prototypes

2. **`src/ipc/ipc_mesq.c`**
   - Implemented `mq_send_cm_cmd()`
   - Implemented `mq_try_recv_cm_cmd()`
   - Implemented `mq_send_cm_reply()`
   - Implemented `mq_try_recv_cm_reply()`
   - Implemented `mq_recv_cm_reply_blocking()` for CM to wait

3. **`include/CM/console_manager.h`**
   - Simplified to just include ipc_mesq.h

4. **`src/CM/console_manager.c`**
   - Refactored to use IPC context
   - Uses `ipc_attach()` to connect to existing IPC
   - Uses blocking receive for synchronous command-response pattern
   - Parses command aliases (f/uf, ts/tickspeed)
   - Handles tickspeed with optional argument

5. **`src/CC/command_center.c`**
   - Added `g_frozen` flag for freeze/unfreeze
   - Added `g_tick_speed_ms` variable for dynamic tick speed control
   - Added `g_cm_mutex` pthread mutex for thread-safe access
   - Created `cm_thread_func()` - dedicated thread for CM command handling
   - Updated `handle_cm_command()` with mutex protection for shared variables
   - Main loop reads `g_frozen` and `g_tick_speed_ms` with mutex locks
   - Thread created at startup, joined at shutdown
   - **Tick overflow guard**: Resets tick counter when approaching UINT32_MAX
   - **No longer blocks** main simulation when processing CM commands

6. **`Makefile`**
   - Updated `command_center` target to link with `-lpthread`

### Message Queue Functions

The implementation uses the existing pattern from other message types:

```c
// Send command from CM
mq_send_cm_cmd(ctx->q_req, &cmd);

// Receive command in CC (non-blocking)
int ret = mq_try_recv_cm_cmd(ctx->q_req, &cmd);

// Send reply from CC
mq_send_cm_reply(ctx->q_rep, &reply);

// Receive reply in CM (blocking)
mq_recv_cm_reply_blocking(ctx->q_rep, &reply);
```

## Testing

To test the CM integration:

```bash
# Terminal 1: Start CC
./command_center

# Terminal 2: Start CM
./console_manager

# In CM terminal, try:
CM> help
CM> end    # This will shutdown CC
CM> quit   # This will exit CM
```

## Next Steps

### Implemented Features ✅

1. **Freeze/Unfreeze**: ✅ Fully implemented with command aliases (f/uf)
2. **Tickspeed Control**: ✅ Dynamic tick speed adjustment (0-1000000 ms)
3. **Tick Overflow Guard**: ✅ Automatic reset when approaching UINT32_MAX
4. **Spawn Command**: ✅ Runtime unit spawning with type/faction support

### Future Enhancements

1. **Status**: Expose more simulation state information
2. **Kill**: Implement unit termination command
3. **Spawn**: Integrate with spawn system via CM
4. **Order**: Send orders to specific units via CM

## Features

### Tick Overflow Protection

The Command Center includes an overflow guard that automatically resets the tick counter when it approaches `UINT32_MAX - 1000`:

```c
if (ctx.S->ticks >= UINT32_MAX - 1000) {
    LOGI("[CC] Tick overflow guard triggered, resetting ticks from %u to 0", ctx.S->ticks);
    ctx.S->ticks = 0;
}
```

This ensures the program can run indefinitely without tick counter overflow issues.

### Dynamic Tick Speed

The tick speed can be adjusted at runtime:
- Range: 0-1000000 milliseconds
- Default: 1000 ms (1 second per tick)
- Setting to 0 makes the simulation run as fast as possible
- Higher values slow down the simulation

### Command Aliases

For convenience, short aliases are provided:
- `f` → `freeze`
- `uf` → `unfreeze`  
- `ts` → `tickspeed`

### Thread Safety

All shared variables accessed by both the main simulation thread and CM handler thread are protected by `pthread_mutex`:
- `g_frozen` - simulation pause state
- `g_tick_speed_ms` - tick interval in milliseconds

The CM thread runs independently with a 10ms polling interval, ensuring:
- Minimal CPU usage when idle
- Fast response to CM commands (typically <10ms)
- No interference with simulation timing

## Benefits of This Design

✅ **Clean separation**: CM is independent process, can crash without affecting simulation  
✅ **Uses existing infrastructure**: Leverages ipc/ folder message queue functions  
✅ **Synchronous**: CM blocks waiting for reply, preventing command spam  
✅ **Scalable**: Easy to add new command types  
✅ **Testable**: Can test CM independently from simulation
