# UI Module Documentation

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Components](#components)
4. [Window Layout](#window-layout)
5. [Threading Model](#threading-model)
6. [Rendering System](#rendering-system)
7. [Data Flow](#data-flow)
8. [API Reference](#api-reference)
9. [Usage Examples](#usage-examples)

---

## Overview

The **UI (User Interface) Module** provides a real-time ncurses-based terminal interface for the Space Skirmish simulation. It displays the battlefield grid, unit statistics, and command center output in a multi-threaded, synchronized visualization system.

### Key Responsibilities
- **Real-time Grid Display**: Visualize M×N battlefield with unit positions
- **Unit Statistics**: Show live unit stats (HP, position, faction)
- **Output Logging**: Display command center and unit logs via FIFO
- **Thread Synchronization**: Coordinate 4 rendering threads with ncurses
- **Color Support**: Faction-based color coding for visual clarity
- **Responsive Layout**: Adaptive window sizing based on terminal dimensions

---

## Architecture

### Process & Thread Model

```
┌─────────────────────────────────────────────┐
│          UI Process (ui_main)                │
│  ┌────────────────────────────────────────┐ │
│  │     Main Thread (Input & Control)      │ │
│  └────────────────────────────────────────┘ │
│                                              │
│  ┌──────────────┐  ┌──────────────┐         │
│  │  MAP Thread  │  │  UST Thread  │         │
│  │  (ui_map.c)  │  │  (ui_ust.c)  │         │
│  └──────────────┘  └──────────────┘         │
│                                              │
│  ┌──────────────┐                           │
│  │  STD Thread  │                           │
│  │  (ui_std.c)  │                           │
│  └──────────────┘                           │
└─────────────────────────────────────────────┘
         │                    │
         │ IPC Shared Memory  │ FIFO
         │                    │
    ┌────▼────────┐      ┌────▼─────┐
    │ Command     │      │ terminal │
    │ Center      │      │   tee    │
    └─────────────┘      └──────────┘
```

### Window Layout

```
┌───────────────────────────────┬─────────────────────┐
│                               │                     │
│         MAP Window            │    UST Window       │
│       (Grid Display)          │  (Unit Statistics)  │
│                               │                     │
│      120 × 40 grid            │  Live unit table    │
│   Faction color-coded         │  HP, faction, pos   │
│                               │                     │
├───────────────────────────────┴─────────────────────┤
│                                                     │
│              STD Window (Output Log)                │
│          Command Center & Unit Logs                 │
│        Scrolling, line-wrapped output               │
│                                                     │
└─────────────────────────────────────────────────────┘
```

**Dimensions**:
- **MAP**: M+2 × N+2 (grid + borders) = 122 × 42
- **UST**: Remaining width × MAP height
- **STD**: Full width × remaining height (minimum 5 lines)

---

## Components

### 1. ui_main.c

**Main UI process** - Initialization, thread management, cleanup.

**Location**: `src/UI/ui_main.c`

**Key Functions**:

#### `ui_init()`
Initialize ncurses and create windows.

**Responsibilities**:
- Initialize ncurses with `initscr()`, `cbreak()`, `noecho()`
- Enable color pairs (4 pairs for factions and highlights)
- Calculate window dimensions based on terminal size
- Create 3 windows: MAP, UST, STD
- Draw initial borders and titles
- Initialize mutex for thread-safe window access

**Error Handling**: Returns 0 on success, -1 on failure.

```c
ui_context_t ui_ctx;
if (ui_init(&ui_ctx, run_dir) == -1) {
    fprintf(stderr, "Failed to initialize UI\n");
    exit(1);
}
```

#### `ui_cleanup()`
Cleanup ncurses and release resources.

**Responsibilities**:
- Delete ncurses windows (`delwin()`)
- End ncurses mode (`endwin()`)
- Close FIFO file descriptors
- Remove FIFO file
- Destroy mutex

```c
ui_cleanup(&ui_ctx);
// Terminal restored to normal mode
```

#### `ui_refresh_all()`
Thread-safe refresh of all windows.

**Thread Safety**: Uses `pthread_mutex_lock()` to prevent concurrent access.

```c
void ui_refresh_all(ui_context_t *ui_ctx) {
    pthread_mutex_lock(&ui_ctx->ui_lock);
    wrefresh(ui_ctx->map_win);
    wrefresh(ui_ctx->ust_win);
    wrefresh(ui_ctx->std_win);
    pthread_mutex_unlock(&ui_ctx->ui_lock);
}
```

#### `main()`
Entry point and thread coordinator.

**Flow**:
1. Parse arguments (`--ftok`, `--run-dir`)
2. Attach to IPC (`ipc_attach()`)
3. Initialize UI (`ui_init()`)
4. Create 3 rendering threads
5. Main loop: handle keyboard input, refresh display
6. On exit: signal threads, join, cleanup

```c
// Create threads
pthread_create(&ui_ctx.map_thread_id, NULL, ui_map_thread, &ui_ctx);
pthread_create(&ui_ctx.ust_thread_id, NULL, ui_ust_thread, &ui_ctx);
pthread_create(&ui_ctx.std_thread_id, NULL, ui_std_thread, &ui_ctx);

// Main loop
while (!ui_ctx.stop) {
    int ch = getch();  // Non-blocking
    if (ch == 'q' || ch == 'Q') {
        ui_ctx.stop = 1;
    }
    ui_refresh_all(&ui_ctx);
    usleep(50000);  // 50ms
}

// Cleanup
pthread_join(ui_ctx.map_thread_id, NULL);
// ... join other threads
ui_cleanup(&ui_ctx);
```

---

### 2. ui_map.c

**MAP thread** - Real-time grid visualization.

**Location**: `src/UI/ui_map.c`

**Rendering Strategy**: Request-response pattern with Command Center.

**Key Functions**:

#### `ui_map_thread()`
Main thread loop for grid display.

**Algorithm**:
```
repeat:
    1. Send MSG_UI_MAP_REQ to Command Center
    2. Block on MSG_UI_MAP_REP (blocking receive)
    3. If rep.ready == 1:
       a. Lock semaphore (SEM_GLOBAL_LOCK)
       b. Copy grid from shared memory
       c. Unlock semaphore
       d. Render grid to MAP window
    4. Sleep briefly (10ms)
goto repeat
```

**Code**:
```c
void* ui_map_thread(void* arg) {
    ui_context_t *ui_ctx = (ui_context_t*)arg;
    
    while (!ui_ctx->stop) {
        // Request snapshot
        mq_ui_map_req_t req = {
            .mtype = MSG_UI_MAP_REQ,
            .sender = getpid()
        };
        mq_send_ui_map_req(ui_ctx->ctx->q_req, &req);
        
        // Wait for response
        mq_ui_map_rep_t rep;
        if (mq_recv_ui_map_rep_blocking(ui_ctx->ctx->q_rep, &rep) > 0) {
            if (rep.ready) {
                // Lock and copy grid
                sem_lock(ui_ctx->ctx->sem_id, SEM_GLOBAL_LOCK);
                unit_id_t grid[M][N];
                memcpy(grid, ui_ctx->ctx->S->grid, sizeof(grid));
                uint32_t tick = ui_ctx->ctx->S->ticks;
                sem_unlock(ui_ctx->ctx->sem_id, SEM_GLOBAL_LOCK);
                
                // Render
                render_map(ui_ctx, grid, tick);
                wrefresh(ui_ctx->map_win);
            }
        }
        
        usleep(10000);  // 10ms
    }
    
    return NULL;
}
```

#### `render_map()`
Draw grid to MAP window.

**Rendering Details**:
- **1:1 Scale**: One grid cell = one terminal character
- **Color Coding**:
  - Republic units: Blue (COLOR_PAIR 1)
  - CIS units: Red (COLOR_PAIR 2)
  - Empty cells: `.`
- **Unit Display**: Last digit of unit ID (`'0' + (id % 10)`)
- **Clipping**: If grid > window, show partial view
- **Header**: Shows grid dimensions, tick counter

**Code**:
```c
static void render_map(ui_context_t *ui_ctx, unit_id_t grid[M][N], uint32_t tick) {
    pthread_mutex_lock(&ui_ctx->ui_lock);
    
    WINDOW *win = ui_ctx->map_win;
    int win_h, win_w;
    getmaxyx(win, win_h, win_w);
    
    // Clear content (preserve border)
    for (int y = 1; y < win_h - 1; y++) {
        wmove(win, y, 1);
        for (int x = 1; x < win_w - 1; x++) {
            waddch(win, ' ');
        }
    }
    
    // Draw grid cells
    for (int gy = 0; gy < N && gy < win_h - 2; gy++) {
        for (int gx = 0; gx < M && gx < win_w - 2; gx++) {
            unit_id_t cell = grid[gx][gy];
            
            int wy = 1 + gy;
            int wx = 1 + gx;
            
            if (cell == 0) {
                mvwaddch(win, wy, wx, '.');
            } else {
                uint8_t faction = ui_ctx->ctx->S->units[cell].faction;
                
                if (faction == FACTION_REPUBLIC) {
                    wattron(win, COLOR_PAIR(COLOR_REPUBLIC));
                    mvwaddch(win, wy, wx, '0' + (cell % 10));
                    wattroff(win, COLOR_PAIR(COLOR_REPUBLIC));
                } else if (faction == FACTION_CIS) {
                    wattron(win, COLOR_PAIR(COLOR_CIS));
                    mvwaddch(win, wy, wx, '0' + (cell % 10));
                    wattroff(win, COLOR_PAIR(COLOR_CIS));
                }
            }
        }
    }
    
    // Redraw border and title
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " MAP %dx%d (1:1) ", M, N);
    mvwprintw(win, 0, win_w - 15, " Tick:%u ", tick);
    
    pthread_mutex_unlock(&ui_ctx->ui_lock);
}
```

---

### 3. ui_ust.c

**UST thread** - Unit Statistics Table display.

**Location**: `src/UI/ui_ust.c`

**Key Functions**:

#### `ui_ust_thread()`
Main thread loop for unit statistics.

**Algorithm**:
```
repeat:
    1. Lock semaphore (SEM_GLOBAL_LOCK)
    2. Copy unit array from shared memory
    3. Copy tick counter
    4. Unlock semaphore
    5. Render unit table to UST window
    6. Sleep 100ms
goto repeat
```

**Code**:
```c
void* ui_ust_thread(void* arg) {
    ui_context_t *ui_ctx = (ui_context_t*)arg;
    
    while (!ui_ctx->stop) {
        render_ust(ui_ctx);
        wrefresh(ui_ctx->ust_win);
        usleep(100000);  // 100ms
    }
    
    return NULL;
}
```

#### `render_ust()`
Draw unit statistics table.

**Table Format**:
```
 UNIT STATS                   Tick:1234 
ID Type       Faction   HP    Pos      PID
 1 Flagship   Republic  1000  (10,20)  12345
 2 Destroyer  CIS       800   (90,30)  12346
 3 Fighter    Republic  200   (15,15)  12347
...
```

**Display Fields**:
- **ID**: Unit ID (1-64)
- **Type**: Flagship, Destroyer, Carrier, Fighter, Bomber, Elite
- **Faction**: Republic (blue), CIS (red)
- **HP**: Current hit points
- **Pos**: (x, y) grid coordinates
- **PID**: Process ID

**Code**:
```c
static void render_ust(ui_context_t *ui_ctx) {
    pthread_mutex_lock(&ui_ctx->ui_lock);
    WINDOW *win = ui_ctx->ust_win;
    
    // Lock and copy data
    sem_lock(ui_ctx->ctx->sem_id, SEM_GLOBAL_LOCK);
    unit_entity_t units[MAX_UNITS+1];
    memcpy(units, ui_ctx->ctx->S->units, sizeof(units));
    uint32_t tick = ui_ctx->ctx->S->ticks;
    sem_unlock(ui_ctx->ctx->sem_id, SEM_GLOBAL_LOCK);
    
    // Clear window
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " UNIT STATS ");
    mvwprintw(win, 0, win_w - 15, " Tick:%u ", tick);
    
    // Table header
    int row = 1;
    wattron(win, A_BOLD);
    mvwprintw(win, row++, 1, "ID Type       Faction   HP    Pos      PID");
    wattroff(win, A_BOLD);
    
    // Display units
    for (int i = 1; i <= MAX_UNITS && row < win_h - 1; i++) {
        if (units[i].alive) {
            const char *type = get_type_name(units[i].type);
            const char *faction = get_faction_name(units[i].faction);
            
            // Color based on faction
            if (units[i].faction == FACTION_REPUBLIC) {
                wattron(win, COLOR_PAIR(COLOR_REPUBLIC));
            } else if (units[i].faction == FACTION_CIS) {
                wattron(win, COLOR_PAIR(COLOR_CIS));
            }
            
            mvwprintw(win, row++, 1, "%2d %-10s %-9s %-5d (%3d,%3d) %5d",
                      i, type, faction, units[i].hp,
                      units[i].position.x, units[i].position.y,
                      units[i].pid);
            
            if (units[i].faction != FACTION_NONE) {
                wattroff(win, COLOR_PAIR(units[i].faction));
            }
        }
    }
    
    pthread_mutex_unlock(&ui_ctx->ui_lock);
}
```

---

### 4. ui_std.c

**STD thread** - Standard output log display via FIFO.

**Location**: `src/UI/ui_std.c`

**Communication**: Reads from `/tmp/skirmish_std.fifo` (written by terminal_tee).

**Key Functions**:

#### `ui_std_thread()`
Main thread loop for output logging.

**Algorithm**:
```
1. Create FIFO at /tmp/skirmish_std.fifo
2. Open FIFO for reading (BLOCKING)
   - Blocks until terminal_tee opens for writing
3. repeat:
   a. Read buffer from FIFO
   b. Parse lines (split on '\n')
   c. Wrap long lines to fit window width
   d. Scroll window if full
   e. Display lines in STD window
   f. Refresh window
   goto repeat until EOF or stop
4. Close FIFO and remove file
```

**Code**:
```c
void *ui_std_thread(void *arg) {
    ui_context_t *ui_ctx = (ui_context_t *)arg;
    char buffer[4096];
    int fifo_fd = -1;
    
    // Create FIFO
    if (mkfifo("/tmp/skirmish_std.fifo", 0666) == -1 && errno != EEXIST) {
        // Non-critical error
        return NULL;
    }
    
    // Open FIFO (blocks until writer connects)
    fifo_fd = open("/tmp/skirmish_std.fifo", O_RDONLY);
    if (fifo_fd == -1) return NULL;
    
    ui_ctx->std_fifo_fd = fifo_fd;
    
    int line = 2;  // Start after title
    int max_y, max_x;
    
    while (!ui_ctx->stop) {
        ssize_t bytes = read(fifo_fd, buffer, sizeof(buffer) - 1);
        
        if (bytes <= 0) {
            if (bytes == 0) break;  // EOF
            if (errno == EINTR || errno == EAGAIN) continue;
            break;
        }
        
        buffer[bytes] = '\0';
        
        pthread_mutex_lock(&ui_ctx->ui_lock);
        
        getmaxyx(ui_ctx->std_win, max_y, max_x);
        
        // Parse and display lines
        char *line_start = buffer;
        char *newline;
        
        while ((newline = strchr(line_start, '\n')) != NULL) {
            *newline = '\0';
            
            // Wrap long lines
            int len = strlen(line_start);
            int max_line_width = max_x - 2;
            
            int offset = 0;
            while (offset < len) {
                // Scroll if needed
                if (line >= max_y - 1) {
                    wscrl(ui_ctx->std_win, 1);
                    line = max_y - 2;
                }
                
                // Display chunk
                wmove(ui_ctx->std_win, line, 1);
                wclrtoeol(ui_ctx->std_win);
                
                int chunk_len = (len - offset > max_line_width) ? 
                                max_line_width : (len - offset);
                waddnstr(ui_ctx->std_win, line_start + offset, chunk_len);
                
                offset += chunk_len;
                line++;
            }
            
            line_start = newline + 1;
        }
        
        wrefresh(ui_ctx->std_win);
        pthread_mutex_unlock(&ui_ctx->ui_lock);
    }
    
    close(fifo_fd);
    unlink("/tmp/skirmish_std.fifo");
    
    return NULL;
}
```

**Line Wrapping**:
- Splits lines longer than window width
- Clears each line with `wclrtoeol()` before writing
- Scrolls window when bottom is reached
- Preserves formatting and indentation where possible

---

## Window Layout

### Adaptive Sizing

**Calculation**:
```c
int max_y, max_x;
getmaxyx(stdscr, max_y, max_x);

// MAP: Grid size + borders
int map_width = M + 2;    // 122
int map_height = N + 2;   // 42

// Adjust if terminal too small
if (map_width > max_x) map_width = max_x;
if (map_height > max_y - 5) map_height = max_y - 5;

// STD: Bottom portion (minimum 5 lines)
int bottom_height = max_y - map_height;
if (bottom_height < 5) {
    bottom_height = 5;
    map_height = max_y - bottom_height;
}

// UST: Remaining width
int ust_width = max_x - map_width;

// Create windows
map_win = newwin(map_height, map_width, 0, 0);
ust_win = newwin(map_height, ust_width, 0, map_width);
std_win = newwin(bottom_height, max_x, map_height, 0);
```

**Minimum Terminal Size**: ~122 columns × 47 rows for full display.

**Small Terminal Handling**:
- Grid clips to available space
- UST scrolls if many units
- STD always visible (minimum 5 lines)

---

## Threading Model

### Thread Overview

| Thread | Purpose | Update Rate | Blocking |
|--------|---------|-------------|----------|
| Main | Input handling, coordination | 50ms | No |
| MAP | Grid visualization | Request-response | Yes (on msgrcv) |
| UST | Unit statistics | 100ms | No |
| STD | Output logging | Event-driven | Yes (on read) |

### Thread Safety

**Mutex Protection**:
```c
pthread_mutex_t ui_lock;  // Protects all ncurses window operations
```

**Critical Sections**:
- All `mvwprintw()`, `waddch()`, `wrefresh()` calls
- Window property queries (`getmaxyx()`)
- Window creation/destruction

**Pattern**:
```c
pthread_mutex_lock(&ui_ctx->ui_lock);

// ncurses operations
mvwprintw(win, y, x, "text");
wrefresh(win);

pthread_mutex_unlock(&ui_ctx->ui_lock);
```

**Why Needed**: ncurses is NOT thread-safe; concurrent window operations cause corruption.

---

## Rendering System

### Color Scheme

**Color Pairs** (initialized in `ui_init()`):
```c
init_pair(1, COLOR_BLUE, COLOR_BLACK);    // Republic
init_pair(2, COLOR_RED, COLOR_BLACK);     // CIS
init_pair(3, COLOR_GREEN, COLOR_BLACK);   // Neutral
init_pair(4, COLOR_YELLOW, COLOR_BLACK);  // Highlights
```

**Usage**:
```c
// Apply color
wattron(win, COLOR_PAIR(COLOR_REPUBLIC));
mvwaddch(win, y, x, 'U');
wattroff(win, COLOR_PAIR(COLOR_REPUBLIC));
```

---

### Refresh Strategy

**Partial Refresh**: Only modified windows are refreshed.

**Refresh Calls**:
- **MAP Thread**: After each grid render
- **UST Thread**: After each stats render
- **STD Thread**: After each FIFO read
- **Main Thread**: Periodic full refresh (50ms)

**Optimization**: Use `wnoutrefresh()` + `doupdate()` for batched refresh:
```c
wnoutrefresh(map_win);
wnoutrefresh(ust_win);
wnoutrefresh(std_win);
doupdate();  // Single physical refresh
```

---

## Data Flow

### MAP Thread Data Flow

```
Command Center              UI MAP Thread
      │                          │
      │                          │
      │     ┌─────────────────┐  │
      │     │ 1. Send request │  │
      │     │ MSG_UI_MAP_REQ  │  │
      │     └────────┬────────┘  │
      │              │            │
      ◄──────────────┘            │
 ┌────▼─────┐                     │
 │ 2. CC    │                     │
 │ receives │                     │
 │ request  │                     │
 └────┬─────┘                     │
      │                           │
 ┌────▼─────┐                     │
 │ 3. Send  │                     │
 │ MSG_UI_  │                     │
 │ MAP_REP  │                     │
 └────┬─────┘                     │
      │                           │
      └──────────────┐            │
                     │            │
                ┌────▼─────────┐  │
                │ 4. Receive   │  │
                │ response     │  │
                │ (blocking)   │  │
                └────┬─────────┘  │
                     │            │
                ┌────▼─────────┐  │
                │ 5. Lock SHM  │  │
                │ copy grid    │  │
                │ unlock       │  │
                └────┬─────────┘  │
                     │            │
                ┌────▼─────────┐  │
                │ 6. Render    │  │
                │ to MAP win   │  │
                └────┬─────────┘  │
                     │            │
                ┌────▼─────────┐  │
                │ 7. Refresh   │  │
                └──────────────┘  │
```

---

### UST Thread Data Flow

```
Shared Memory              UI UST Thread
      │                          │
      │     ┌─────────────────┐  │
      │     │ 1. Lock SEM_    │  │
      │     │ GLOBAL_LOCK     │  │
      │     └────────┬────────┘  │
      │              │            │
      ◄──────────────┘            │
 ┌────▼─────┐                     │
 │ 2. Copy  │                     │
 │ unit[]   │                     │
 │ array    │                     │
 └────┬─────┘                     │
      │                           │
      └──────────────┐            │
                ┌────▼─────────┐  │
                │ 3. Unlock    │  │
                │ semaphore    │  │
                └────┬─────────┘  │
                     │            │
                ┌────▼─────────┐  │
                │ 4. Render    │  │
                │ unit table   │  │
                └────┬─────────┘  │
                     │            │
                ┌────▼─────────┐  │
                │ 5. Refresh   │  │
                │ UST window   │  │
                └──────────────┘  │
```

---

### STD Thread Data Flow

```
Command Center          terminal_tee           UI STD Thread
      │                     │                        │
      │ printf/fprintf      │                        │
      ├────────────────────►│                        │
      │                     │                        │
      │                ┌────▼─────┐                  │
      │                │ Tee to   │                  │
      │                │ log AND  │                  │
      │                │ FIFO     │                  │
      │                └────┬─────┘                  │
      │                     │                        │
      │                     │ write() to FIFO        │
      │                     ├───────────────────────►│
      │                     │                        │
      │                     │                   ┌────▼─────┐
      │                     │                   │ read()   │
      │                     │                   │ from     │
      │                     │                   │ FIFO     │
      │                     │                   └────┬─────┘
      │                     │                        │
      │                     │                   ┌────▼─────┐
      │                     │                   │ Parse    │
      │                     │                   │ lines    │
      │                     │                   └────┬─────┘
      │                     │                        │
      │                     │                   ┌────▼─────┐
      │                     │                   │ Wrap &   │
      │                     │                   │ display  │
      │                     │                   └────┬─────┘
      │                     │                        │
      │                     │                   ┌────▼─────┐
      │                     │                   │ Refresh  │
      │                     │                   │ STD win  │
      │                     │                   └──────────┘
```

---

## API Reference

### Initialization & Cleanup

```c
int ui_init(ui_context_t *ui_ctx, const char *run_dir);
```
- **Purpose**: Initialize ncurses, create windows, setup colors
- **Parameters**: 
  - `ui_ctx`: UI context to initialize
  - `run_dir`: Run directory path (optional, can be NULL)
- **Returns**: 0 on success, -1 on error
- **Side Effects**: Initializes ncurses, creates windows

```c
void ui_cleanup(ui_context_t *ui_ctx);
```
- **Purpose**: Cleanup ncurses and release resources
- **Parameters**: `ui_ctx`: UI context to cleanup
- **Side Effects**: Ends ncurses, destroys windows, removes FIFO

```c
void ui_refresh_all(ui_context_t *ui_ctx);
```
- **Purpose**: Thread-safe refresh of all windows
- **Thread Safe**: Yes (uses mutex)

---

### Thread Entry Points

```c
void* ui_map_thread(void* arg);
```
- **Purpose**: MAP rendering thread
- **Parameters**: `arg`: Pointer to `ui_context_t`
- **Returns**: NULL on exit
- **Behavior**: Request-response loop with Command Center

```c
void* ui_ust_thread(void* arg);
```
- **Purpose**: UST rendering thread
- **Parameters**: `arg`: Pointer to `ui_context_t`
- **Returns**: NULL on exit
- **Behavior**: Periodic polling (100ms) of shared memory

```c
void* ui_std_thread(void* arg);
```
- **Purpose**: STD output logging thread
- **Parameters**: `arg`: Pointer to `ui_context_t`
- **Returns**: NULL on exit
- **Behavior**: Blocking read from FIFO

---

### Data Structures

#### `ui_context_t`

**Purpose**: UI runtime context shared by all threads.

```c
typedef struct {
    /* ncurses windows */
    WINDOW *map_win;    // Top-left: grid display
    WINDOW *ust_win;    // Top-right: unit stats
    WINDOW *std_win;    // Bottom: output log
    
    /* IPC context */
    ipc_ctx_t *ctx;     // Shared memory access
    
    /* Communication */
    char run_dir[512];  // Run directory path
    int std_fifo_fd;    // FIFO file descriptor
    int cm_in_fd;       // CM input (unused)
    int cm_out_fd;      // CM output (unused)
    
    /* Thread control */
    pthread_mutex_t ui_lock;        // ncurses mutex
    volatile sig_atomic_t stop;     // Stop flag
    
    /* Thread IDs */
    pthread_t map_thread_id;
    pthread_t ust_thread_id;
    pthread_t std_thread_id;
} ui_context_t;
```

---

## Usage Examples

### Example 1: Starting UI

```bash
# Start UI (automatic IPC detection)
./ui

# Start UI with explicit ftok path
./ui --ftok ./ipc.key

# Start UI with run directory (for logs)
./ui --run-dir ./logs/run_2026-02-05_12-00-00_pid12345
```

---

### Example 2: Custom Window Creation

```c
#include <ncurses.h>
#include "UI/ui.h"

int main() {
    ui_context_t ui_ctx;
    
    // Initialize UI
    if (ui_init(&ui_ctx, NULL) == -1) {
        fprintf(stderr, "UI initialization failed\n");
        return 1;
    }
    
    // Get dimensions
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    printf("Terminal size: %dx%d\n", max_x, max_y);
    printf("MAP window: %p\n", ui_ctx.map_win);
    printf("UST window: %p\n", ui_ctx.ust_win);
    printf("STD window: %p\n", ui_ctx.std_win);
    
    // Wait for user
    getch();
    
    // Cleanup
    ui_cleanup(&ui_ctx);
    
    return 0;
}
```

---

### Example 3: Manual Grid Rendering

```c
#include "UI/ui.h"
#include "ipc/ipc_context.h"

void render_test_pattern(ui_context_t *ui_ctx) {
    pthread_mutex_lock(&ui_ctx->ui_lock);
    
    WINDOW *win = ui_ctx->map_win;
    int win_h, win_w;
    getmaxyx(win, win_h, win_w);
    
    // Draw checkerboard pattern
    for (int y = 1; y < win_h - 1; y++) {
        for (int x = 1; x < win_w - 1; x++) {
            char ch = ((x + y) % 2 == 0) ? '#' : '.';
            mvwaddch(win, y, x, ch);
        }
    }
    
    // Refresh
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " TEST PATTERN ");
    wrefresh(win);
    
    pthread_mutex_unlock(&ui_ctx->ui_lock);
}
```

---

### Example 4: Color Usage

```c
#include <ncurses.h>

void demo_colors(WINDOW *win) {
    if (has_colors()) {
        start_color();
        
        // Define color pairs
        init_pair(1, COLOR_BLUE, COLOR_BLACK);
        init_pair(2, COLOR_RED, COLOR_BLACK);
        
        // Use colors
        wattron(win, COLOR_PAIR(1));
        mvwprintw(win, 1, 1, "Blue text (Republic)");
        wattroff(win, COLOR_PAIR(1));
        
        wattron(win, COLOR_PAIR(2));
        mvwprintw(win, 2, 1, "Red text (CIS)");
        wattroff(win, COLOR_PAIR(2));
        
        wrefresh(win);
    }
}
```

---

## File Organization

```
src/UI/
├── ui_main.c             # Main UI process
├── ui_map.c              # MAP rendering thread
├── ui_ust.c              # UST rendering thread
└── ui_std.c              # STD output thread

include/UI/
├── ui.h                  # UI context structure
├── ui_map.h              # MAP thread interface
├── ui_ust.h              # UST thread interface
└── ui_std.h              # STD thread interface
```

---

## Build Integration

From project `Makefile`:

```makefile
# UI object files
UI_OBJS = ui_main.o ui_map.o ui_ust.o ui_std.o

# Build UI binary
ui: $(UI_OBJS) $(IPC_OBJS) error_handler.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ -lncurses -lpthread

# Compile UI components
ui_main.o: src/UI/ui_main.c include/UI/ui.h
	$(CC) $(CFLAGS) -c $< -o $@

ui_map.o: src/UI/ui_map.c include/UI/ui_map.h
	$(CC) $(CFLAGS) -c $< -o $@

ui_ust.o: src/UI/ui_ust.c include/UI/ui_ust.h
	$(CC) $(CFLAGS) -c $< -o $@

ui_std.o: src/UI/ui_std.c include/UI/ui_std.h
	$(CC) $(CFLAGS) -c $< -o $@
```

**Dependencies**:
- `libncurses`: Terminal UI library
- `libpthread`: POSIX threads

---

## Error Handling

### Common Issues

#### Window NULL Pointers
**Symptom**: Segmentation fault in rendering threads

**Cause**: Window not initialized or already destroyed

**Solution**:
```c
if (!ui_ctx->map_win) {
    pthread_mutex_unlock(&ui_ctx->ui_lock);
    return;
}
```

---

#### FIFO Connection Failures
**Symptom**: STD thread shows "Waiting for command_center..."

**Cause**: terminal_tee hasn't opened FIFO for writing yet

**Solution**: STD thread blocks on `open()` until writer connects (expected behavior)

---

#### ncurses Corruption
**Symptom**: Garbled display, overlapping text

**Cause**: Missing mutex protection on ncurses calls

**Solution**: Always use `pthread_mutex_lock(&ui_ctx->ui_lock)` around ncurses operations

---

#### Terminal Too Small
**Symptom**: Clipped display, missing windows

**Cause**: Terminal smaller than minimum required size

**Solution**: 
- Resize terminal to at least 122×47
- UI adapts by clipping grid and scrolling

---

## Performance Optimization

### Reducing CPU Usage

**Current Rates**:
- Main thread: 50ms refresh (20 Hz)
- MAP thread: Request-response (10-50 Hz depending on CC)
- UST thread: 100ms refresh (10 Hz)
- STD thread: Event-driven (no polling)

**Optimization**:
```c
// Reduce main thread rate
usleep(100000);  // 100ms instead of 50ms

// Use wnoutrefresh + doupdate
wnoutrefresh(map_win);
wnoutrefresh(ust_win);
wnoutrefresh(std_win);
doupdate();  // Single physical update
```

---

### Minimizing IPC Contention

**Strategy**: Copy data quickly while holding lock.

```c
// ✅ Good: Short critical section
sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
memcpy(local_grid, ctx->S->grid, sizeof(local_grid));
sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);

render_grid(local_grid);  // Process without lock

// ❌ Bad: Long critical section
sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
render_grid(ctx->S->grid);  // Holds lock during rendering!
sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
```

---

## Debugging

### Enable ncurses Debug Mode

```c
// Before initscr()
trace(TRACE_CALLS);  // Log all ncurses calls
initscr();
```

### Monitor Thread Activity

```bash
# Watch UI process threads
watch -n 1 'ps -L -p $(pgrep ui) -o pid,lwp,comm'

# Check FIFO
ls -la /tmp/skirmish_std.fifo

# Test FIFO manually
echo "Test message" > /tmp/skirmish_std.fifo
```

### Common Debug Patterns

```c
// Log window dimensions
LOGI("[UI-MAP] Window: %dx%d, Grid: %dx%d", win_w, win_h, M, N);

// Log grid contents
int non_empty = 0;
for (int y = 0; y < N; y++) {
    for (int x = 0; x < M; x++) {
        if (grid[x][y] != 0) non_empty++;
    }
}
LOGI("[UI-MAP] Non-empty cells: %d", non_empty);

// Verify color support
if (has_colors()) {
    LOGI("[UI] Terminal supports %d colors", COLORS);
} else {
    LOGW("[UI] Terminal does not support colors");
}
```

---

## Best Practices

1. **Always Lock ncurses Operations**
   ```c
   pthread_mutex_lock(&ui_ctx->ui_lock);
   // ... ncurses calls ...
   pthread_mutex_unlock(&ui_ctx->ui_lock);
   ```

2. **Check Window Pointers**
   ```c
   if (!win) return;  // Exit early if window destroyed
   ```

3. **Use Non-Blocking Input**
   ```c
   nodelay(stdscr, TRUE);  // getch() doesn't block
   int ch = getch();
   if (ch != ERR) {
       // Handle input
   }
   ```

4. **Cleanup on Exit**
   ```c
   // Join threads before cleanup
   pthread_join(map_thread_id, NULL);
   pthread_join(ust_thread_id, NULL);
   pthread_join(std_thread_id, NULL);
   
   // Then cleanup UI
   ui_cleanup(&ui_ctx);
   ```

5. **Handle Terminal Resize**
   ```c
   signal(SIGWINCH, handle_resize);
   
   void handle_resize(int sig) {
       endwin();
       refresh();
       // Recreate windows with new dimensions
   }
   ```

---

## See Also

- [CC Module Documentation](CC_MODULE.md) - Command Center integration
- [IPC Module Documentation](IPC_MODULE.md) - IPC message protocol
- [Project README](../README.md) - Build and run instructions
- `man 3 ncurses` - ncurses library documentation
- `man 7 fifo` - FIFO (named pipe) usage

---

## References

- **ncurses Programming HOWTO**: https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/
- **POSIX Threads**: `man 7 pthreads`
- **FIFO**: `man 7 fifo`, `man 3 mkfifo`
- **Color in ncurses**: `man 3 init_pair`, `man 3 color_pair`
