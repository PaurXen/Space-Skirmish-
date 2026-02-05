// UI STD thread - receives output from terminal_tee
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <ncurses.h>

#include "UI/ui.h"
#include "log.h"
#include "error_handler.h"

#define FIFO_PATH "/tmp/skirmish_std.fifo"
#define BUFFER_SIZE 4096

/* STD thread - reads from tee FIFO and displays in std_win */
void *ui_std_thread(void *arg) {
    ui_context_t *ui_ctx = (ui_context_t *)arg;
    char buffer[BUFFER_SIZE];
    int fifo_fd = -1;
    int created_fifo = 0;
    
    /* Try to create FIFO if it doesn't exist */
    if (access(FIFO_PATH, F_OK) != 0) {
        if (mkfifo(FIFO_PATH, 0666) == 0) {
            created_fifo = 1;
        } else if (errno != EEXIST) {
            /* Failed to create FIFO - not critical, just won't receive tee output */
            HANDLE_SYS_ERROR_NONFATAL("ui_std:mkfifo", "Failed to create FIFO");
            pthread_mutex_lock(&ui_ctx->ui_lock);
            if (ui_ctx->std_win) {
                mvwprintw(ui_ctx->std_win, 1, 1, "Warning: Could not create FIFO");
                wrefresh(ui_ctx->std_win);
            }
            pthread_mutex_unlock(&ui_ctx->ui_lock);
        }
    } else {
        /* FIFO already exists (maybe from previous run) - we'll use it */
        created_fifo = 1;
    }
    
    pthread_mutex_lock(&ui_ctx->ui_lock);
    if (ui_ctx->std_win) {
        mvwprintw(ui_ctx->std_win, 1, 1, "[STD] Waiting for command_center...");
        wrefresh(ui_ctx->std_win);
    }
    pthread_mutex_unlock(&ui_ctx->ui_lock);
    
    /* Open FIFO in BLOCKING mode - this will wait for a writer (tee) to connect */
    /* This is the correct way to use FIFOs - reader blocks until writer appears */
    fifo_fd = CHECK_SYS_CALL_NONFATAL(open(FIFO_PATH, O_RDONLY), "ui_std:open");
    
    if (fifo_fd == -1) {
        pthread_mutex_lock(&ui_ctx->ui_lock);
        if (ui_ctx->std_win) {
            mvwprintw(ui_ctx->std_win, 1, 1, "[STD] Failed to open FIFO: %s", strerror(errno));
            wrefresh(ui_ctx->std_win);
        }
        pthread_mutex_unlock(&ui_ctx->ui_lock);
        
        /* Still remove FIFO if we created it */
        if (created_fifo) {
            unlink(FIFO_PATH);
        }
        return NULL;
    }
    
    ui_ctx->std_fifo_fd = fifo_fd;
    
    pthread_mutex_lock(&ui_ctx->ui_lock);
    if (ui_ctx->std_win) {
        mvwprintw(ui_ctx->std_win, 1, 1, "[STD] Connected to tee");
        wrefresh(ui_ctx->std_win);
    }
    pthread_mutex_unlock(&ui_ctx->ui_lock);
    
    LOGI("[UI-STD] Connected to tee via FIFO");
    
    /* Read loop */
    int line = 2; // Start after title and status message
    int max_y, max_x;
    
    while (!ui_ctx->stop) {
        ssize_t bytes = read(fifo_fd, buffer, sizeof(buffer) - 1);
        
        if (bytes <= 0) {
            if (bytes == 0) {
                /* EOF - tee closed the pipe */
                pthread_mutex_lock(&ui_ctx->ui_lock);
                if (ui_ctx->std_win) {
                    mvwprintw(ui_ctx->std_win, 1, 1, "[STD] Tee disconnected     ");
                    wrefresh(ui_ctx->std_win);
                }
                pthread_mutex_unlock(&ui_ctx->ui_lock);
                LOGI("[UI-STD] Tee disconnected (EOF)");
                break;
            }
            
            if (errno == EINTR || errno == EAGAIN) continue;
            
            /* Other error */
            break;
        }
        
        buffer[bytes] = '\0';
        
        /* Write to window */
        pthread_mutex_lock(&ui_ctx->ui_lock);
        
        if (!ui_ctx->std_win) {
            pthread_mutex_unlock(&ui_ctx->ui_lock);
            break;
        }
        
        getmaxyx(ui_ctx->std_win, max_y, max_x);
        
        /* Parse buffer and write line by line */
        char *line_start = buffer;
        char *newline;
        
        while ((newline = strchr(line_start, '\n')) != NULL) {
            *newline = '\0';
            
            /* Wrap long lines */
            int len = strlen(line_start);
            int max_line_width = max_x - 2;  // Account for borders
            
            if (len == 0) {
                /* Empty line - just move to next line */
                if (line >= max_y - 1) {
                    wscrl(ui_ctx->std_win, 1);
                    line = max_y - 2;
                }
                /* Clear the line */
                wmove(ui_ctx->std_win, line, 1);
                wclrtoeol(ui_ctx->std_win);
                line++;
            } else {
                /* Print line with wrapping */
                int offset = 0;
                while (offset < len) {
                    /* Scroll if needed */
                    if (line >= max_y - 1) {
                        wscrl(ui_ctx->std_win, 1);
                        line = max_y - 2;
                    }
                    
                    /* Clear line first */
                    wmove(ui_ctx->std_win, line, 1);
                    wclrtoeol(ui_ctx->std_win);
                    
                    /* Print chunk of line that fits */
                    int chunk_len = (len - offset > max_line_width) ? max_line_width : (len - offset);
                    mvwprintw(ui_ctx->std_win, line, 1, "%.*s", chunk_len, line_start + offset);
                    line++;
                    offset += chunk_len;
                }
            }
            
            line_start = newline + 1;
        }
        
        /* Handle remaining partial line (no newline at end) */
        if (*line_start) {
            int len = strlen(line_start);
            int max_line_width = max_x - 2;
            int offset = 0;
            
            while (offset < len) {
                if (line >= max_y - 1) {
                    wscrl(ui_ctx->std_win, 1);
                    line = max_y - 2;
                }
                
                /* Clear line first */
                wmove(ui_ctx->std_win, line, 1);
                wclrtoeol(ui_ctx->std_win);
                
                int chunk_len = (len - offset > max_line_width) ? max_line_width : (len - offset);
                mvwprintw(ui_ctx->std_win, line, 1, "%.*s", chunk_len, line_start + offset);
                offset += chunk_len;
                if (offset < len) line++;  /* Only increment line if we're continuing */
            }
        }
        
        /* Refresh without redrawing border - border is static */
        wrefresh(ui_ctx->std_win);
        pthread_mutex_unlock(&ui_ctx->ui_lock);
    }
    
    /* Cleanup */
    LOGI("[UI-STD] Cleaning up STD thread");
    
    if (fifo_fd != -1) {
        close(fifo_fd);
        ui_ctx->std_fifo_fd = -1;
    }
    
    /* Remove FIFO to signal tee that UI is gone */
    if (created_fifo || access(FIFO_PATH, F_OK) == 0) {
        LOGI("[UI-STD] Removing FIFO at %s", FIFO_PATH);
        unlink(FIFO_PATH);
    }
    
    return NULL;
}
