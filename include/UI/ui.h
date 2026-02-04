#pragma once

#include <signal.h>
#include <ncurses.h>
#include <pthread.h>
#include "ipc/ipc_context.h"

/* UI window layout:
 * +----------------+----------------+
 * |   MAP (TL)     |   UST (TR)     |
 * |                |                |
 * +----------------+----------------+
 * |        STD (Bottom - Full)      |
 * +------------------------------------+
 */

typedef struct {
    /* ncurses windows */
    WINDOW *map_win;    // Top-left: grid map display
    WINDOW *ust_win;    // Top-right: unit stats table
    WINDOW *std_win;    // Bottom: standard output log (full width)
    
    /* IPC context */
    ipc_ctx_t *ctx;
    
    /* Communication FIFOs */
    char run_dir[512];
    int std_fifo_fd;    // Read from tee
    int cm_in_fd;       // Write to CM
    int cm_out_fd;      // Read from CM
    
    /* Thread control */
    pthread_mutex_t ui_lock;
    volatile sig_atomic_t stop;
    
    /* Thread IDs */
    pthread_t map_thread_id;
    pthread_t ust_thread_id;
    pthread_t ucm_thread_id;
    pthread_t std_thread_id;
} ui_context_t;

/* Initialize UI context and ncurses */
int ui_init(ui_context_t *ui_ctx, const char *run_dir);

/* Cleanup and shutdown UI */
void ui_cleanup(ui_context_t *ui_ctx);

/* Refresh all windows */
void ui_refresh_all(ui_context_t *ui_ctx);

/* Thread entry points */
void *ui_std_thread(void *arg);


/* Thread entry points */
void *ui_std_thread(void *arg);
