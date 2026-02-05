// UI main process
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ncurses.h>
#include <pthread.h>

#include "UI/ui.h"
#include "UI/ui_map.h"
#include "UI/ui_ust.h"
#include "ipc/ipc_context.h"
#include "log.h"
#include "error_handler.h"

static ui_context_t g_ui_ctx = {0};

static void signal_handler(int sig) {
    (void)sig;
    g_ui_ctx.stop = 1;
    LOGI("[UI] Signal %d received, setting stop flag", sig);
}

int ui_init(ui_context_t *ui_ctx, const char *run_dir) {
    memset(ui_ctx, 0, sizeof(*ui_ctx));
    
    if (run_dir) {
        strncpy(ui_ctx->run_dir, run_dir, sizeof(ui_ctx->run_dir) - 1);
    }
    
    ui_ctx->std_fifo_fd = -1;
    ui_ctx->cm_in_fd = -1;
    ui_ctx->cm_out_fd = -1;
    
    if (CHECK_SYS_CALL_NONFATAL(pthread_mutex_init(&ui_ctx->ui_lock, NULL), "ui_init:pthread_mutex_init") != 0) {
        return -1;
    }
    
    /* Initialize ncurses */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);  // Non-blocking getch()
    curs_set(0);  // Hide cursor
    
    /* Enable colors if available */
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_BLUE, COLOR_BLACK);    // Faction 1 (Republic)
        init_pair(2, COLOR_RED, COLOR_BLACK);     // Faction 2 (CIS)
        init_pair(3, COLOR_GREEN, COLOR_BLACK);   // Neutral
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);  // Highlights
    }
    
    /* Get screen dimensions */
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    /* Calculate MAP window to fit grid (M x N) + borders */
    int map_width = M + 2;   // Grid width + 2 for borders
    int map_height = N + 2;  // Grid height + 2 for borders
    
    /* Adjust if screen is too small */
    if (map_width > max_x) map_width = max_x;
    if (map_height > max_y - 5) map_height = max_y - 5;  // Leave room for STD
    
    /* Calculate bottom portion for STD (at least 5 lines) */
    int bottom_height = max_y - map_height;
    if (bottom_height < 5) {
        bottom_height = 5;
        map_height = max_y - bottom_height;
    }
    
    /* UST takes remaining width to the right of MAP */
    int ust_width = max_x - map_width;
    
    /* Create windows */
    ui_ctx->map_win = newwin(map_height, map_width, 0, 0);
    ui_ctx->ust_win = newwin(map_height, ust_width, 0, map_width);
    ui_ctx->std_win = newwin(bottom_height, max_x, map_height, 0);
    
    /* Enable scrolling for STD */
    scrollok(ui_ctx->std_win, TRUE);
    
    /* Draw borders and titles */
    box(ui_ctx->map_win, 0, 0);
    box(ui_ctx->ust_win, 0, 0);
    box(ui_ctx->std_win, 0, 0);
    
    mvwprintw(ui_ctx->map_win, 0, 2, " MAP ");
    mvwprintw(ui_ctx->ust_win, 0, 2, " UNIT STATS ");
    mvwprintw(ui_ctx->std_win, 0, 2, " OUTPUT ");
    
    wrefresh(ui_ctx->map_win);
    wrefresh(ui_ctx->ust_win);
    wrefresh(ui_ctx->std_win);
    
    return 0;
}

void ui_cleanup(ui_context_t *ui_ctx) {
    if (ui_ctx->map_win) delwin(ui_ctx->map_win);
    if (ui_ctx->ust_win) delwin(ui_ctx->ust_win);
    if (ui_ctx->std_win) delwin(ui_ctx->std_win);
    
    endwin();
    
    if (ui_ctx->std_fifo_fd != -1) close(ui_ctx->std_fifo_fd);
    if (ui_ctx->cm_in_fd != -1) close(ui_ctx->cm_in_fd);
    if (ui_ctx->cm_out_fd != -1) close(ui_ctx->cm_out_fd);
    
    /* Remove FIFO - this signals tee to return to normal terminal output */
    unlink("/tmp/skirmish_std.fifo");
    
    pthread_mutex_destroy(&ui_ctx->ui_lock);
}

void ui_refresh_all(ui_context_t *ui_ctx) {
    pthread_mutex_lock(&ui_ctx->ui_lock);
    
    wrefresh(ui_ctx->map_win);
    wrefresh(ui_ctx->ust_win);
    wrefresh(ui_ctx->std_win);
    
    pthread_mutex_unlock(&ui_ctx->ui_lock);
}

int main(int argc, char **argv) {
    const char *ftok_path = "./ipc.key";
    char run_dir[512] = {0};
    
    /* Initialize logging */
    log_init("UI", 0);
    LOGI("[UI] Starting UI process...");
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--ftok") && i + 1 < argc) {
            ftok_path = argv[++i];
        } else if (!strcmp(argv[i], "--run-dir") && i + 1 < argc) {
            strncpy(run_dir, argv[++i], sizeof(run_dir) - 1);
        }
    }
    
    /* If no run-dir provided, try to get from environment (optional) */
    if (run_dir[0] == '\0') {
        const char *env_run_dir = getenv("SKIRMISH_RUN_DIR");
        if (env_run_dir) {
            strncpy(run_dir, env_run_dir, sizeof(run_dir) - 1);
        }
        /* If still no run_dir, that's OK - UI will work without it */
    }
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Attach to IPC */
    ipc_ctx_t ctx;
    if (CHECK_SYS_CALL_NONFATAL(ipc_attach(&ctx, ftok_path), "ui_main:ipc_attach") == -1) {
        fprintf(stderr, "[UI] Failed to attach to IPC. Is command_center running?\n");
        LOGE("[UI] Failed to attach to IPC");
        return 1;
    }
    LOGI("[UI] Successfully attached to IPC");
    
    /* Initialize UI */
    if (ui_init(&g_ui_ctx, run_dir[0] ? run_dir : NULL) == -1) {
        fprintf(stderr, "[UI] Failed to initialize UI\n");
        LOGE("[UI] Failed to initialize ncurses");
        ipc_detach(&ctx);
        return 1;
    }
    LOGI("[UI] ncurses initialized successfully");
    
    g_ui_ctx.ctx = &ctx;
    
    /* Start MAP thread */
    if (pthread_create(&g_ui_ctx.map_thread_id, NULL, ui_map_thread, &g_ui_ctx) != 0) {
        HANDLE_SYS_ERROR_NONFATAL("ui_main:pthread_create_MAP", "Failed to create MAP thread");
        ui_cleanup(&g_ui_ctx);
        ipc_detach(&ctx);
        return 1;
    }
    
    /* Start UST thread */
    if (pthread_create(&g_ui_ctx.ust_thread_id, NULL, ui_ust_thread, &g_ui_ctx) != 0) {
        HANDLE_SYS_ERROR_NONFATAL("ui_main:pthread_create_UST", "Failed to create UST thread");
        g_ui_ctx.stop = 1;
        pthread_join(g_ui_ctx.map_thread_id, NULL);
        ui_cleanup(&g_ui_ctx);
        ipc_detach(&ctx);
        return 1;
    }
    
    /* Start STD thread */
    if (pthread_create(&g_ui_ctx.std_thread_id, NULL, ui_std_thread, &g_ui_ctx) != 0) {
        HANDLE_SYS_ERROR_NONFATAL("ui_main:pthread_create_STD", "Failed to create STD thread");
        g_ui_ctx.stop = 1;
        pthread_join(g_ui_ctx.map_thread_id, NULL);
        pthread_join(g_ui_ctx.ust_thread_id, NULL);
        ui_cleanup(&g_ui_ctx);
        ipc_detach(&ctx);
        return 1;
    }
    
    /* Main loop - handle input and refresh */
    while (!g_ui_ctx.stop) {
        int ch = getch();
        
        if (ch == 'q' || ch == 'Q') {
            g_ui_ctx.stop = 1;
            LOGI("[UI] User requested quit");
            break;
        }
        
        /* Refresh all windows */
        ui_refresh_all(&g_ui_ctx);

        //usleep(50000);  // 50ms refresh rate
    }
    
    LOGI("[UI] Main loop exited, waiting for threads...");
    
    /* Close FIFO immediately to unblock STD thread if it's waiting */
    if (g_ui_ctx.std_fifo_fd != -1) {
        LOGI("[UI] Closing STD FIFO fd=%d", g_ui_ctx.std_fifo_fd);
        close(g_ui_ctx.std_fifo_fd);
        g_ui_ctx.std_fifo_fd = -1;
    }
    unlink("/tmp/skirmish_std.fifo");
    
    /* Wait for threads to finish */
    LOGI("[UI] Joining MAP thread...");
    pthread_join(g_ui_ctx.map_thread_id, NULL);
    LOGI("[UI] Joining UST thread...");
    pthread_join(g_ui_ctx.ust_thread_id, NULL);
    LOGI("[UI] Joining STD thread...");
    pthread_join(g_ui_ctx.std_thread_id, NULL);
    LOGI("[UI] All threads joined");
    
    /* Cleanup */
    LOGI("[UI] Shutting down...");
    ui_cleanup(&g_ui_ctx);
    CHECK_SYS_CALL_NONFATAL(ipc_detach(&ctx), "ui_main:ipc_detach");
    LOGI("[UI] Shutdown complete");
    log_close();
    
    return 0;
}
