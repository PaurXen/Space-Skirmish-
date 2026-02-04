// terminal_tee.c - Standalone terminal output duplicator
#define _GNU_SOURCE

#include "tee/terminal_tee.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif

#define BUFFER_SIZE 4096

static void ignore_sig(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    (void)sigaction(sig, &sa, NULL);
}

/* Worker process that reads from pipe and writes to log + terminal/UI */
static void tee_worker(int pipe_fd, const char *log_path, const char *ui_pipe_path) {
#ifdef __linux__
    prctl(PR_SET_NAME, "terminal_tee", 0, 0, 0);
#endif

    // Ignore signals - only exit on pipe EOF
    ignore_sig(SIGINT);
    ignore_sig(SIGTERM);
    ignore_sig(SIGHUP);

    // Open log file for appending
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd == -1) {
        perror("tee: open log file");
        _exit(1);
    }

    // Check if UI pipe exists and is writable
    int ui_fd = -1;
    int use_ui = 0;
    
    if (access(ui_pipe_path, F_OK) == 0) {
        // UI pipe exists - try to open (non-blocking to avoid hanging)
        ui_fd = open(ui_pipe_path, O_WRONLY | O_NONBLOCK);
        if (ui_fd != -1) {
            use_ui = 1;
            // Make it blocking after successful connection
            int flags = fcntl(ui_fd, F_GETFL);
            fcntl(ui_fd, F_SETFL, flags & ~O_NONBLOCK);
        }
    }

    // Open terminal output
    int term_fd = -1;
    if (!use_ui) {
        // No UI - write to terminal
        term_fd = open("/dev/tty", O_WRONLY);
        if (term_fd == -1) {
            // Fallback to stdout (probably won't work since CC redirected it)
            term_fd = STDOUT_FILENO;
        }
    }

    // Main loop - read from pipe, write to log and terminal/UI
    char buffer[BUFFER_SIZE];
    ssize_t bytes;
    int read_count = 0;

    while ((bytes = read(pipe_fd, buffer, sizeof(buffer))) > 0) {
        read_count++;
        
        // Write to log file
        ssize_t written = 0;
        while (written < bytes) {
            ssize_t w = write(log_fd, buffer + written, bytes - written);
            if (w <= 0) break;
            written += w;
        }

        // Write to terminal or UI
        if (use_ui && ui_fd != -1) {
            // Send to UI [STD] thread via pipe
            written = 0;
            while (written < bytes) {
                ssize_t w = write(ui_fd, buffer + written, bytes - written);
                if (w <= 0) {
                    // UI pipe closed (EPIPE) or broken - fall back to terminal immediately
                    use_ui = 0;
                    close(ui_fd);
                    ui_fd = -1;
                    
                    // Open terminal for output
                    term_fd = open("/dev/tty", O_WRONLY);
                    if (term_fd == -1) {
                        term_fd = STDOUT_FILENO;
                    }
                    
                    // Write remaining data to terminal
                    ssize_t term_written = 0;
                    while (term_written < bytes) {
                        ssize_t tw = write(term_fd, buffer + term_written, bytes - term_written);
                        if (tw <= 0) break;
                        term_written += tw;
                    }
                    break;
                }
                written += w;
            }
        } else if (term_fd != -1) {
            // Send to terminal
            written = 0;
            while (written < bytes) {
                ssize_t w = write(term_fd, buffer + written, bytes - written);
                if (w <= 0) break;
                written += w;
            }
        }

        // Every 10 reads, check if UI pipe appeared or disappeared
        if (read_count % 1 == 0) {
            if (!use_ui && access(ui_pipe_path, F_OK) == 0) {
                // UI pipe appeared - try to connect
                ui_fd = open(ui_pipe_path, O_WRONLY | O_NONBLOCK);
                if (ui_fd != -1) {
                    use_ui = 1;
                    int flags = fcntl(ui_fd, F_GETFL);
                    fcntl(ui_fd, F_SETFL, flags & ~O_NONBLOCK);
                    
                    // Close terminal fd
                    if (term_fd != -1 && term_fd != STDOUT_FILENO) {
                        close(term_fd);
                    }
                    term_fd = -1;
                }
            } else if (use_ui && access(ui_pipe_path, F_OK) != 0) {
                // UI pipe disappeared - UI was closed
                use_ui = 0;
                if (ui_fd != -1) {
                    close(ui_fd);
                    ui_fd = -1;
                }
                term_fd = open("/dev/tty", O_WRONLY);
                if (term_fd == -1) {
                    term_fd = STDOUT_FILENO;
                }
            }
        }
    }

    // EOF or error - cleanup and exit
    close(log_fd);
    if (ui_fd != -1) close(ui_fd);
    if (term_fd != -1 && term_fd != STDOUT_FILENO) close(term_fd);
    
    _exit(0);
}

int start_terminal_tee(const char *run_dir) {
    int pfd[2];
    if (pipe(pfd) == -1) {
        perror("tee: pipe");
        return -1;
    }

    // Construct paths
    char log_path[600];
    char ui_pipe_path[600];
    snprintf(log_path, sizeof(log_path), "%s/ALL.term.log", run_dir);
    snprintf(ui_pipe_path, sizeof(ui_pipe_path), "/tmp/skirmish_std.fifo");

    // Double fork to detach tee worker completely
    pid_t first = fork();
    if (first == -1) {
        perror("tee: fork");
        close(pfd[0]);
        close(pfd[1]);
        return -1;
    }

    if (first == 0) {
        // First child - fork again and exit immediately
        pid_t worker = fork();
        if (worker == -1) _exit(1);

        if (worker == 0) {
            // Grandchild - the actual tee worker
            close(pfd[1]);  // Close write end
            
            // Become session leader to fully detach
            setsid();
            
            // Run the tee worker
            tee_worker(pfd[0], log_path, ui_pipe_path);
            _exit(0);  // Should never reach here
        }

        // First child exits immediately
        _exit(0);
    }

    // Parent (CC) - close read end, return write end
    close(pfd[0]);
    
    // Wait for first child to prevent zombie
    waitpid(first, NULL, 0);

    return pfd[1];
}
