#include "error_handler.h"
#include "log.h"
#include <stdarg.h>
#include <string.h>

/* Error level strings for logging */
static const char *error_level_str[] = {
    [ERR_FATAL]   = "FATAL",
    [ERR_ERROR]   = "ERROR",
    [ERR_WARNING] = "WARNING"
};

/* Application error messages */
static const char *app_error_messages[] = {
    [ERR_OK]                  = "Success",
    [ERR_INVALID_INPUT]       = "Invalid input",
    [ERR_INVALID_RANGE]       = "Value out of valid range",
    [ERR_INVALID_COORD]       = "Invalid coordinates",
    [ERR_QUEUE_FULL]          = "Queue is full",
    [ERR_QUEUE_EMPTY]         = "Queue is empty",
    [ERR_SHM_ERROR]           = "Shared memory error",
    [ERR_SEM_ERROR]           = "Semaphore error",
    [ERR_MSGQ_ERROR]          = "Message queue error",
    [ERR_FORK_ERROR]          = "Fork error",
    [ERR_PIPE_ERROR]          = "Pipe error",
    [ERR_FILE_ERROR]          = "File operation error",
    [ERR_MEMORY_ERROR]        = "Memory allocation error",
    [ERR_TIMEOUT]             = "Operation timeout",
    [ERR_INVALID_STATE]       = "Invalid state",
    [ERR_UNIT_NOT_FOUND]      = "Unit not found",
    [ERR_WEAPON_NOT_FOUND]    = "Weapon not found",
    [ERR_INVALID_UNIT_TYPE]   = "Invalid unit type",
    [ERR_INVALID_WEAPON_TYPE] = "Invalid weapon type",
    [ERR_PARSE_ERROR]         = "Parse error",
    [ERR_IPC_ERROR]           = "IPC communication error",
    [ERR_LOG_ERROR]           = "Logging error"
};

const char* get_error_message(app_error_t err_code) {
    if (err_code >= 0 && err_code < sizeof(app_error_messages)/sizeof(app_error_messages[0])) {
        return app_error_messages[err_code];
    }
    return "Unknown error";
}

void handle_error(error_level_t level, const char *context, 
                 app_error_t err_code, int use_errno, 
                 const char *fmt, ...) {
    char buffer[512];
    char final_msg[1024];
    va_list args;
    
    /* Format the custom message */
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    /* Build the complete error message */
    if (use_errno) {
        /* System error - use perror and errno */
        snprintf(final_msg, sizeof(final_msg), 
                "[%s] %s: %s - %s (errno=%d)", 
                error_level_str[level], context, buffer, 
                strerror(errno), errno);
        
        /* Also use perror for standard error output */
        char perror_msg[256];
        snprintf(perror_msg, sizeof(perror_msg), "[%s] %s: %s", 
                error_level_str[level], context, buffer);
        perror(perror_msg);
    } else {
        /* Application error */
        const char *err_msg = get_error_message(err_code);
        snprintf(final_msg, sizeof(final_msg), 
                "[%s] %s: %s - %s (code=%d)", 
                error_level_str[level], context, buffer, 
                err_msg, err_code);
        
        /* Print to stderr */
        fprintf(stderr, "%s\n", final_msg);
    }
    
    /* Log the error if logging is initialized */
    switch (level) {
        case ERR_FATAL:
            LOGE("%s", final_msg);
            /* Fatal errors cause program termination */
            log_close();
            exit(EXIT_FAILURE);
            break;
        case ERR_ERROR:
            LOGE("%s", final_msg);
            break;
        case ERR_WARNING:
            LOGW("%s", final_msg);
            break;
    }
}

int validate_int_range(int value, int min, int max, const char *context) {
    if (value < min || value > max) {
        handle_error(ERR_ERROR, context, ERR_INVALID_RANGE, 0,
                    "Value %d not in range [%d, %d]", value, min, max);
        return -1;
    }
    return 0;
}

int validate_coordinate(int x, int y, int max_x, int max_y, const char *context) {
    if (x < 0 || x >= max_x || y < 0 || y >= max_y) {
        handle_error(ERR_ERROR, context, ERR_INVALID_COORD, 0,
                    "Coordinates (%d, %d) out of bounds [0-%d, 0-%d]", 
                    x, y, max_x-1, max_y-1);
        return -1;
    }
    return 0;
}

int validate_string(const char *str, size_t min_len, size_t max_len, const char *context) {
    if (str == NULL) {
        handle_error(ERR_ERROR, context, ERR_INVALID_INPUT, 0,
                    "NULL string");
        return -1;
    }
    
    size_t len = strlen(str);
    if (len < min_len || len > max_len) {
        handle_error(ERR_ERROR, context, ERR_INVALID_INPUT, 0,
                    "String length %zu not in range [%zu, %zu]", 
                    len, min_len, max_len);
        return -1;
    }
    
    return 0;
}
