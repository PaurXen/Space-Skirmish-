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

static ui_context_t g_ui_ctx = {0};

static void signal_handler(int sig) {
    (void)sig;
    g_ui_ctx.stop = 1;
}

int ui_init(ui_context_t *ui_ctx, const char *run_dir) {
    memset(ui_ctx, 0, sizeof(*ui_ctx));
    
    if (run_dir) {
        strncpy(ui_ctx->run_dir, run_dir, sizeof(ui_ctx->run_dir) - 1);
    }
    
    ui_ctx->std_fifo_fd = -1;
    ui_ctx->cm_in_fd = -1;
    ui_ctx->cm_out_fd = -1;
    
    pthread_mutex_init(&ui_ctx->ui_lock, NULL);
    
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
    
    /* Calculate MAP window size to fit the grid (M x N) + borders */
    int map_height = (N + 2 < max_y) ? N + 2 : max_y;  // +2 for borders
    int map_width = (M + 2 < max_x) ? M + 2 : max_x;
    
    /* Create windows - MAP takes what it needs, others split remaining space */
    ui_ctx->map_win = newwin(map_height, map_width, 0, 0);
    
    /* Remaining space for other windows */
    int remaining_w = max_x - map_width;
    int remaining_h = max_y;
    
    if (remaining_w > 0) {
        /* UST on the right of MAP */
        ui_ctx->ust_win = newwin(map_height, remaining_w, 0, map_width);
        
        /* UCM and STD below MAP if there's room */
        if (max_y > map_height) {
            int bottom_h = max_y - map_height;
            int half_w = max_x / 2;
            ui_ctx->ucm_win = newwin(bottom_h, half_w, map_height, 0);
            ui_ctx->std_win = newwin(bottom_h, max_x - half_w, map_height, half_w);
        } else {
            /* Not enough vertical space, put them in remaining horizontal space */
            int half_h = remaining_h / 2;
            ui_ctx->ucm_win = newwin(half_h, remaining_w, 0, map_width + remaining_w);
            ui_ctx->std_win = newwin(remaining_h - half_h, remaining_w, half_h, map_width + remaining_w);
        }
    } else {
        /* MAP takes full width, stack others vertically below */
        int remaining_y = max_y - map_height;
        if (remaining_y > 0) {
            int third = remaining_y / 3;
            ui_ctx->ust_win = newwin(third, max_x, map_height, 0);
            ui_ctx->ucm_win = newwin(third, max_x, map_height + third, 0);
            ui_ctx->std_win = newwin(remaining_y - 2*third, max_x, map_height + 2*third, 0);
        } else {
            /* Not enough room, create minimal windows */
            ui_ctx->ust_win = newwin(1, max_x, 0, 0);
            ui_ctx->ucm_win = newwin(1, max_x, 0, 0);
            ui_ctx->std_win = newwin(1, max_x, 0, 0);
        }
    }
    
    /* Enable scrolling for log windows */
    scrollok(ui_ctx->std_win, TRUE);
    scrollok(ui_ctx->ucm_win, TRUE);
    
    /* Draw borders and titles */
    box(ui_ctx->map_win, 0, 0);
    box(ui_ctx->ust_win, 0, 0);
    box(ui_ctx->ucm_win, 0, 0);
    box(ui_ctx->std_win, 0, 0);
    
    mvwprintw(ui_ctx->map_win, 0, 2, " MAP ");
    mvwprintw(ui_ctx->ust_win, 0, 2, " UNIT STATS ");
    mvwprintw(ui_ctx->ucm_win, 0, 2, " CONSOLE ");
    mvwprintw(ui_ctx->std_win, 0, 2, " OUTPUT ");
    
    wrefresh(ui_ctx->map_win);
    wrefresh(ui_ctx->ust_win);
    wrefresh(ui_ctx->ucm_win);
    wrefresh(ui_ctx->std_win);
    
    /* Create FIFOs for communication if run_dir is provided */
    if (run_dir && run_dir[0] != '\0') {
        char std_fifo_path[600];
        snprintf(std_fifo_path, sizeof(std_fifo_path), "%s/ui_stdout.fifo", run_dir);
        
        unlink(std_fifo_path);  // Remove if exists
        if (mkfifo(std_fifo_path, 0666) == -1) {
            perror("mkfifo ui_stdout");
            // Non-critical, continue anyway
        }
    }
    
    return 0;
}

void ui_cleanup(ui_context_t *ui_ctx) {
    if (ui_ctx->map_win) delwin(ui_ctx->map_win);
    if (ui_ctx->ust_win) delwin(ui_ctx->ust_win);
    if (ui_ctx->ucm_win) delwin(ui_ctx->ucm_win);
    if (ui_ctx->std_win) delwin(ui_ctx->std_win);
    
    endwin();
    
    if (ui_ctx->std_fifo_fd != -1) close(ui_ctx->std_fifo_fd);
    if (ui_ctx->cm_in_fd != -1) close(ui_ctx->cm_in_fd);
    if (ui_ctx->cm_out_fd != -1) close(ui_ctx->cm_out_fd);
    
    /* Remove FIFOs */
    char fifo_path[600];
    snprintf(fifo_path, sizeof(fifo_path), "%s/ui_stdout.fifo", ui_ctx->run_dir);
    unlink(fifo_path);
    
    pthread_mutex_destroy(&ui_ctx->ui_lock);
}

void ui_refresh_all(ui_context_t *ui_ctx) {
    pthread_mutex_lock(&ui_ctx->ui_lock);
    
    wrefresh(ui_ctx->map_win);
    wrefresh(ui_ctx->ust_win);
    wrefresh(ui_ctx->ucm_win);
    wrefresh(ui_ctx->std_win);
    
    pthread_mutex_unlock(&ui_ctx->ui_lock);
}

int main(int argc, char **argv) {
    const char *ftok_path = "./ipc.key";
    char run_dir[512] = {0};
    
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
    if (ipc_attach(&ctx, ftok_path) == -1) {
        fprintf(stderr, "[UI] Failed to attach to IPC. Is command_center running?\n");
        return 1;
    }
    
    /* Initialize UI */
    if (ui_init(&g_ui_ctx, run_dir[0] ? run_dir : NULL) == -1) {
        fprintf(stderr, "[UI] Failed to initialize UI\n");
        ipc_detach(&ctx);
        return 1;
    }
    
    g_ui_ctx.ctx = &ctx;
    
    /* Start MAP thread */
    if (pthread_create(&g_ui_ctx.map_thread_id, NULL, ui_map_thread, &g_ui_ctx) != 0) {
        fprintf(stderr, "[UI] Failed to create MAP thread\n");
        ui_cleanup(&g_ui_ctx);
        ipc_detach(&ctx);
        return 1;
    }
    
    /* Start UST thread */
    if (pthread_create(&g_ui_ctx.ust_thread_id, NULL, ui_ust_thread, &g_ui_ctx) != 0) {
        fprintf(stderr, "[UI] Failed to create UST thread\n");
        g_ui_ctx.stop = 1;
        pthread_join(g_ui_ctx.map_thread_id, NULL);
        ui_cleanup(&g_ui_ctx);
        ipc_detach(&ctx);
        return 1;
    }
    
    /* Main loop - handle input and refresh */
    while (!g_ui_ctx.stop) {
        int ch = getch();
        
        if (ch == 'q' || ch == 'Q') {
            g_ui_ctx.stop = 1;
            break;
        }
        
        /* Refresh all windows */
        ui_refresh_all(&g_ui_ctx);
        
        usleep(50000);  // 50ms refresh rate
    }
    
    /* Wait for threads to finish */
    pthread_join(g_ui_ctx.map_thread_id, NULL);
    pthread_join(g_ui_ctx.ust_thread_id, NULL);
    
    /* Cleanup */
    ui_cleanup(&g_ui_ctx);
    ipc_detach(&ctx);
    
    return 0;
}
