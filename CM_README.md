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
CM> freeze      - Pause simulation (not fully implemented)
CM> unfreeze    - Resume simulation (not fully implemented)
CM> speedup     - Increase simulation speed (not fully implemented)
CM> slowdown    - Decrease simulation speed (not fully implemented)
CM> end         - Shutdown simulation gracefully
CM> help        - Show available commands
CM> quit        - Exit console manager
```

## Implementation Details

### Files Modified/Created

1. **`include/ipc/ipc_mesq.h`**
   - Added `MSG_CM_CMD` message type
   - Added `cm_command_type_t` enum
   - Added `mq_cm_cmd_t` and `mq_cm_rep_t` structures
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

5. **`src/CC/command_center.c`**
   - Updated `handle_cm_command()` to use IPC mesq functions
   - Removed custom message queue setup
   - Calls `handle_cm_command(&ctx)` in main loop

6. **`Makefile`**
   - Updated `console_manager` target with IPC dependencies

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

To fully implement the commands:

1. **Freeze/Unfreeze**: Add a global flag in shared memory that units check
2. **Speedup/Slowdown**: Modify the tick_us variable or add speed multiplier
3. **Status**: Expose more simulation state information
4. **Kill**: Already partially implemented
5. **Spawn**: Integrate with spawn system
6. **Order**: Send orders to specific units

## Benefits of This Design

✅ **Clean separation**: CM is independent process, can crash without affecting simulation  
✅ **Uses existing infrastructure**: Leverages ipc/ folder message queue functions  
✅ **Synchronous**: CM blocks waiting for reply, preventing command spam  
✅ **Scalable**: Easy to add new command types  
✅ **Testable**: Can test CM independently from simulation
