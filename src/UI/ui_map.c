// UI MAP thread - displays grid
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/msg.h>
#include <errno.h>
#include <ncurses.h>

#include "UI/ui.h"
#include "UI/ui_map.h"
#include "ipc/shared.h"
#include "ipc/ipc_mesq.h"
#include "ipc/semaphores.h"
#include "log.h"

/* Color pairs for factions */
#define COLOR_REPUBLIC  1
#define COLOR_CIS       2
#define COLOR_NEUTRAL   3

/* Render the map grid */
static void render_map(ui_context_t *ui_ctx, unit_id_t grid[M][N], uint32_t tick) {
    pthread_mutex_lock(&ui_ctx->ui_lock);
    
    WINDOW *win = ui_ctx->map_win;
    int win_h, win_w;
    getmaxyx(win, win_h, win_w);
    
    /* Clear content area (preserve border) */
    for (int y = 1; y < win_h - 1; y++) {
        wmove(win, y, 1);
        for (int x = 1; x < win_w - 1; x++) {
            waddch(win, ' ');
        }
    }
    
    /* Calculate available display area (excluding borders) */
    int content_h = win_h - 2;
    int content_w = win_w - 2;
    
    /* Debug: log actual window and content dimensions */
    static int logged = 0;
    if (!logged) {
        LOGI("[UI-MAP] Window size: %dx%d, Content size: %dx%d, Grid: %dx%d", 
             win_w, win_h, content_w, content_h, M, N);
        
        /* Count non-empty cells to verify we're reading the grid correctly */
        int non_empty = 0;
        for (int y = 0; y < N; y++) {
            for (int x = 0; x < M; x++) {
                if (grid[x][y] != 0) non_empty++;
            }
        }
        LOGI("[UI-MAP] Non-empty cells in grid: %d", non_empty);
        logged = 1;
    }
    
    /* Draw grid at 1:1 scale (one cell = one character) */
    int cells_drawn = 0;
    for (int gy = 0; gy < N && gy < content_h; gy++) {
        for (int gx = 0; gx < M && gx < content_w; gx++) {
            unit_id_t cell = grid[gx][gy];
            
            int wy = 1 + gy;
            int wx = 1 + gx;
            
            cells_drawn++;
            
            /* Draw cell */
            if (cell == 0) {
                mvwaddch(win, wy, wx, '.');
            } else {
                /* Get unit info from shared memory */
                uint8_t faction = ui_ctx->ctx->S->units[cell].faction;
                
                /* Apply color based on faction */
                if (faction == FACTION_REPUBLIC) {
                    wattron(win, COLOR_PAIR(COLOR_REPUBLIC));
                    mvwaddch(win, wy, wx, '0' + (cell % 10));
                    wattroff(win, COLOR_PAIR(COLOR_REPUBLIC));
                } else if (faction == FACTION_CIS) {
                    wattron(win, COLOR_PAIR(COLOR_CIS));
                    mvwaddch(win, wy, wx, '0' + (cell % 10));
                    wattroff(win, COLOR_PAIR(COLOR_CIS));
                } else {
                    mvwaddch(win, wy, wx, '0' + (cell % 10));
                }
            }
        }
    }
    
    /* Debug: log draw statistics */
    if (!logged) {
        LOGI("[UI-MAP] Drew %d cells (expected %d x %d = %d)", 
             cells_drawn, 
             (M < content_w ? M : content_w),
             (N < content_h ? N : content_h),
             (M < content_w ? M : content_w) * (N < content_h ? N : content_h));
    }
    
    /* Note if map is clipped */
    int clipped_x = (M > content_w) ? 1 : 0;
    int clipped_y = (N > content_h) ? 1 : 0;
    
    /* Redraw border and title */
    box(win, 0, 0);
    if (clipped_x || clipped_y) {
        mvwprintw(win, 0, 2, " MAP %dx%d (showing %dx%d) ", 
                  M, N, 
                  (M > content_w) ? content_w : M, 
                  (N > content_h) ? content_h : N);
    } else {
        mvwprintw(win, 0, 2, " MAP %dx%d (1:1) ", M, N);
    }
    
    /* Show tick in header */
    mvwprintw(win, 0, win_w - 15, " Tick:%u ", tick);
    
    pthread_mutex_unlock(&ui_ctx->ui_lock);
}

void* ui_map_thread(void* arg) {
    ui_context_t *ui_ctx = (ui_context_t*)arg;
    uint32_t last_tick = 0;
    
    LOGI("[UI-MAP] Thread started, sending requests to CC");
    
    /* Request-response loop */
    while (!ui_ctx->stop) {
        /* Send request for map snapshot */
        mq_ui_map_req_t req;
        req.mtype = MSG_UI_MAP_REQ;
        req.sender = getpid();
        
        if (mq_send_ui_map_req(ui_ctx->ctx->q_req, &req) == 0) {
            /* Wait for response (blocking with timeout) */
            mq_ui_map_rep_t rep;
            int ret = mq_recv_ui_map_rep_blocking(ui_ctx->ctx->q_rep, &rep);
            
            if (ret > 0 && rep.ready) {
                /* Got notification, read grid from shared memory */
                if (sem_lock(ui_ctx->ctx->sem_id, SEM_GLOBAL_LOCK) == 0) {
                    /* Copy grid from shared memory */
                    unit_id_t grid_snapshot[M][N];
                    memcpy(grid_snapshot, ui_ctx->ctx->S->grid, sizeof(grid_snapshot));
                    uint32_t tick = ui_ctx->ctx->S->ticks;
                    sem_unlock(ui_ctx->ctx->sem_id, SEM_GLOBAL_LOCK);
                    
                    /* Update display */
                    last_tick = tick;
                    render_map(ui_ctx, grid_snapshot, last_tick);
                }
            } else if (!ui_ctx->stop) {
                LOGW("[UI-MAP] Failed to receive response");
            }
        } else if (!ui_ctx->stop) {
            LOGW("[UI-MAP] Failed to send request");
        }
        
        /* Wait before next request */
        usleep(200000);  // 200ms = 5 updates/sec
    }
    
    LOGI("[UI-MAP] Thread exiting");
    return NULL;
}
