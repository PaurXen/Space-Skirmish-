#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error handling levels */
typedef enum {
    ERR_FATAL,      /* Critical error - program cannot continue */
    ERR_ERROR,      /* Error condition - operation failed */
    ERR_WARNING     /* Warning - operation may have issues */
} error_level_t;

/* Custom error codes for application-specific errors */
typedef enum {
    ERR_OK = 0,
    ERR_INVALID_INPUT,
    ERR_INVALID_RANGE,
    ERR_INVALID_COORD,
    ERR_QUEUE_FULL,
    ERR_QUEUE_EMPTY,
    ERR_SHM_ERROR,
    ERR_SEM_ERROR,
    ERR_MSGQ_ERROR,
    ERR_FORK_ERROR,
    ERR_PIPE_ERROR,
    ERR_FILE_ERROR,
    ERR_MEMORY_ERROR,
    ERR_TIMEOUT,
    ERR_INVALID_STATE,
    ERR_UNIT_NOT_FOUND,
    ERR_WEAPON_NOT_FOUND,
    ERR_INVALID_UNIT_TYPE,
    ERR_INVALID_WEAPON_TYPE,
    ERR_PARSE_ERROR,
    ERR_IPC_ERROR,
    ERR_LOG_ERROR
} app_error_t;

/**
 * Main error handling function
 * 
 * @param level - severity level of the error
 * @param context - contextual information (e.g., function name, operation)
 * @param err_code - application error code or 0 if using errno
 * @param use_errno - if 1, use perror() and errno; if 0, use err_code
 * @param fmt - printf-style format string for additional message
 * @param ... - variable arguments for format string
 */
void handle_error(error_level_t level, const char *context, 
                 app_error_t err_code, int use_errno, 
                 const char *fmt, ...);

/**
 * Convenience macro for system call errors (using errno)
 * Automatically exits on FATAL errors
 */
#define HANDLE_SYS_ERROR(context, msg) \
    handle_error(ERR_FATAL, context, ERR_OK, 1, "%s", msg)

/**
 * Convenience macro for non-fatal system errors
 */
#define HANDLE_SYS_ERROR_NONFATAL(context, msg) \
    handle_error(ERR_ERROR, context, ERR_OK, 1, "%s", msg)

/**
 * Convenience macro for application errors
 */
#define HANDLE_APP_ERROR(level, context, err_code, msg) \
    handle_error(level, context, err_code, 0, "%s", msg)

/**
 * Check return value of system call and handle error if failed
 * Returns the value for further use
 */
#define CHECK_SYS_CALL(call, context) \
    ({ \
        int _ret = (call); \
        if (_ret == -1) { \
            HANDLE_SYS_ERROR(context, #call); \
        } \
        _ret; \
    })

/**
 * Check return value of system call (non-fatal)
 * Returns -1 on error, otherwise returns the value
 */
#define CHECK_SYS_CALL_NONFATAL(call, context) \
    ({ \
        int _ret = (call); \
        if (_ret == -1) { \
            HANDLE_SYS_ERROR_NONFATAL(context, #call); \
        } \
        _ret; \
    })

/**
 * Check pointer return value (e.g., malloc, fopen)
 */
#define CHECK_NULL(ptr, context) \
    ({ \
        void *_ptr = (ptr); \
        if (_ptr == NULL) { \
            HANDLE_SYS_ERROR(context, "NULL pointer returned"); \
        } \
        _ptr; \
    })

/**
 * Check pointer return value (non-fatal)
 */
#define CHECK_NULL_NONFATAL(ptr, context) \
    ({ \
        void *_ptr = (ptr); \
        if (_ptr == NULL) { \
            HANDLE_SYS_ERROR_NONFATAL(context, "NULL pointer returned"); \
        } \
        _ptr; \
    })

/**
 * Validate integer range
 */
int validate_int_range(int value, int min, int max, const char *context);

/**
 * Validate coordinate (0-based)
 */
int validate_coordinate(int x, int y, int max_x, int max_y, const char *context);

/**
 * Validate string input (non-empty, length limits)
 */
int validate_string(const char *str, size_t min_len, size_t max_len, const char *context);

/**
 * Get error message for application error code
 */
const char* get_error_message(app_error_t err_code);

#endif /* ERROR_HANDLER_H */
