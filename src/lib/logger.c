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
    static uint32_t last_update_count = 0;
    static uint32_t last_update_index = 0;
    static lv_obj_t *log_labels[MAX_LOG_ENTRIES] = {NULL};
    
    // Get current log count
    uint32_t current_count;
    const log_entry_t *logs = logger_get_logs(&current_count);
    
    // Check if there are new logs to display - check both count and index
    if (current_count == last_update_count && logger_state.current_index == last_update_index) {
        return;
    }

    // Store scroll position and check if we're at the bottom
    lv_coord_t scroll_y = lv_obj_get_scroll_y(log_container);
    lv_coord_t max_scroll = lv_obj_get_scroll_bottom(log_container);
    bool was_at_bottom = (scroll_y >= max_scroll - 10);
    
    // Case 1: Buffer was not full but is now
    if (last_update_count < MAX_LOG_ENTRIES && current_count >= MAX_LOG_ENTRIES) {
        // Need to rebuild the display - clean existing labels and array
        lv_obj_clean(log_container);
        memset(log_labels, 0, sizeof(log_labels));
        
        // Rebuild from oldest entry
        uint32_t start_idx = logger_state.current_index;
        for (uint32_t i = 0; i < MAX_LOG_ENTRIES; i++) {
            uint32_t idx = (start_idx + i) % MAX_LOG_ENTRIES;
            const log_entry_t *entry = &logs[idx];
            
            if (entry->level >= logger_state.min_level) {
                lv_obj_t *log_label = lv_label_create(log_container);
                
                char buffer[512];
                snprintf(buffer, sizeof(buffer), "[%s] %s: %s", 
                        entry->timestamp, 
                        log_level_names[entry->level],
                        entry->message);
                
                lv_label_set_text(log_label, buffer);
                lv_obj_set_style_text_color(log_label, log_level_colors[entry->level], 0);
                
                // Store reference to this label
                log_labels[idx] = log_label;
            }
        }
    }
    // Case 2: Initial population
    else if (last_update_count == 0 && current_count > 0) {
        // First population, add all entries
        for (uint32_t i = 0; i < current_count; i++) {
            const log_entry_t *entry = &logs[i];
            if (entry->level >= logger_state.min_level) {
                lv_obj_t *log_label = lv_label_create(log_container);
                
                char buffer[512];
                snprintf(buffer, sizeof(buffer), "[%s] %s: %s", 
                        entry->timestamp, 
                        log_level_names[entry->level],
                        entry->message);
                
                lv_label_set_text(log_label, buffer);
                lv_obj_set_style_text_color(log_label, log_level_colors[entry->level], 0);
                
                // Store reference to this label
                log_labels[i] = log_label;
            }
        }
    }
    // Case 3: Normal incremental updates
    else {
        // If buffer isn't full, just add new entries
        if (current_count < MAX_LOG_ENTRIES) {
            for (uint32_t i = last_update_count; i < current_count; i++) {
                const log_entry_t *entry = &logs[i];
                if (entry->level >= logger_state.min_level) {
                    lv_obj_t *log_label = lv_label_create(log_container);
                    
                    char buffer[512];
                    snprintf(buffer, sizeof(buffer), "[%s] %s: %s", 
                            entry->timestamp, 
                            log_level_names[entry->level],
                            entry->message);
                    
                    lv_label_set_text(log_label, buffer);
                    lv_obj_set_style_text_color(log_label, log_level_colors[entry->level], 0);
                    
                    // Store reference to this label
                    log_labels[i] = log_label;
                }
            }
        }
        // Buffer is full - find which entries were overwritten
        else {
            uint32_t num_new_entries = (logger_state.current_index + MAX_LOG_ENTRIES - last_update_index) % MAX_LOG_ENTRIES;
            
            for (uint32_t i = 0; i < num_new_entries; i++) {
                // Calculate the index of overwritten entry
                uint32_t overwritten_idx = (last_update_index + i) % MAX_LOG_ENTRIES;
                
                // Delete the UI element for the overwritten entry if it exists
                if (log_labels[overwritten_idx] != NULL) {
                    lv_obj_del(log_labels[overwritten_idx]);
                    log_labels[overwritten_idx] = NULL;
                }
                
                // Now add the new entry that replaced it
                const log_entry_t *entry = &logs[overwritten_idx];
                if (entry->level >= logger_state.min_level) {
                    lv_obj_t *log_label = lv_label_create(log_container);
                    
                    char buffer[512];
                    snprintf(buffer, sizeof(buffer), "[%s] %s: %s", 
                            entry->timestamp, 
                            log_level_names[entry->level],
                            entry->message);
                    
                    lv_label_set_text(log_label, buffer);
                    lv_obj_set_style_text_color(log_label, log_level_colors[entry->level], 0);
                    
                    // Store reference to this label
                    log_labels[overwritten_idx] = log_label;
                }
            }
        }
    }
    
    // Update our counters
    last_update_count = current_count;
    last_update_index = logger_state.current_index;
    
    // Auto-scroll to bottom if we were already there
    if (was_at_bottom) {
        lv_obj_scroll_to_y(log_container, LV_COORD_MAX, LV_ANIM_ON);
    }
}