#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "logger.h"
#include "lvgl/lvgl.h"
#include "../main.h"

// Log level strings - make them accessible
const char *log_level_names[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "CRITICAL"
};

lv_color_t log_level_colors[5];

// Logger state
static struct {
    log_entry_t entries[MAX_LOG_ENTRIES];
    uint32_t current_index;
    uint32_t count;
    log_level_t min_level;
} logger_state;

// Structure to pass scroll information to the timer
typedef struct {
    lv_obj_t *container;
    lv_coord_t scroll_y;
} scroll_restore_data_t;

/**
 * Initialize the logger
 */
void logger_init(void) {
    memset(&logger_state, 0, sizeof(logger_state));
    logger_state.min_level = INITIAL_LOG_LEVEL;

    // Initialize colors at runtime with better contrast for light backgrounds
    log_level_colors[0] = lv_color_make(80, 80, 100);    // DEBUG - Blue-gray (darker)
    log_level_colors[1] = lv_color_make(0, 128, 64);     // INFO - Darker green
    log_level_colors[2] = lv_color_make(180, 120, 0);    // WARNING - Amber/gold
    log_level_colors[3] = lv_color_make(210, 80, 0);     // ERROR - Darker orange
    log_level_colors[4] = lv_color_make(200, 0, 0);      // CRITICAL - Slightly darker red
}

/**
 * Set minimum log level to display
 */
void logger_set_level(log_level_t level) {
    logger_state.min_level = level;
}

/**
 * Internal function to add a log entry
 */
static void add_log_entry(log_level_t level, const char *fmt, va_list args) {
    // Check if we should log this level
    if (level < logger_state.min_level) {
        return;
    }
    
    // Get current time
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    // Create new log entry
    log_entry_t *entry = &logger_state.entries[logger_state.current_index];
    
    // Format timestamp
    strftime(entry->timestamp, sizeof(entry->timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Set log level
    entry->level = level;
    
    // Format message
    vsnprintf(entry->message, sizeof(entry->message), fmt, args);
    
    // Also output to console
    fprintf(level >= LOG_LEVEL_WARNING ? stderr : stdout, 
        "[%s] %s: %s\n", 
        entry->timestamp, 
        log_level_names[level],
        entry->message);
    
    // Update indices
    logger_state.current_index = (logger_state.current_index + 1) % MAX_LOG_ENTRIES;
    if (logger_state.count < MAX_LOG_ENTRIES) {
        logger_state.count++;
    }
}

/**
 * Log a debug message
 */
void log_debug(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    add_log_entry(LOG_LEVEL_DEBUG, fmt, args);
    va_end(args);
}

/**
 * Log an info message
 */
void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    add_log_entry(LOG_LEVEL_INFO, fmt, args);
    va_end(args);
}

/**
 * Log a warning message
 */
void log_warning(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    add_log_entry(LOG_LEVEL_WARNING, fmt, args);
    va_end(args);
}

/**
 * Log an error message
 */
void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    add_log_entry(LOG_LEVEL_ERROR, fmt, args);
    va_end(args);
}

/**
 * Log a critical message
 */
void log_critical(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    add_log_entry(LOG_LEVEL_CRITICAL, fmt, args);
    va_end(args);
}

/**
 * Get logs for display
 */
const log_entry_t* logger_get_logs(uint32_t *count) {
    if (count) {
        *count = logger_state.count;
    }
    return logger_state.entries;
}

/**
 * Clear all logs
 */
void logger_clear(void) {
    logger_state.current_index = 0;
    logger_state.count = 0;
}

/**
 * Update the logs display in the UI
 */
void logger_update_ui(lv_obj_t *log_container) {
    static uint32_t last_index = 0;
    static uint32_t last_count = 0;

    // Use index to verify if there are new entries in the log
    if (logger_state.current_index == last_index) {
        return;
    }
    
    // Store scroll position
    lv_coord_t scroll_y = lv_obj_get_scroll_y(log_container);
    
    // Only add new entries
    uint32_t current_count;
    const log_entry_t *logs = logger_get_logs(&current_count);

    for (uint32_t i = last_count; i < current_count; i++) {
        uint32_t idx = (logger_state.current_index - (current_count - i)) % MAX_LOG_ENTRIES;
        const log_entry_t *entry = &logs[idx];
        
        if (entry->level < logger_state.min_level) {
            continue;
        }
        
        // Create just the new log entry
        lv_obj_t *log_label = lv_label_create(log_container);
        
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "[%s] %s: %s", 
                 entry->timestamp, 
                 log_level_names[entry->level],
                 entry->message);
        
        lv_label_set_text(log_label, buffer);
        lv_obj_set_style_text_color(log_label, log_level_colors[entry->level], 0);
    }
    
    // Update our counter
    last_count = current_count;
    last_index = logger_state.current_index;
    
    // Restore scroll position if at bottom (auto-follow)
    lv_coord_t max_scroll = lv_obj_get_scroll_bottom(log_container);
    if (scroll_y >= max_scroll - 10) {
        lv_obj_scroll_to_y(log_container, LV_COORD_MAX, LV_ANIM_OFF);
    }
}