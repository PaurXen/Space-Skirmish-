#pragma once
#include <sys/types.h>

/* Start terminal tee as a detached background process.
 * Creates a pipe for CC output, forks worker that:
 *  - Reads from pipe
 *  - Writes to log file (ALL.term.log)
 *  - Writes to terminal OR UI [STD] pipe (if available)
 * 
 * Returns pipe write fd for CC to redirect stdout/stderr to, or -1 on error.
 */
int start_terminal_tee(const char *run_dir);
