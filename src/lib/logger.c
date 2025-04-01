#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "logger.h"
#include "lvgl/lvgl.h"

#define MAX_LOG_ENTRIES 100  // Circular buffer size

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
    logger_state.min_level = LOG_LEVEL_INFO;

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
        log_level_names[level],  // Changed from level_strings
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

// Timer callback to restore scroll position
static void restore_scroll_cb(lv_timer_t *timer) {
    // Use proper LVGL API to get user data instead of direct struct access
    scroll_restore_data_t *data = (scroll_restore_data_t *)lv_timer_get_user_data(timer);
    
    // Restore the scroll position
    lv_obj_scroll_to_y(data->container, data->scroll_y, LV_ANIM_OFF);
    
    // Free the data structure and delete the timer
    free(data);
    lv_timer_del(timer);
}

/**
 * Update the logs display in the UI
 */
void logger_update_ui(lv_obj_t *log_container) {
    // Store current scroll position
    lv_coord_t scroll_y = lv_obj_get_scroll_y(log_container);
    
    // Temporarily hide the container during updates to prevent flicker
    lv_obj_add_flag(log_container, LV_OBJ_FLAG_HIDDEN);
    
    // Clear current content
    lv_obj_clean(log_container);
    
    // Get logs
    uint32_t count;
    const log_entry_t *logs = logger_get_logs(&count);
    
    // Calculate starting index to display logs in chronological order
    uint32_t start_idx = 0;
    if (count == MAX_LOG_ENTRIES) {
        start_idx = logger_state.current_index;
    }
    
    // Add log entries to the container
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (start_idx + i) % MAX_LOG_ENTRIES;
        const log_entry_t *entry = &logs[idx];
        
        if (entry->level < logger_state.min_level) {
            continue;
        }
        
        // Create the log entry label
        lv_obj_t *log_label = lv_label_create(log_container);
        
        // Format: [TIME] LEVEL: Message
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "[%s] %s: %s", 
                 entry->timestamp, 
                 log_level_names[entry->level],
                 entry->message);
        
        lv_label_set_text(log_label, buffer);
        lv_obj_set_style_text_color(log_label, log_level_colors[entry->level], 0);
    }
    
    // Restore scroll position immediately - no need for timer
    lv_obj_scroll_to_y(log_container, scroll_y, LV_ANIM_OFF);
    
    // Make container visible again after update is complete
    lv_obj_clear_flag(log_container, LV_OBJ_FLAG_HIDDEN);
}