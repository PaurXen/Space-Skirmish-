# Error Handling, Logging, and Terminal Tee Module Documentation

## Table of Contents

1. [Overview](#overview)
2. [Module Architecture](#module-architecture)
3. [Error Handling System](#error-handling-system)
   - [Error Levels](#error-levels)
   - [Application Error Codes](#application-error-codes)
   - [Error Handling Macros](#error-handling-macros)
   - [Error Handling Functions](#error-handling-functions)
   - [Validation Functions](#validation-functions)
4. [Logging System](#logging-system)
   - [Log Levels](#log-levels)
   - [Log Infrastructure](#log-infrastructure)
   - [Log Initialization](#log-initialization)
   - [Logging API](#logging-api)
   - [Log File Organization](#log-file-organization)
5. [Terminal Tee System](#terminal-tee-system)
   - [Purpose and Design](#purpose-and-design)
   - [Double-Fork Architecture](#double-fork-architecture)
   - [Output Redirection](#output-redirection)
   - [Signal Handling](#signal-handling)
6. [API Reference](#api-reference)
   - [Error Handling API](#error-handling-api)
   - [Logging API](#logging-api)
   - [Terminal Tee API](#terminal-tee-api)
7. [Usage Examples](#usage-examples)
   - [Error Handling Examples](#error-handling-examples)
   - [Logging Examples](#logging-examples)
   - [Terminal Tee Examples](#terminal-tee-examples)
8. [Integration Patterns](#integration-patterns)
9. [Best Practices](#best-practices)
10. [Debugging and Troubleshooting](#debugging-and-troubleshooting)
11. [Performance Considerations](#performance-considerations)
12. [References](#references)

---

## Overview

The **Error Handling, Logging, and Terminal Tee** subsystem provides robust infrastructure for error reporting, diagnostic logging, and output management in the Space Skirmish simulation. These three components work together to ensure:

- **Reliability**: Consistent error handling across system calls and application logic
- **Observability**: Comprehensive logging of system state and events
- **Debugging**: Unified output capture for post-mortem analysis
- **User Experience**: Clean terminal output with optional UI integration

### Key Features

**Error Handling System:**
- Three-level severity hierarchy (FATAL, ERROR, WARNING)
- 22 standardized application error codes
- Macro-based system call checking with automatic error reporting
- Centralized error message formatting
- errno integration for POSIX error codes

**Logging System:**
- Four log levels (DEBUG, INFO, WARN, ERROR) with runtime filtering
- Dual logging: per-process log files + combined ALL.log
- Atomic append operations for thread/process safety
- Timestamped entries with process/unit identification
- Minimal performance overhead with buffering

**Terminal Tee System:**
- Transparent stdout/stderr redirection
- Simultaneous logging to file and terminal
- Optional UI integration via FIFO
- Independent background process for resilience
- Signal-tolerant design (survives SIGINT/SIGTERM)

### Design Philosophy

These systems embody several design principles:

1. **Fail-Fast**: Fatal errors terminate immediately with clear diagnostics
2. **Observability**: All significant events are logged with context
3. **Separation of Concerns**: Error handling, logging, and output are independent
4. **Minimal Overhead**: Low-latency macros and buffered I/O
5. **Robustness**: Graceful degradation when subsystems fail

---

## Module Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Space Skirmish Application                    │
└────────────┬────────────┬────────────┬──────────────────────────┘
             │            │            │
             v            v            v
      ┌──────────┐  ┌─────────┐  ┌──────────────────┐
      │  Error   │  │ Logging │  │  Terminal Tee    │
      │ Handling │  │ System  │  │     System       │
      └──────────┘  └─────────┘  └──────────────────┘
             │            │            │
             │            │            │
        ┌────v────────────v────────────v─────┐
        │      Unified Output Stream          │
        │  (per-process logs, ALL.log, tty)   │
        └─────────────────────────────────────┘
                         │
            ┌────────────┼────────────┐
            v            v            v
    ┌─────────────┐  ┌─────────┐  ┌──────────┐
    │ Per-Process │  │ALL.log  │  │ Terminal │
    │  Log Files  │  │ (atomic)│  │  Output  │
    └─────────────┘  └─────────┘  └──────────┘
                                        │
                                        v
                                  ┌──────────┐
                                  │UI STD    │
                                  │Window    │
                                  │(optional)│
                                  └──────────┘
```

### Component Interaction

```
┌─────────────────────────────────────────────────────────────────┐
│                      Application Code                            │
│                                                                   │
│  if (CHECK_SYS_CALL(shm_fd = shm_open(...)) == -1) {            │
│      HANDLE_SYS_ERROR("shm_open", ERR_SHM_ERROR);               │
│  }                                                                │
│  LOGI("Shared memory initialized: %s", shm_name);               │
└──────────────┬────────────────────────────┬─────────────────────┘
               │                            │
               │ (on error)                 │ (always)
               v                            v
    ┌──────────────────┐         ┌──────────────────────┐
    │  Error Handler   │         │   Logging System     │
    │  - Format error  │         │   - Timestamp        │
    │  - Log to file   │         │   - Format message   │
    │  - Exit if FATAL │         │   - Write to logs    │
    └──────────┬───────┘         └──────────┬───────────┘
               │                            │
               └────────────┬───────────────┘
                            │
                            v
               ┌─────────────────────────┐
               │   Terminal Tee Process  │
               │   (background daemon)   │
               │                         │
               │  stdin ─┬─> /dev/tty   │
               │         ├─> ALL.term.log│
               │         └─> ui_stdout.  │
               │             fifo        │
               └─────────────────────────┘
```

### File Organization

```
include/
├── error_handler.h      # Error types, codes, macros (146 lines)
├── log.h                # Logging API and macros (48 lines)
└── tee_spawn.h          # Terminal tee spawn interface (14 lines)

src/
├── error_handler.c      # Error handling implementation (136 lines)
├── utils.c              # Logging backend implementation (234 lines)
└── CC/
    └── terminal_tee.c   # Terminal tee process management (114 lines)
```

[\<error_handler.h\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/error_handler.h)\
[\<log.h\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/log.h)\
[\<tee_spawn.h\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/tee_spawn.h)\
[\<error_handler.c\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/error_handler.c)\
[\<utils.c\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/utils.c)\
[\<terminal_tee.c\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c)

logs/run_YYYY-MM-DD_HH-MM-SS_pidXXXXX/
├── ALL.log              # Combined log from all processes
├── ALL.term.log         # Terminal output capture
├── CC.log               # Command Center log
├── UI.log               # User Interface log
├── CM.log               # Console Manager log
├── Flagship0.log        # Unit process logs
├── Battleship0.log
├── Battleship1.log
└── Squadron0.log
```

---

## Error Handling System

The error handling system provides consistent error reporting, categorization, and recovery mechanisms across the entire application.

### Error Levels
[\<error_level_t\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/error_handler.h?plain=1#L9-L14)

Three severity levels determine how errors are handled:

```c
typedef enum {
    ERR_FATAL,      /* Critical error - program cannot continue */
    ERR_ERROR,      /* Error condition - operation failed */
    ERR_WARNING     /* Warning - operation may have issues */
} error_level_t;
```

#### Error Level Usage

| Level       | Meaning                         | Action                      | Examples                          |
|-------------|---------------------------------|-----------------------------|-----------------------------------|
| `ERR_FATAL` | System cannot continue          | Log error, cleanup, exit(1) | Shared memory init failure        |
| `ERR_ERROR` | Operation failed but recoverable| Log error, return error code| Semaphore operation timeout       |
| `ERR_WARNING` | Unexpected but handled         | Log warning, continue       | Missing config parameter          |

### Application Error Codes
[\<app_error_t\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/error_handler.h?plain=1#L16-L40)

20 standardized error codes cover domain-specific failures:

```c
typedef enum {
    ERR_OK = 0,
    ERR_INVALID_INPUT,       // Invalid user input or parameters
    ERR_INVALID_RANGE,       // Value out of valid range
    ERR_INVALID_COORD,       // Invalid coordinates
    ERR_QUEUE_FULL,          // Queue is full
    ERR_QUEUE_EMPTY,         // Queue is empty
    ERR_SHM_ERROR,           // Shared memory error
    ERR_SEM_ERROR,           // Semaphore error
    ERR_MSGQ_ERROR,          // Message queue error
    ERR_FORK_ERROR,          // Fork error
    ERR_PIPE_ERROR,          // Pipe error
    ERR_FILE_ERROR,          // File operation error
    ERR_MEMORY_ERROR,        // Memory allocation error
    ERR_TIMEOUT,             // Operation timeout
    ERR_INVALID_STATE,       // Invalid state
    ERR_UNIT_NOT_FOUND,      // Unit not found
    ERR_WEAPON_NOT_FOUND,    // Weapon not found
    ERR_INVALID_UNIT_TYPE,   // Invalid unit type
    ERR_INVALID_WEAPON_TYPE, // Invalid weapon type
    ERR_PARSE_ERROR,         // Parse error
    ERR_IPC_ERROR,           // IPC communication error
    ERR_LOG_ERROR            // Logging error
} app_error_t;
```

### Error Handling Macros
[\<Error Macros\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/error_handler.h?plain=1#L56-L99)

#### CHECK_SYS_CALL
[\<CHECK_SYS_CALL\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/error_handler.h?plain=1#L75-L86)

Wraps system calls that return -1 on error and set `errno`:

```c
#define CHECK_SYS_CALL(call, context) \
    ({ \
        int _ret = (call); \
        if (_ret == -1) { \
            HANDLE_SYS_ERROR(context, #call); \
        } \
        _ret; \
    })
```

**Features:**
- Evaluates expression exactly once (statement expression)
- Automatically calls `HANDLE_SYS_ERROR` on failure (exits for fatal errors)
- Returns original result for further checking
- Preserves errno value

**Usage:**
```c
int fd = CHECK_SYS_CALL(open(path, O_RDONLY), "open file");
```

#### CHECK_SYS_CALL_NONFATAL
[\<CHECK_SYS_CALL_NONFATAL\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/error_handler.h?plain=1#L88-L99)

Similar to `CHECK_SYS_CALL` but uses non-fatal error handling:

```c
#define CHECK_SYS_CALL_NONFATAL(call, context) \
    ({ \
        int _ret = (call); \
        if (_ret == -1) { \
            HANDLE_SYS_ERROR_NONFATAL(context, #call); \
        } \
        _ret; \
    })
```

**Usage:**
```c
// Close may fail if FD already closed - log but don't exit
int ret = CHECK_SYS_CALL_NONFATAL(close(fd), "close fd");
if (ret == -1) {
    // Already logged, just continue
}
```

#### HANDLE_SYS_ERROR
[\<HANDLE_SYS_ERROR\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/error_handler.h?plain=1#L56-L61)

Handles system call errors using `errno` and terminates with `ERR_FATAL`:

```c
#define HANDLE_SYS_ERROR(context, msg) \
    handle_error(ERR_FATAL, context, ERR_OK, 1, "%s", msg)
```

**Parameters:**
- `context`: Context string (e.g., function name, operation)
- `msg`: Error message describing what failed

**Typical Pattern:**
```c
if (shm_fd == -1) {
    HANDLE_SYS_ERROR("init_shm", "shm_open failed");
}
```

#### HANDLE_SYS_ERROR_NONFATAL
[\<HANDLE_SYS_ERROR_NONFATAL\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/error_handler.h?plain=1#L62-L64)

Non-fatal version that logs but doesn't terminate:

```c
#define HANDLE_SYS_ERROR_NONFATAL(context, msg) \
    handle_error(ERR_ERROR, context, ERR_OK, 1, "%s", msg)
```

#### HANDLE_APP_ERROR
[\<HANDLE_APP_ERROR\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/error_handler.h?plain=1#L63-L68)

Handles application-specific errors (not using errno):

```c
#define HANDLE_APP_ERROR(level, context, err_code, msg) \
    handle_error(level, context, err_code, 0, "%s", msg)
```

### Error Handling Functions

#### handle_error
[\<handle_error\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/error_handler.c?plain=1#L46-L98)

Central error handling function:

```c
void handle_error(error_level_t level, const char *context, 
                 app_error_t err_code, int use_errno, 
                 const char *fmt, ...);
```

**Parameters:**
- `level`: Error severity (`ERR_FATAL`, `ERR_ERROR`, `ERR_WARNING`)
- `context`: Context string (e.g., function name)
- `err_code`: Application error code (or `ERR_OK` if using errno)
- `use_errno`: If 1, use `perror()` and errno; if 0, use err_code
- `fmt`: printf-style format string for additional message

**Behavior:**
1. Formats error message with `vsnprintf`
2. If `use_errno`: uses `perror()` and includes errno description
3. If not `use_errno`: uses `get_error_message()` for app error codes
4. Logs to `LOGE()` or `LOGW()` based on level
5. For `ERR_FATAL`: calls `log_close()` then `exit(EXIT_FAILURE)`

**Implementation Highlights:**
```c
void handle_error(error_level_t level, const char *context, 
                 app_error_t err_code, int use_errno, 
                 const char *fmt, ...) {
    char buffer[512];
    char final_msg[1024];
    
    // Format the custom message
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    if (use_errno) {
        // System error - include errno info
        snprintf(final_msg, sizeof(final_msg), 
                "[%s] %s: %s - %s (errno=%d)", 
                error_level_str[level], context, buffer, 
                strerror(errno), errno);
        perror(...);
    } else {
        // Application error
        const char *err_msg = get_error_message(err_code);
        snprintf(final_msg, sizeof(final_msg), 
                "[%s] %s: %s - %s (code=%d)", 
                error_level_str[level], context, buffer, 
                err_msg, err_code);
    }
    
    // Log and possibly exit
    switch (level) {
        case ERR_FATAL:
            LOGE("%s", final_msg);
            log_close();
            exit(EXIT_FAILURE);
        case ERR_ERROR:
            LOGE("%s", final_msg);
            break;
        case ERR_WARNING:
            LOGW("%s", final_msg);
            break;
    }
}
```

#### get_error_message
[\<get_error_message\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/error_handler.c?plain=1#L39-L44)

Get human-readable message for an application error code:

```c
const char* get_error_message(app_error_t err_code);
```

### Validation Functions
[\<Validation Functions\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/error_handler.c?plain=1#L100-L135)

Utility functions for input validation with automatic error reporting:

#### validate_int_range
[\<validate_int_range\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/error_handler.c?plain=1#L100-L107)

```c
int validate_int_range(int value, int min, int max, const char *context);
```

**Returns:** `0` on success, `-1` if out of range

**Example:**
```c
if (validate_int_range(num_units, 1, MAX_UNITS, "spawn_units") != 0) {
    return -1;  // Error already logged
}
```

#### validate_coordinate
[\<validate_coordinate\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/error_handler.c?plain=1#L109-L117)

```c
int validate_coordinate(int x, int y, int max_x, int max_y, const char *context);
```

**Returns:** `0` if valid, `-1` otherwise

**Example:**
```c
if (validate_coordinate(pos_x, pos_y, MAP_WIDTH, MAP_HEIGHT, "spawn_pos") != 0) {
    return -1;  // Error already logged
}
```

#### validate_string
[\<validate_string\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/error_handler.c?plain=1#L119-L135)

```c
int validate_string(const char *str, size_t min_len, size_t max_len, const char *context);
```

**Parameters:**
- `str`: String to validate
- `min_len`: Minimum length (inclusive)
- `max_len`: Maximum length (inclusive)
- `context`: Context string for error messages

**Returns:** `0` if valid, `-1` otherwise

**Example:**
```c
if (validate_string(scenario_file, 1, 256, "scenario_file") != 0) {
    return -1;  // Error already logged
}
```

---

## Logging System

The logging system provides comprehensive diagnostic output with minimal performance overhead.

### Log Levels
[\<log_level_t\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/log.h?plain=1#L18-L23)

Four log levels with increasing severity:

```c
typedef enum {
    LOG_LVL_DEBUG = 0,
    LOG_LVL_INFO  = 1,
    LOG_LVL_WARN  = 2,
    LOG_LVL_ERROR = 3
} log_level_t;
```

#### Log Level Guidelines

| Level   | Usage                                    | Examples                                  |
|---------|------------------------------------------|-------------------------------------------|
| DEBUG   | Detailed state dumps, function entry/exit| "Entering tick_sync(), tick=42"          |
| INFO    | Lifecycle events, milestones             | "Shared memory initialized"               |
| WARN    | Recoverable issues, deprecated features  | "Unit spawn outside map - clamping"       |
| ERROR   | Operation failures, exceptions           | "Failed to acquire semaphore: timeout"    |

### Log Infrastructure
[\<Logging Backend\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/utils.c?plain=1#L1-L45)

#### Global State

```c
static FILE *g_logf = NULL;                    // Per-process log file
static int g_all_fd = -1;                      // ALL.log file descriptor
static log_level_t g_min_lvl = LOG_LVL_DEBUG;  // Minimum level to log
static char g_role[8] = "??";                  // Process role (CC, BS, etc.)
static uint16_t g_unit_id = 0;                 // Unit ID if applicable
static char g_run_dir[512] = "logs";           // Run directory path
```

#### Run Directory Management

Logs are stored in timestamped directories:

```bash
logs/run_2026-02-04_12-46-08_pid1289/
```

**Directory Resolution:**
1. Check `SKIRMISH_RUN_DIR` environment variable
2. Read from `/tmp/skirmish_run_dir.txt` if variable not set
3. Fail if neither available

### Log Initialization

#### log_init
[\<log_init\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/utils.c?plain=1#L81-L149)

```c
int log_init(const char *role, int16_t unit_id);
```

**Parameters:**
- `role`: Process role ("CC", "BS", "SQ", etc. - max 7 chars)
- `unit_id`: Unit ID (0 for CC, positive for units)

**Behavior:**
1. Resolves run directory from `SKIRMISH_RUN_DIR` env or `/tmp/skirmish_run_dir.txt`
2. Ensures `logs/` and run directory exist
3. Creates per-process log file: `<run_dir>/<role>_pid_<pid>.log` or `<run_dir>/<role>_u<id>_pid_<pid>.log`
4. Opens combined log: `<run_dir>/ALL.log` with `O_APPEND`
5. Sets per-process log to line-buffered
6. Logs startup header

**Implementation:**
```c
int log_init(const char *role, int16_t unit_id) {
    // Pick run directory from env if set
    const char *rd = getenv("SKIRMISH_RUN_DIR");
    if (rd && *rd) {
        strncpy(g_run_dir, rd, sizeof(g_run_dir) - 1);
    } else {
        // Try to read from CC's run_dir file
        FILE *f = fopen("/tmp/skirmish_run_dir.txt", "r");
        if (f) {
            fgets(g_run_dir, sizeof(g_run_dir), f);
            fclose(f);
        }
    }

    ensure_dir_exists("logs");
    ensure_dir_exists(g_run_dir);

    g_unit_id = unit_id;
    strncpy(g_role, role ? role : "??", sizeof(g_role) - 1);

    char path[600];
    pid_t pid = getpid();

    if (unit_id == 0) {
        snprintf(path, sizeof(path), "%s/%s_pid_%d.log", 
                 g_run_dir, g_role, (int)pid);
    } else {
        snprintf(path, sizeof(path), "%s/%s_u%u_pid_%d.log",
                 g_run_dir, g_role, (unsigned)unit_id, (int)pid);
    }

    g_logf = fopen(path, "a");
    setvbuf(g_logf, NULL, _IOLBF, 0);  // Line-buffered

    open_global_log();  // Opens ALL.log with O_APPEND

    log_msg(LOG_LVL_INFO, "logger started (role=%s unit=%u pid=%d)",
            g_role, (unsigned)unit_id, (int)pid);
    return 0;
}
```

### Logging API
[\<Logging API\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/log.h?plain=1#L25-L48)

#### Convenience Macros
[\<Log Macros\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/log.h?plain=1#L44-L48)

```c
#define LOGD(...) log_msg(LOG_LVL_DEBUG, __VA_ARGS__)
#define LOGI(...) log_msg(LOG_LVL_INFO,  __VA_ARGS__)
#define LOGW(...) log_msg(LOG_LVL_WARN,  __VA_ARGS__)
#define LOGE(...) log_msg(LOG_LVL_ERROR, __VA_ARGS__)
```

**Usage:**
```c
LOGI("System initialized with %d units", num_units);
LOGW("Semaphore wait timed out - retrying");
LOGE("Failed to parse scenario file: %s", error_msg);
LOGD("State transition: %s -> %s", old_state, new_state);
```

#### log_msg
[\<log_msg\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/utils.c?plain=1#L177-L233)

Core logging function:

```c
void log_msg(log_level_t lvl, const char *fmt, ...);
```

**Format:**
```
YYYY-MM-DD HH:MM:SS.mmm [LEVEL] ROLE u=UNIT_ID pid=PID: message
```

**Example Output:**
```
2026-02-04 12:46:12.345 [INFO] CC u=0 pid=1289: Shared memory initialized
2026-02-04 12:46:12.456 [DEBUG] Flagship0 u=1 pid=1301: Entering combat tick
2026-02-04 12:46:12.567 [WARN] Battleship1 u=3 pid=1303: Target out of range
2026-02-04 12:46:12.678 [ERROR] UI u=0 pid=1294: ncurses init failed
```

**Implementation Highlights:**
```c
void log_msg(log_level_t lvl, const char *fmt, ...) {
    if (lvl < g_min_lvl) return;  // Level filtering
    
    char line[1024];
    
    // Timestamp with millisecond precision
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    
    // Format prefix
    int len = snprintf(line, sizeof(line),
        "%04d-%02d-%02d %02d:%02d:%02d.%03ld [%s] %s u=%u pid=%d: ",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000,
        level_name(lvl), g_role, g_unit_id, getpid());
    
    // Format message
    va_list ap;
    va_start(ap, fmt);
    len += vsnprintf(line + len, sizeof(line) - len, fmt, ap);
    va_end(ap);
    
    // Add newline
    if (len < sizeof(line) - 1) {
        line[len++] = '\n';
        line[len] = '\0';
    }
    
    // Write to per-process log (buffered)
    if (g_logf) {
        fwrite(line, 1, len, g_logf);
        fflush(g_logf);  // Ensure immediate flush
    }
    
    // Write to combined log (atomic with O_APPEND)
    if (g_all_fd != -1) {
        write(g_all_fd, line, len);
    }
}
```

#### log_close
[\<log_close\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/utils.c?plain=1#L151-L170)

```c
void log_close(void);
```

**Behavior:**
- Logs "logger closing" message
- Closes per-process log file
- Closes ALL.log file descriptor
- Idempotent (safe to call multiple times)

#### log_set_level
[\<log_set_level\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/utils.c?plain=1#L172-L175)

```c
void log_set_level(log_level_t lvl);
```

**Usage:**
```c
// Reduce verbosity in production
log_set_level(LOG_LVL_INFO);
```

### Log File Organization

#### Per-Process Logs

Each process writes to its own log file with PID and unit ID:

```
logs/run_2026-02-04_12-46-08_pid1289/
├── CC_pid_1289.log           # Command Center
├── UI_pid_1294.log           # User Interface
├── CM_pid_1295.log           # Console Manager
├── FS_u1_pid_1301.log        # Flagship unit 1
├── BS_u2_pid_1302.log        # Battleship unit 2
├── BS_u3_pid_1303.log        # Battleship unit 3
└── SQ_u4_pid_1304.log        # Squadron unit 4
```

**Advantages:**
- Easy to follow single process activity
- No contention (single writer per file)
- Can be monitored independently: `tail -f CC.log`

#### Combined ALL.log

All processes write to a single combined log using atomic append:

```c
// Opened with O_APPEND flag
g_all_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);

// Atomic write (kernel guarantees atomicity for O_APPEND)
write(g_all_fd, line, len);
```

**Advantages:**
- Single timeline view of entire system
- Easier correlation of events across processes
- Natural interleaving shows causality

**POSIX Guarantees:**
- `write()` with `O_APPEND` is atomic for <= PIPE_BUF bytes (4096 on Linux)
- Log lines are typically 150-300 bytes, well within limit
- No need for explicit locking

---

## Terminal Tee System
[\<terminal_tee.c\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c)

The terminal tee system captures all stdout/stderr output and redirects it to multiple destinations simultaneously.

### Purpose and Design

**Problem:** The simulation produces diagnostic output from multiple processes. We need to:
1. Display output to the terminal for interactive use
2. Capture output to a log file for post-mortem analysis
3. Optionally feed output to the UI's STD window
4. Survive process termination and signals

**Solution:** A standalone "tee" background process that:
- Reads from a pipe connected to CC's stdout/stderr
- Writes to `/dev/tty` (real terminal)
- Appends to `ALL.term.log`
- Optionally writes to `ui_stdout.fifo` if UI is connected

### Double-Fork Architecture

The terminal tee uses a **double-fork pattern** to create a truly independent background process:

```
Command Center (parent)
    │
    └─ fork() ─────────────> Short-lived child
                               │
                               └─ fork() ────> Terminal Tee Worker
                                     │         (grandchild)
                                     │
                                exit(0)

CC waits for short-lived child (exits immediately)
Terminal Tee Worker becomes orphan, reparented to init
```

**Why Double-Fork?**

1. **No Zombie Processes:** CC can `waitpid()` on short-lived child immediately
2. **True Independence:** Grandchild is reparented to init, not tied to CC lifecycle
3. **Signal Isolation:** Terminal tee ignores SIGINT/SIGTERM, survives CC shutdown
4. **Resource Cleanup:** init reaps terminal tee when pipe closes

### Output Redirection

#### Pipe Creation and Redirection
[\<start_terminal_tee\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c?plain=1#L27-L121)

```c
pid_t start_terminal_tee(const char *run_dir) {
    int pfd[2];
    if (pipe(pfd) == -1) return -1;
    
    // ... fork logic ...
    
    // In parent (CC):
    close(pfd[0]);  // Close read end
    
    // Redirect stdout/stderr to pipe write end
    setvbuf(stdout, NULL, _IOLBF, 0);  // Line-buffered
    setvbuf(stderr, NULL, _IOLBF, 0);
    
    dup2(pfd[1], STDOUT_FILENO);
    dup2(pfd[1], STDERR_FILENO);
    close(pfd[1]);
    
    return shortlived_pid;
}
```

#### Multi-Destination Tee

The terminal tee process uses a shell script to replicate output:

```bash
#!/bin/sh
# argv[1] = path to ALL.term.log

TERMOUT=/dev/tty
[ -w "$TERMOUT" ] || TERMOUT=/proc/self/fd/1

cat | tee -a "$1" > "$TERMOUT"
```

**Behavior:**
- `cat` reads from stdin (the pipe)
- `tee -a "$1"` appends to ALL.term.log
- Output goes to `/dev/tty` (actual terminal)
- Falls back to `/proc/self/fd/1` if `/dev/tty` unavailable

**Process Name:**
```c
char *argv[] = { (char*)"terminal_tee", (char*)"-c", (char*)script,
                 (char*)"sh", (char*)out_path, NULL };
execv("/bin/sh", argv);
```

Setting `argv[0]` to `"terminal_tee"` makes the process identifiable in `ps` output.

### Signal Handling
[\<ignore_sig\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c?plain=1#L18-L25)

The terminal tee process ignores termination signals:

```c
static void ignore_sig(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(sig, &sa, NULL);
}

// In grandchild:
ignore_sig(SIGINT);   // User Ctrl+C won't kill tee
ignore_sig(SIGTERM);  // Kill command won't stop tee
```

**Rationale:**
- Terminal tee should exit only when pipe closes (EOF)
- Prevents premature termination during debugging (Ctrl+C)
- User can still kill CC; tee will exit when pipe breaks
- Allows graceful shutdown even if CC crashes

#### Process Naming (Linux-specific)

```c
#ifdef __linux__
prctl(PR_SET_NAME, "terminal_tee", 0, 0, 0);
#endif
```

Sets process name visible in `/proc/PID/comm` and `ps`.

### Lifecycle

```
1. CC starts up
   ↓
2. CC calls tee_spawn_and_redirect()
   ↓
3. Pipe created, double-fork spawns terminal_tee
   ↓
4. CC stdout/stderr redirected to pipe
   ↓
5. Terminal tee reads from pipe, writes to:
   - /dev/tty (terminal)
   - ALL.term.log (log file)
   - ui_stdout.fifo (if UI connected)
   ↓
6. CC runs simulation
   ↓
7. CC exits (or crashes)
   ↓
8. Pipe write end closes
   ↓
9. Terminal tee receives EOF
   ↓
10. Terminal tee exits gracefully
```

---

## API Reference

### Error Handling API
[\<error_handler.h\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/error_handler.h)

#### Macros

```c
CHECK_SYS_CALL(call, context)
```
- **Purpose:** Wrap system calls for automatic error handling
- **Parameters:** `call` - system call expression, `context` - context string
- **Returns:** Result of `call` (preserves return value)
- **Side Effects:** Calls `HANDLE_SYS_ERROR` if `call == -1` (exits program)

```c
CHECK_SYS_CALL_NONFATAL(call, context)
```
- **Purpose:** Wrap system calls for non-fatal error handling
- **Parameters:** `call` - system call expression, `context` - context string
- **Returns:** Result of `call`
- **Side Effects:** Logs error but doesn't exit if `call == -1`

```c
HANDLE_SYS_ERROR(context, msg)
```
- **Purpose:** Log system call error with errno and terminate
- **Parameters:** 
  - `context` - Context string (e.g., function name)
  - `msg` - Error message
- **Returns:** Does not return (calls `exit(EXIT_FAILURE)`)

```c
HANDLE_APP_ERROR(level, context, err_code, msg)
```
- **Purpose:** Handle application-specific errors
- **Parameters:** 
  - `level` - Error severity
  - `context` - Context string
  - `err_code` - Application error code
  - `msg` - Error message
- **Returns:** void (exits if `level == ERR_FATAL`)

#### Functions

```c
void handle_error(error_level_t level, const char *context, 
                 app_error_t err_code, int use_errno, 
                 const char *fmt, ...);
```
- **Purpose:** Central error handling with formatted message
- **Parameters:**
  - `level` - Error severity (ERR_FATAL/ERR_ERROR/ERR_WARNING)
  - `context` - Context string
  - `err_code` - Application error code (or ERR_OK if using errno)
  - `use_errno` - If 1, use perror() with errno; if 0, use err_code
  - `fmt` - printf-style format string
  - `...` - Format arguments
- **Returns:** void (exits if `level == ERR_FATAL`)

```c
int validate_int_range(int value, int min, int max, const char *context);
```
- **Purpose:** Validate integer is within range
- **Parameters:**
  - `value` - Integer to validate
  - `min` - Minimum allowed value (inclusive)
  - `max` - Maximum allowed value (inclusive)
  - `context` - Context for error messages
- **Returns:** 0 on success, -1 if out of range

```c
int validate_coordinate(int x, int y, int max_x, int max_y, const char *context);
```
- **Purpose:** Validate 2D coordinate is within bounds
- **Parameters:**
  - `x`, `y` - Coordinates to validate
  - `max_x`, `max_y` - Maximum values (exclusive)
  - `context` - Context for error messages
- **Returns:** 0 if valid, -1 otherwise

```c
int validate_string(const char *str, size_t min_len, size_t max_len, const char *context);
```
- **Purpose:** Validate string length is within bounds
- **Parameters:**
  - `str` - String to validate
  - `min_len` - Minimum length (inclusive)
  - `max_len` - Maximum length (inclusive)
  - `context` - Context for error messages
- **Returns:** 0 if valid, -1 otherwise

---

### Logging API
[\<log.h\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/log.h)

#### Macros

```c
LOGD(...)  // Log at DEBUG level
LOGI(...)  // Log at INFO level
LOGW(...)  // Log at WARN level
LOGE(...)  // Log at ERROR level
```
- **Purpose:** Convenience macros for logging at specific levels
- **Parameters:** printf-style format string and arguments
- **Returns:** void

#### Functions

```c
int log_init(const char *role, int16_t unit_id);
```
- **Purpose:** Initialize logging system for current process
- **Parameters:**
  - `role` - Process role string (e.g., "CC", "BS", "SQ" - max 7 chars)
  - `unit_id` - Unit ID (0 for CC, positive for units)
- **Returns:** 0 on success, -1 on failure
- **Note:** Must be called before any logging functions

```c
void log_close(void);
```
- **Purpose:** Close log files and clean up resources
- **Parameters:** None
- **Returns:** void
- **Note:** Idempotent (safe to call multiple times)

```c
void log_msg(log_level_t lvl, const char *fmt, ...);
```
- **Purpose:** Log a formatted message at specified level
- **Parameters:**
  - `lvl` - Log level (LOG_LVL_DEBUG/INFO/WARN/ERROR)
  - `fmt` - printf-style format string
  - `...` - Format arguments
- **Returns:** void

```c
void log_printf(const char *fmt, ...);
```
- **Purpose:** Write same line to stdout and logs
- **Parameters:** printf-style format string and arguments
- **Returns:** void

```c
void log_set_level(log_level_t lvl);
```
- **Purpose:** Set minimum log level (filter out lower levels)
- **Parameters:**
  - `lvl` - Minimum level to log
- **Returns:** void
- **Example:** `log_set_level(LOG_LVL_INFO)` - suppress DEBUG messages

---

### Terminal Tee API
[\<tee_spawn.h\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/tee_spawn.h)

```c
int tee_spawn_and_redirect(const char *run_dir);
```
- **Purpose:** Spawn terminal tee and redirect stdout/stderr
- **Parameters:**
  - `run_dir` - Run directory for ALL.term.log
- **Returns:** 0 on success, -1 on failure
- **Side Effects:** 
  - Forks two processes (short-lived child + terminal_tee)
  - Redirects calling process stdout/stderr to pipe
  - Sets line-buffered mode on stdout/stderr
- **Note:** Call early in main() before other output

```c
pid_t start_terminal_tee(const char *run_dir);
```
- **Purpose:** Internal implementation of terminal tee spawning
- **Parameters:**
  - `run_dir` - Run directory for log file
- **Returns:** PID of short-lived child (for waitpid), -1 on error
- **Note:** Use `tee_spawn_and_redirect()` instead

---

## Usage Examples

### Error Handling Examples

#### Basic System Call Checking

```c
#include "error_handler.h"

void init_shared_memory(const char *name) {
    int shm_fd;
    
    // Check system call - exits on failure
    shm_fd = CHECK_SYS_CALL(shm_open(name, O_CREAT | O_RDWR, 0600), "init_shm");
    
    // Resize shared memory
    CHECK_SYS_CALL(ftruncate(shm_fd, 4096), "init_shm");
    
    LOGI("Shared memory %s initialized successfully", name);
}
```

#### Recoverable Error Handling

```c
int acquire_semaphore_with_timeout(int sem_id) {
    struct sembuf op = { .sem_num = 0, .sem_op = -1, .sem_flg = 0 };
    
    int ret = CHECK_SYS_CALL_NONFATAL(semop(sem_id, &op, 1), "semop");
    if (ret == -1) {
        if (errno == EINTR) {
            // Interrupted - recoverable
            LOGW("Semaphore operation interrupted - retrying");
            return -1;  // Caller can retry
        }
        // Other errors already logged by macro
        return -1;
    }
    
    return 0;
}
```

#### Input Validation

```c
int parse_scenario_config(const char *filename, scenario_t *scenario) {
    if (validate_string(filename, 1, 256, "scenario_file") != 0) {
        return -1;  // Error already logged
    }
    
    FILE *f = fopen(filename, "r");
    if (!f) {
        HANDLE_SYS_ERROR("parse_scenario", "fopen failed");
    }
    
    // Parse configuration...
    int num_units;
    fscanf(f, "num_units=%d", &num_units);
    
    if (validate_int_range(num_units, 1, MAX_UNITS, "parse_scenario") != 0) {
        fclose(f);
        return -1;  // Error already logged
    }
    
    scenario->num_units = num_units;
    fclose(f);
    return 0;
}
```

---

### Logging Examples

#### Process Initialization

```c
#include "log.h"

int main(int argc, char **argv) {
    // Initialize logging early
    if (log_init("CC", 0) != 0) {
        fprintf(stderr, "Failed to initialize logging\n");
        return 1;
    }
    
    LOGI("Command Center starting (pid=%d)", getpid());
    LOGD("Arguments: argc=%d, argv[0]=%s", argc, argv[0]);
    
    // ... main logic ...
    
    LOGI("Command Center shutting down");
    log_close();
    return 0;
}
```

#### State Transitions

```c
void unit_state_machine(unit_t *unit) {
    const char *old_state = state_name(unit->state);
    
    switch (unit->state) {
        case STATE_IDLE:
            if (has_target(unit)) {
                unit->state = STATE_ENGAGING;
                LOGI("Unit %u: %s -> ENGAGING (target acquired)", 
                     unit->id, old_state);
            }
            break;
            
        case STATE_ENGAGING:
            if (!has_target(unit)) {
                unit->state = STATE_IDLE;
                LOGI("Unit %u: %s -> IDLE (target lost)", 
                     unit->id, old_state);
            } else if (is_dead(unit->target)) {
                unit->state = STATE_IDLE;
                LOGI("Unit %u: %s -> IDLE (target destroyed)", 
                     unit->id, old_state);
            }
            break;
    }
}
```

#### Error Context

```c
int process_message(msgbuf_t *msg) {
    LOGD("Processing message: type=%d, size=%zu", msg->mtype, sizeof(*msg));
    
    if (msg->mtype < 0 || msg->mtype > MAX_MSG_TYPE) {
        LOGE("Invalid message type: %d (expected 0-%d)", 
             msg->mtype, MAX_MSG_TYPE);
        return -1;
    }
    
    switch (msg->mtype) {
        case MSG_FIRE:
            LOGD("FIRE command: from=%u, target=%u", 
                 msg->data.fire.from_id, msg->data.fire.target_id);
            handle_fire_command(&msg->data.fire);
            break;
            
        default:
            LOGW("Unhandled message type: %d", msg->mtype);
            return -1;
    }
    
    return 0;
}
```

#### Performance Measurement

```c
void tick_simulation(simulation_t *sim) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    LOGD("Tick %u starting", sim->tick);
    
    // ... simulation logic ...
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    long elapsed_us = (end.tv_sec - start.tv_sec) * 1000000 +
                      (end.tv_nsec - start.tv_nsec) / 1000;
    
    LOGD("Tick %u completed in %ld μs", sim->tick, elapsed_us);
    
    if (elapsed_us > TICK_BUDGET_US) {
        LOGW("Tick %u exceeded budget: %ld μs > %d μs", 
             sim->tick, elapsed_us, TICK_BUDGET_US);
    }
}
```

---

### Terminal Tee Examples

#### Basic Initialization

```c
#include "tee_spawn.h"
#include "log.h"

int main(void) {
    // Determine run directory
    char run_dir[256];
    const char *env_run = getenv("SKIRMISH_RUN_DIR");
    if (env_run) {
        snprintf(run_dir, sizeof(run_dir), "%s", env_run);
    } else {
        // Create timestamped directory
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        snprintf(run_dir, sizeof(run_dir), 
                 "logs/run_%04d-%02d-%02d_%02d-%02d-%02d_pid%d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec, getpid());
        mkdir(run_dir, 0755);
    }
    
    // Spawn terminal tee
    if (tee_spawn_and_redirect(run_dir) != 0) {
        fprintf(stderr, "Failed to spawn terminal tee\n");
        return 1;
    }
    
    // Initialize logging after tee redirection
    log_init("CC", 0);
    
    // All stdout/stderr now goes to:
    // - Terminal (/dev/tty)
    // - logs/run_YYYY-MM-DD_HH-MM-SS_pidXXXX/ALL.term.log
    
    printf("This appears on terminal AND in ALL.term.log\n");
    LOGI("This goes to CC.log and ALL.log");
    
    // ... run simulation ...
    
    return 0;
}
```

#### Output Verification

```bash
# Terminal output is visible in real-time:
$ ./command_center
[INFO] Command Center starting (pid=1289)
[INFO] Spawning 4 units...
[INFO] Unit 1 (Flagship0) spawned at (10, 20)
...

# AND captured in log file:
$ cat logs/run_2026-02-04_12-46-08_pid1289/ALL.term.log
[INFO] Command Center starting (pid=1289)
[INFO] Spawning 4 units...
[INFO] Unit 1 (Flagship0) spawned at (10, 20)
...
```

#### Integration with UI

The terminal tee can optionally feed output to the UI's STD window via a FIFO:

```c
// In terminal_tee.c (future enhancement):
int ui_fifo = open("logs/run_XXX/ui_stdout.fifo", O_WRONLY | O_NONBLOCK);
if (ui_fifo != -1) {
    // Tee to UI as well
    write(ui_fifo, buffer, len);
}
```

---

## Integration Patterns

### Typical Initialization Sequence

```c
int main(int argc, char **argv) {
    // 1. Set up run directory
    char run_dir[256];
    create_run_directory(run_dir, sizeof(run_dir));
    setenv("SKIRMISH_RUN_DIR", run_dir, 1);
    
    // 2. Spawn terminal tee (redirects stdout/stderr)
    if (tee_spawn_and_redirect(run_dir) != 0) {
        fprintf(stderr, "Terminal tee failed\n");
        return 1;
    }
    
    // 3. Initialize logging (uses redirected output)
    if (log_init("CC", 0) != 0) {
        fprintf(stderr, "Logging init failed\n");
        return 1;
    }
    
    LOGI("Initialization complete");
    
    // 4. Main application logic with error handling
    if (init_shared_memory() != 0) {
        handle_error(ERR_FATAL, ERR_SHM_ERROR, "SHM init failed");
    }
    
    // 5. Cleanup
    log_close();
    return 0;
}
```

### Error Handling + Logging Pattern

```c
int critical_operation(void) {
    LOGI("Starting critical operation");
    
    int fd;
    if (CHECK_SYS_CALL(fd = open("/dev/important", O_RDWR)) == -1) {
        // Error already logged by CHECK_SYS_CALL
        handle_error(ERR_ERROR, ERR_FILE_ERROR, 
                    "Failed to open critical device");
        return -1;  // Recoverable error
    }
    
    LOGD("Device opened successfully (fd=%d)", fd);
    
    // ... perform operation ...
    
    if (CHECK_SYS_CALL_NONFATAL(close(fd)) == -1) {
        // Logged but not fatal
        LOGW("Failed to close fd %d (errno=%d)", fd, errno);
    }
    
    LOGI("Critical operation completed successfully");
    return 0;
}
```

### Multi-Process Logging Coordination

```c
// In parent (Command Center):
void spawn_unit_processes(int num_units) {
    for (int i = 0; i < num_units; i++) {
        pid_t pid = fork();
        
        if (pid == 0) {
            // Child process
            char role[32];
            snprintf(role, sizeof(role), "Battleship%d", i);
            
            // Re-initialize logging with child's role
            log_close();  // Close parent's log files
            log_init(role, i + 1);
            
            LOGI("Unit process started");
            unit_main(i);
            log_close();
            exit(0);
        } else if (pid > 0) {
            LOGI("Spawned unit process: %s (pid=%d)", 
                 unit_name(i), pid);
        } else {
            LOGE("Failed to spawn unit %d: %s", i, strerror(errno));
        }
    }
}
```

Result: Each process writes to its own log (`Battleship0.log`, etc.) and all contribute to `ALL.log` atomically.

---

## Best Practices

### Error Handling

1. **Use Macros Consistently**
   - Always use `CHECK_SYS_CALL` for system calls
   - Use `HANDLE_SYS_ERROR` for fatal system call failures
   - Prefer macros over manual errno checking

2. **Choose Appropriate Error Levels**
   - `ERR_FATAL`: Unrecoverable failures (e.g., SHM init)
   - `ERR_ERROR`: Recoverable failures (e.g., retry-able operations)
   - `ERR_WARNING`: Expected edge cases (e.g., empty message queue)

3. **Provide Context in Error Messages**
   ```c
   // Bad:
   handle_error(ERR_FATAL, ERR_FILE_ERROR, "File error");
   
   // Good:
   handle_error(ERR_FATAL, ERR_FILE_ERROR, 
               "Cannot open scenario file '%s': %s", 
               filename, strerror(errno));
   ```

4. **Validate Early**
   ```c
   // Validate inputs before expensive operations
   if (validate_int_range(num_units, 1, MAX_UNITS, "num_units") != 0) {
       return ERR_INVALID_INPUT;
   }
   // ... proceed with initialization ...
   ```

5. **Clean Up on Error Paths**
   ```c
   int init_resources(void) {
       if (init_shm() != 0) goto err_shm;
       if (init_sem() != 0) goto err_sem;
       if (init_msgq() != 0) goto err_msgq;
       return 0;
       
   err_msgq:
       cleanup_sem();
   err_sem:
       cleanup_shm();
   err_shm:
       return -1;
   }
   ```

### Logging

1. **Log at Appropriate Levels**
   - DEBUG: Function entry/exit, detailed state dumps
   - INFO: Lifecycle events, milestones
   - WARN: Recoverable issues, unusual conditions
   - ERROR: Operation failures

2. **Include Relevant Context**
   ```c
   // Include IDs, timestamps, state information
   LOGI("Unit %u transitioning %s -> %s (tick=%u)", 
        unit->id, old_state, new_state, sim->tick);
   ```

3. **Avoid Logging in Hot Paths**
   ```c
   // Bad: Logs on every iteration
   for (int i = 0; i < 1000000; i++) {
       LOGD("Processing item %d", i);
       process(i);
   }
   
   // Good: Log summary
   LOGD("Processing %d items...", count);
   for (int i = 0; i < count; i++) {
       process(i);
   }
   LOGD("Processed %d items successfully", count);
   ```

4. **Use Structured Logging**
   ```c
   // Consistent format for parsing/analysis
   LOGI("COMBAT unit=%u target=%u damage=%d hp=%d",
        attacker_id, target_id, damage, target_hp);
   ```

5. **Flush Logs Before Exits**
   ```c
   LOGI("Fatal error - terminating");
   log_close();  // Ensures logs are flushed
   exit(1);
   ```

### Terminal Tee

1. **Spawn Early**
   ```c
   // Spawn tee before other output
   int main(void) {
       tee_spawn_and_redirect(run_dir);  // First
       log_init("CC", 0);                 // Then logging
       // ... rest of init ...
   }
   ```

2. **Line-Buffered Output**
   ```c
   // Tee already sets line-buffering, but for custom I/O:
   setvbuf(stdout, NULL, _IOLBF, 0);
   ```

3. **Verify Tee Process**
   ```bash
   # Check tee is running
   ps aux | grep terminal_tee
   
   # Monitor output
   tail -f logs/run_XXX/ALL.term.log
   ```

4. **Handle Tee Failure Gracefully**
   ```c
   if (tee_spawn_and_redirect(run_dir) != 0) {
       // Tee failed, but continue anyway
       fprintf(stderr, "Warning: Terminal tee failed\n");
       // Logs will still work (just no ALL.term.log)
   }
   ```

---

## Debugging and Troubleshooting

### Common Error Handling Issues

**Problem:** Errors not being logged

**Solution:**
```c
// Ensure logging is initialized before error handling
log_init("MyProcess", 0);

// Now errors will be logged properly
if (CHECK_SYS_CALL(fd = open(path, O_RDONLY)) == -1) {
    HANDLE_SYS_ERROR("open", ERR_FILE_ERROR);
}
```

**Problem:** errno value incorrect

**Solution:**
```c
// Don't call functions between system call and CHECK_SYS_CALL
int fd = open(path, O_RDONLY);
printf("Opening file...\n");  // BAD: May modify errno
if (CHECK_SYS_CALL(fd) == -1) { ... }

// Instead:
if (CHECK_SYS_CALL(fd = open(path, O_RDONLY)) == -1) { ... }
printf("File opened\n");  // OK: After check
```

### Common Logging Issues

**Problem:** Logs not appearing in ALL.log

**Diagnosis:**
```bash
# Check file permissions
ls -l logs/run_XXX/ALL.log

# Check if file descriptor is open
lsof -p <PID> | grep ALL.log

# Check for write errors
strace -e write -p <PID>
```

**Solution:**
```c
// Ensure ALL.log is opened with O_APPEND
g_all_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);

// Check for errors
if (g_all_fd == -1) {
    perror("open ALL.log");
    // Fall back to per-process log only
}
```

**Problem:** Truncated log messages

**Cause:** Log line exceeds buffer size (1024 bytes)

**Solution:**
```c
// Keep log messages concise
LOGI("Short message with essential info: unit=%u, hp=%d", id, hp);

// For large dumps, use multiple lines
LOGD("State dump for unit %u:", id);
LOGD("  Position: (%d, %d)", x, y);
LOGD("  HP: %d/%d", current_hp, max_hp);
```

**Problem:** Logs out of order in ALL.log

**Cause:** Multiple processes writing simultaneously without synchronization

**Explanation:** This is expected! POSIX guarantees atomic append for writes ≤ PIPE_BUF, but not temporal ordering across processes.

**Solution:**
```bash
# Sort logs by timestamp for analysis
grep "^\[2026-02-04" ALL.log | sort
```

### Common Terminal Tee Issues

**Problem:** Terminal tee not capturing output

**Diagnosis:**
```bash
# Check if tee process is running
ps aux | grep terminal_tee

# Check pipe connections
lsof -p <CC_PID> | grep pipe
lsof -p <TEE_PID> | grep pipe

# Check ALL.term.log
ls -l logs/run_XXX/ALL.term.log
```

**Solution:**
```c
// Ensure tee is spawned successfully
if (tee_spawn_and_redirect(run_dir) != 0) {
    fprintf(stderr, "Tee spawn failed: %s\n", strerror(errno));
    return 1;
}

// Verify redirection worked
printf("Test output to tee\n");
fflush(stdout);
```

**Problem:** Tee exits prematurely

**Cause:** CC closes stdout before exiting

**Solution:**
```c
// Don't close stdout/stderr explicitly
// Let process exit clean them up automatically

// Bad:
close(STDOUT_FILENO);
exit(0);

// Good:
return 0;  // Automatic cleanup
```

**Problem:** Output appears on terminal but not in log

**Diagnosis:**
```bash
# Check if ALL.term.log is writable
touch logs/run_XXX/ALL.term.log && echo "writable"

# Check for disk space
df -h
```

**Solution:**
```c
// In terminal_tee.c, check for write errors
ssize_t written = write(out_fd, buffer, len);
if (written == -1) {
    perror("write to ALL.term.log");
}
```

---

## Performance Considerations

### Error Handling Overhead

**Macro Cost:**
- `CHECK_SYS_CALL`: Minimal overhead (single comparison, conditional logging)
- Optimized out if system call succeeds
- Only pays logging cost on error path

**Measurement:**
```c
// Negligible impact on hot paths
for (int i = 0; i < 1000000; i++) {
    if (CHECK_SYS_CALL(write(fd, buf, len)) == -1) {
        // Error path: expensive but rare
    }
    // Success path: fast
}
```

### Logging Performance

**Buffering Strategy:**
- Per-process logs: Line-buffered (`_IOLBF`)
- ALL.log: Unbuffered (direct `write()`)
- Explicit `fflush()` after each log ensures visibility

**Atomic Write Cost:**
- `write()` system call: ~1-2 μs latency
- Line-buffered `fwrite()`: amortized cost with fewer syscalls
- Combined overhead: ~5-10 μs per log message

**Optimization Tips:**
1. **Reduce Log Frequency in Hot Paths**
   ```c
   // Log every 1000 iterations instead of every iteration
   if (i % 1000 == 0) {
       LOGD("Processed %d items so far", i);
   }
   ```

2. **Use Appropriate Log Levels**
   ```c
   // Set higher minimum level in production
   log_set_level(LOG_LVL_INFO);  // Drops DEBUG messages
   ```

3. **Batch Related Logs**
   ```c
   // Instead of many small logs:
   LOGD("x=%d", x); LOGD("y=%d", y); LOGD("z=%d", z);
   
   // Use single log:
   LOGD("Position: x=%d, y=%d, z=%d", x, y, z);
   ```

### Terminal Tee Impact

**Overhead:**
- Pipe write: ~2-3 μs per operation
- Tee process runs independently (no blocking)
- Line-buffering minimizes system calls

**Buffering:**
```c
setvbuf(stdout, NULL, _IOLBF, 0);  // Line-buffered
```
- Accumulates output until newline
- Single `write()` per line instead of per character
- Reduces context switches

**Disk I/O:**
- Tee writes to file asynchronously
- OS page cache buffers writes
- Minimal impact on CC performance

---

## References

### Internal Documentation

- [Command Center Module](CC_MODULE.md) - Unit lifecycle, tick synchronization
- [IPC Module](IPC_MODULE.md) - Shared memory, semaphores, message queues
- [UI Module](UI_MODULE.md) - ncurses interface, multi-threading
- [Console Manager Module](CM_MODULE.md) - Interactive command interface

### Source Files

**Error Handling:**
- [\<error_handler.h\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/error_handler.h) - Error types, codes, and macros (146 lines)
- [\<error_handler.c\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/error_handler.c) - Error handling implementation (136 lines)

**Logging:**
- [\<log.h\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/log.h) - Logging API (48 lines)
- [\<utils.c\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/utils.c) - Logging backend (234 lines)

**Terminal Tee:**
- [\<tee_spawn.h\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/include/tee_spawn.h) - Tee spawn interface (14 lines)
- [\<terminal_tee.c\>](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c) - Terminal tee implementation (114 lines)

### External References

**POSIX Standards:**
- [errno(3)](https://man7.org/linux/man-pages/man3/errno.3.html) - Error number definitions
- [strerror(3)](https://man7.org/linux/man-pages/man3/strerror.3.html) - Error string formatting
- [write(2)](https://man7.org/linux/man-pages/man2/write.2.html) - Atomic write with O_APPEND

**Logging:**
- [syslog(3)](https://man7.org/linux/man-pages/man3/syslog.3.html) - System logging facility
- [clock_gettime(2)](https://man7.org/linux/man-pages/man2/clock_gettime.2.html) - High-resolution timestamps

**Process Management:**
- [fork(2)](https://man7.org/linux/man-pages/man2/fork.2.html) - Process creation
- [pipe(2)](https://man7.org/linux/man-pages/man2/pipe.2.html) - Interprocess pipe
- [dup2(2)](https://man7.org/linux/man-pages/man2/dup2.2.html) - File descriptor duplication
- [prctl(2)](https://man7.org/linux/man-pages/man2/prctl.2.html) - Process name setting

**Signal Handling:**
- [signal(7)](https://man7.org/linux/man-pages/man7/signal.7.html) - Signal overview
- [sigaction(2)](https://man7.org/linux/man-pages/man2/sigaction.2.html) - Signal handler setup

---

## Summary

The **Error Handling, Logging, and Terminal Tee** subsystem provides the diagnostic and reliability foundation for Space Skirmish:

- **Error Handling:** Standardized error reporting with three severity levels and 20 application-specific error codes
- **Logging:** Dual logging strategy (per-process + combined) with four log levels and atomic append guarantees
- **Terminal Tee:** Background process for simultaneous terminal/file/UI output with signal tolerance

**Key Benefits:**
- **Reliability:** Consistent error detection and reporting across the entire codebase
- **Observability:** Comprehensive log trail for debugging and analysis
- **Maintainability:** Centralized error/log infrastructure reduces code duplication
- **Robustness:** Graceful degradation and signal handling ensure system resilience

These systems work seamlessly together to provide a production-ready diagnostic and error management infrastructure for the multi-process, IPC-heavy simulation environment.

---

**Document Version:** 1.1  
**Last Updated:** 2026-02-06  
**Author:** Space Skirmish Development Team
