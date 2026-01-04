#ifndef LOG_H
#define LOG_H

#include <stdint.h>

/* Simple per-process logging API used across the project.
 *
 * - log_init(role, unit_id): open per-process log file and global ALL.log in
 *   the run directory. role is a short string ("CC","BS",...). unit_id is 0 for CC.
 * - log_close(): flush/close logs.
 * - log_set_level(): adjust verbosity (default INFO).
 * - log_msg(): printf-like logging; writes to per-process log and global ALL.log.
 * - log_printf(): write a line to both stdout and the logs.
 *
 * Convenience macros LOGD/LOGI/LOGW/LOGE map to log_msg with levels.
 */

typedef enum {
    LOG_LVL_DEBUG = 0,
    LOG_LVL_INFO = 1,
    LOG_LVL_WARN = 2,
    LOG_LVL_ERROR = 3
} log_level_t;

/* Initialize logger for this process.
 * - role: short string identifying process role ("CC", "BS", ...).
 * - unit_id: 0 for CC, otherwise the unit id.
 * - Returns 0 on success, -1 on failure.
 */
int log_init(const char *role, uint16_t unit_id);

/* Close logger (idempotent). */
void log_close(void);

/* Set minimum log level; messages below this level are dropped. */
void log_set_level(log_level_t lvl);

/* Log line (printf-like). */
void log_msg(log_level_t lvl, const char *fmt, ...);

/* Write same line to stdout and logs. */
void log_printf(const char *fmt, ...);

/* Convenience macros */
#define LOGD(...) log_msg(LOG_LVL_DEBUG, __VA_ARGS__)
#define LOGI(...) log_msg(LOG_LVL_INFO,  __VA_ARGS__)
#define LOGW(...) log_msg(LOG_LVL_WARN,  __VA_ARGS__)
#define LOGE(...) log_msg(LOG_LVL_ERROR, __VA_ARGS__)

#endif