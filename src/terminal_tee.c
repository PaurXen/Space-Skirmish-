// terminal_tee.c
#define _GNU_SOURCE

#include "terminal_tee.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif

static void ignore_sig(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    (void)sigaction(sig, &sa, NULL);
}

pid_t start_terminal_tee(const char *run_dir) {
    int pfd[2];
    if (pipe(pfd) == -1) {
        perror("pipe");
        return -1;
    }

    // Open ALL.term.log now (so we can fail early in parent if needed)
    char out_path[600];
    snprintf(out_path, sizeof(out_path), "%s/ALL.term.log", run_dir);

    int out_fd = open(out_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (out_fd == -1) {
        perror("open ALL.term.log");
        close(pfd[0]);
        close(pfd[1]);
        return -1;
    }

    // First fork: CC will waitpid() this pid, but it will exit immediately.
    pid_t shortlived = fork();
    if (shortlived == -1) {
        perror("fork tee");
        close(out_fd);
        close(pfd[0]);
        close(pfd[1]);
        return -1;
    }

    if (shortlived == 0) {
        // ---- child #1 (short-lived) ----
        pid_t worker = fork();
        if (worker == -1) _exit(1);

        if (worker == 0) {
            // ---- grandchild (real tee worker) ----
#ifdef __linux__
            prctl(PR_SET_NAME, "terminal_tee", 0, 0, 0);
#endif
            // As requested: ignore SIGINT/SIGTERM; exit only on EOF
            ignore_sig(SIGINT);
            ignore_sig(SIGTERM);

            // We only read from pipe in worker
            close(pfd[1]); // close write end

            // Keep out_fd and pfd[0]. Now exec into a small helper:
            //
            // We use /bin/sh to run a tiny loop that:
            // - reads from stdin (pipe)
            // - writes to real terminal (/dev/tty if possible, else stdout)
            // - appends to ALL.term.log
            //
            // NOTE: argv[0] is set to "terminal_tee" so `ps -u` shows it nicely.
            //
            // Use /dev/tty to ensure it writes to terminal even though CC stdout is piped.
            const char *script =
                "TERMOUT=/dev/tty; "
                "[ -w \"$TERMOUT\" ] || TERMOUT=/proc/self/fd/1; "
                "cat | tee -a \"$1\" > \"$TERMOUT\"";

            // Ensure pfd[0] becomes stdin
            if (dup2(pfd[0], STDIN_FILENO) == -1) _exit(2);
            close(pfd[0]);

            // Ensure out_fd is closed; we pass path to tee instead
            close(out_fd);

            // Exec: process name becomes not ./command_center
            char *argv[] = { (char*)"terminal_tee", (char*)"-c", (char*)script,
                             (char*)"sh", (char*)out_path, NULL };
            execv("/bin/sh", argv);

            _exit(127);
        }

        // child #1 exits immediately => CC won't block waiting for it
        _exit(0);
    }

    // ---- parent (CC) ----
    // Redirect CC stdout/stderr into pipe
    close(pfd[0]); // close read end in CC
    close(out_fd);

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    if (dup2(pfd[1], STDOUT_FILENO) == -1) perror("dup2 stdout");
    if (dup2(pfd[1], STDERR_FILENO) == -1) perror("dup2 stderr");
    close(pfd[1]);

    // Return pid of short-lived child (CC may waitpid it; it exits immediately)
    return shortlived;
}
