#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include "lvgl/lvgl.h"

// Log levels
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_CRITICAL
} log_level_t;

// Log entry structure
typedef struct {
    char timestamp[20];          // Format: YYYY-MM-DD HH:MM:SS
    log_level_t level;
    char message[256];
} log_entry_t;

extern lv_color_t log_level_colors[5];
extern const char *log_level_names[5];

// Initialize the logger
void logger_init(void);

// Set minimum log level to display
void logger_set_level(log_level_t level);

// Log functions for different levels
void log_debug(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_warning(const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_critical(const char *fmt, ...);

// Get logs for display
const log_entry_t* logger_get_logs(uint32_t *count);

// Clear all logs
void logger_clear(void);

// Update the logs display in the UI
void logger_update_ui(lv_obj_t *log_container);

#endif /* LOGGER_H */