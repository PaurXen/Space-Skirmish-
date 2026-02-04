#define _GNU_SOURCE
#include "log.h"
#include "error_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/* Simple logging backend used by the project.
 *
 * - Each process has a per-process FILE* log (g_logf) for its own messages.
 * - All processes append lines to a single combined file ALL.log (g_all_fd)
 *   opened with O_APPEND so writes are atomic per line.
 * - The run directory is taken from SKIRMISH_RUN_DIR or defaults to "logs".
 * - log_msg formats a timestamped line and writes it to both destinations.
 */

/* per-process log (FILE*) */
static FILE *g_logf = NULL;
/* global combined log (fd opened with O_APPEND for atomic line appends) */
static int g_all_fd = -1;

static log_level_t g_min_lvl = LOG_LVL_DEBUG;
static char g_role[8] = "??";
static uint16_t g_unit_id = 0;

/* directory for this run (default "logs", overridden by env) */
static char g_run_dir[512] = "logs";

static const char *lvl_name(log_level_t lvl) {
    switch (lvl) {
        case LOG_LVL_DEBUG: return "DEBUG";
        case LOG_LVL_INFO:  return "INFO ";
        case LOG_LVL_WARN:  return "WARN ";
        case LOG_LVL_ERROR: return "ERROR";
        default:            return "UNK";
    }
}

/* Ensure a directory exists; print to stderr if path exists but is not a
 * directory. Create with mode 0755 when absent.
 */
static void ensure_dir_exists(const char *dir) {
    struct stat st;
    if (stat(dir, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return;
        fprintf(stderr, "[LOG] '%s' exists but is not a directory!\n", dir);
        return;
    }
    if (mkdir(dir, 0755) == -1 && errno != EEXIST) {
        perror("[LOG] mkdir");
        fprintf(stderr, "[LOG] Failed to create directory '%s': %s (errno=%d)\n", 
                dir, strerror(errno), errno);
    }
}

/* Open the combined ALL.log in the run directory (O_APPEND). */
static void open_global_log(void) {
    if (g_all_fd != -1) return;

    ensure_dir_exists(g_run_dir);

    char all_path[600];
    snprintf(all_path, sizeof(all_path), "%s/ALL.log", g_run_dir);

    g_all_fd = open(all_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (g_all_fd == -1) {
        perror("[LOG] open ALL.log");
        fprintf(stderr, "[LOG] Failed to open '%s': %s (errno=%d)\n", 
                all_path, strerror(errno), errno);
    }
}

/* Initialize logging for this process.
 * - role: short identifier string ("CC","BS",...)
 * - unit_id: 0 for CC, otherwise the unit id.
 * Creates per-process log file under run dir and opens ALL.log.
 * Returns 0 on success, -1 on failure.
 */
int log_init(const char *role, int16_t unit_id) {
    /* pick run directory from env if set (inherited by child processes) */
    const char *rd = getenv("SKIRMISH_RUN_DIR");
    if (rd && *rd) {
        strncpy(g_run_dir, rd, sizeof(g_run_dir) - 1);
        g_run_dir[sizeof(g_run_dir) - 1] = '\0';
    } else {
        /* Try to read from CC's run_dir file (for CM/UI started independently) */
        FILE *f = fopen("/tmp/skirmish_run_dir.txt", "r");
        if (f) {
            if (fgets(g_run_dir, sizeof(g_run_dir), f)) {
                /* Remove trailing newline */
                size_t len = strlen(g_run_dir);
                if (len > 0 && g_run_dir[len-1] == '\n') {
                    g_run_dir[len-1] = '\0';
                }
            } else {
                strncpy(g_run_dir, "logs", sizeof(g_run_dir) - 1);
                g_run_dir[sizeof(g_run_dir) - 1] = '\0';
            }
            fclose(f);
        } else {
            strncpy(g_run_dir, "logs", sizeof(g_run_dir) - 1);
            g_run_dir[sizeof(g_run_dir) - 1] = '\0';
        }
    }

    ensure_dir_exists("logs");     /* base */
    ensure_dir_exists(g_run_dir);  /* run dir (may be "logs" too) */

    g_unit_id = unit_id;
    strncpy(g_role, role ? role : "??", sizeof(g_role) - 1);
    g_role[sizeof(g_role) - 1] = '\0';

    char path[600];
    pid_t pid = getpid();

    if (unit_id == 0) {
        snprintf(path, sizeof(path), "%s/%s_pid_%d.log", g_run_dir, g_role, (int)pid);
    } else {
        snprintf(path, sizeof(path), "%s/%s_u%u_pid_%d.log",
                 g_run_dir, g_role, (unsigned)unit_id, (int)pid);
    }

    g_logf = fopen(path, "a");  /* append */
    if (!g_logf) {
        perror("[LOG] fopen per-process log");
        fprintf(stderr, "[LOG] Failed to open log file '%s': %s (errno=%d)\n", 
                path, strerror(errno), errno);
        return -1;
    }

    /* Make per-process log line-buffered for timely flushing. */
    setvbuf(g_logf, NULL, _IOLBF, 0);

    open_global_log();

    /* Emit startup header to both logs. */
    log_msg(LOG_LVL_INFO, "logger started (role=%s unit=%u pid=%d run_dir=%s)",
            g_role, (unsigned)unit_id, (int)pid, g_run_dir);

    return 0;
}

/* Close logs (idempotent). */
void log_close(void) {
    if (g_logf) {
        log_msg(LOG_LVL_INFO, "logger closing");
        if (fclose(g_logf) != 0) {
            perror("[LOG] fclose per-process log");
            fprintf(stderr, "[LOG] Error closing per-process log: %s (errno=%d)\n", 
                    strerror(errno), errno);
        }
        g_logf = NULL;
    }
    if (g_all_fd != -1) {
        if (close(g_all_fd) == -1) {
            perror("[LOG] close ALL.log");
            fprintf(stderr, "[LOG] Error closing ALL.log: %s (errno=%d)\n", 
                    strerror(errno), errno);
        }
        g_all_fd = -1;
    }
}

/* Adjust minimum log level; messages below this are dropped. */
void log_set_level(log_level_t lvl) {
    g_min_lvl = lvl;
}

/* Format a timestamped log line and write to per-process log and ALL.log.
 * - Lines are terminated with '\n'. The combined log uses write(2) to ensure
 *   O_APPEND append atomicy across processes.
 */
void log_msg(log_level_t lvl, const char *fmt, ...) {
    if (lvl < g_min_lvl) return;

    char line[1024];

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);

    int len = snprintf(
        line, sizeof(line),
        "%04d-%02d-%02d %02d:%02d:%02d.%03ld [%s] %s u=%u pid=%d: ",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000,
        lvl_name(lvl), g_role, (unsigned)g_unit_id, (int)getpid()
    );

    va_list ap;
    va_start(ap, fmt);
    len += vsnprintf(line + len, sizeof(line) - (size_t)len, fmt, ap);
    va_end(ap);

    if (len < (int)sizeof(line) - 1) {
        line[len++] = '\n';
        line[len] = '\0';
    } else {
        line[sizeof(line) - 2] = '\n';
        line[sizeof(line) - 1] = '\0';
        len = (int)sizeof(line) - 1;
    }

    /* Write to per-process log file if available. */
    if (g_logf) {
        if (fwrite(line, 1, (size_t)len, g_logf) != (size_t)len) {
            perror("[LOG] fwrite to per-process log");
        }
        if (fflush(g_logf) != 0) {
            perror("[LOG] fflush");
        }
    }

    /* Append atomically to combined ALL.log if available. */
    if (g_all_fd != -1) {
        ssize_t written = write(g_all_fd, line, (size_t)len);
        if (written == -1) {
            perror("[LOG] write to ALL.log");
        } else if (written != len) {
            fprintf(stderr, "[LOG] Partial write to ALL.log: %zd/%d bytes\n", 
                    written, len);
        }
    }
}
