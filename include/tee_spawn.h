#pragma once

/* Start the standalone tee process that reads from a pipe and writes to:
 *  - run_dir/ALL.term.log (always)
 *  - /dev/tty (if no UI connection)
 *  - run_dir/ui_stdout.fifo (if UI is connected)
 *
 * Creates a pipe, forks to launch ./tee as independent process, then
 * redirects caller's stdout/stderr to the pipe.
 *
 * Returns 0 on success, -1 on error.
 * The tee process runs independently and exits when caller closes the pipe.
 */
int tee_spawn_and_redirect(const char *run_dir);
