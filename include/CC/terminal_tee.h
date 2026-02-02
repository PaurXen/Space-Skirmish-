#pragma once
#include <sys/types.h>

/* Start a small 'tee' process that mirrors the program's stdout/stderr into a file:
 *  - creates a pipe, forks a child that reads from pipe and writes to both
 *    the real terminal (stdout) and a run-specific ALL.term.log file.
 *  - parent redirects its stdout/stderr to the pipe so all output is captured.
 * Returns pid of tee child or -1 on error.
 */
pid_t start_terminal_tee(const char *log_path);
