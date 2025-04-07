/*******************************************************************
 *
 * ui.c - User interface implementation for the Camper GUI
 *
 ******************************************************************/
#include "ui.h"
#include "status_tab.h"
#include "logs_tab.h"
#include "analytics_tab.h"
#include "../lib/logger.h"
#include "lvgl/lvgl.h"
#include "../lib/lv_sdl_disp.h"
#include "../main.h"
#include "../data/data_manager.h"
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <SDL2/SDL.h>

// Add at the top with other static variables
static bool is_sleeping = false;
static lv_obj_t *sleep_overlay = NULL;
static lv_timer_t *inactivity_timer = NULL;
static lv_timer_t *update_timer = NULL;
#ifdef LV_CAMPER_DEBUG
static lv_timer_t *memory_monitor_timer = NULL;
#endif

extern SDL_Window *window;
extern SDL_Renderer *renderer;

// Forward declaration
static void on_wake_event(lv_event_t *e);
static void inactivity_timer_cb(lv_timer_t *timer);
static void data_update_timer_cb(lv_timer_t *timer);

/**
 * Turn display off using
 */
static int display_power_off(void) {
    log_debug("Turning off display");
    
    // Configure SDL for power saving
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    SDL_EnableScreenSaver();

    int rv = drm_blank_display(window, 1);
    if(rv != 0) {
        log_error("Error turning off display", rv);
    }
    return rv;
}

/**
 * Turn display on
 */
static void display_power_on(void) {
    log_debug("Turning on display");

    int rv = drm_blank_display(window, 0);
    if(rv != 0) {
        log_error("Error turning on display %d", rv);
    }

    // Restore power management
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "0");
    SDL_DisableScreenSaver();
}

/**
 * Enter sleep mode - turn off display
 */
void ui_enter_sleep_mode(void) {
    if (is_sleeping) return;  // Already in sleep mode
    
    log_info("Entering sleep mode");
    is_sleeping = true;
    
    // Delete previous overlay if it exists (prevents memory leaks)
    if (sleep_overlay != NULL) {
        lv_obj_del(sleep_overlay);
        sleep_overlay = NULL;
    }
    
    // Create a black overlay with "touch to wake" text
    sleep_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(sleep_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(sleep_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sleep_overlay, 255, 0);  // Fully opaque
    lv_obj_set_style_border_width(sleep_overlay, 0, 0);
    lv_obj_set_style_radius(sleep_overlay, 0, 0);
    
    // Add "touch to wake" text
    lv_obj_t *hint_label = lv_label_create(sleep_overlay);
    lv_label_set_text(hint_label, "Touch to wake");
    lv_obj_set_style_text_color(hint_label, lv_color_white(), 0);
    lv_obj_center(hint_label);
    
    // Add event callback to wake on touch
    lv_obj_add_event_cb(sleep_overlay, on_wake_event, LV_EVENT_PRESSED, NULL);
    
    // Force immediate redraw to show overlay before turning off display
    lv_refr_now(NULL);
    
    // Now turn off the display using DPMS
    if (display_power_off() != 0) {
        lv_label_set_text(hint_label, "Cannot turn off display using KMSDRM DPMS property");
    }
}

/**
 * Exit sleep mode - turn on display
 */
void ui_exit_sleep_mode(void) {
    if (inactivity_timer) {
        lv_timer_reset(inactivity_timer);
    }

    if (!is_sleeping) return;
    
    log_info("Exiting sleep mode");
    
    // Turn on the display
    display_power_on();
    
    // Remove the sleep overlay
    if (sleep_overlay) {
        lv_obj_del(sleep_overlay);
        sleep_overlay = NULL;
    }
    
    is_sleeping = false;
}

/**
 * Check if display is in sleep mode
 */
bool ui_is_sleeping(void) {
    return is_sleeping;
}

/**
 * Event handler for wake event
 */
static void on_wake_event(lv_event_t *e) {
    ui_exit_sleep_mode();
}

/**
 * Inactivity timer callback
 */
static void inactivity_timer_cb(lv_timer_t *timer) {
    if (!is_sleeping) {
        ui_enter_sleep_mode();
    }
}

/**
 * Reset the inactivity timer
 */
void ui_reset_inactivity_timer(void) {
    if (inactivity_timer) {
        lv_timer_reset(inactivity_timer);
    }
}

/**
 * Timer callback for data updates
 */
static void data_update_timer_cb(lv_timer_t *timer) {
    if (!ui_is_sleeping() && !is_background_busy()) {
        // Request a background fetch instead of blocking
        request_camper_data_fetch();
        
        // Update UI with latest data regardless of whether new data is being fetched
        update_status_ui(get_camper_data());
    }
}

#ifdef LV_CAMPER_DEBUG
/**
 * Timer callback to periodically monitor memory usage
 */
static void memory_monitor_timer_cb(lv_timer_t *timer) {
    static size_t last_used = 0;
    
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    
    size_t current_used = mon.total_size - mon.free_size;
    
    // Log if memory usage changed significantly
    if (abs((int)(current_used - last_used)) > MEM_CHANGE_THRESHOLD_BYTES) {
        log_debug("Memory usage changed: %+d bytes (%u bytes total used, %u%% fragmentation)",
                 (int)(current_used - last_used),
                 current_used, 
                 mon.frag_pct);
        last_used = current_used;
    }
}
#endif

/**
 * Print memory usage statistics for debugging
 */
void ui_print_memory_usage(void) {
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    
    log_info("Memory usage statistics:");
    log_info("  Total size: %u bytes", mon.total_size);
    log_info("  Free memory: %u bytes (%u%%)", mon.free_size, 100 - mon.used_pct);
    log_info("  Used memory: %u bytes (%u%%)", mon.total_size - mon.free_size, mon.used_pct);
    log_info("  Largest free block: %u bytes", mon.free_biggest_size);
    log_info("  Fragmentation: %u%%", mon.frag_pct);
}

/**
 * Cleanup UI resources - should be called before exiting the application
 */
void ui_cleanup(void) {
    log_debug("Cleaning up UI resources");
    
    // Shutdown the background fetcher
    shutdown_background_fetcher();
    
    // Delete timers
    if (inactivity_timer != NULL) {
        lv_timer_del(inactivity_timer);
        inactivity_timer = NULL;
    }
    
    if (update_timer != NULL) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    
#ifdef LV_CAMPER_DEBUG
    if (memory_monitor_timer != NULL) {
        lv_timer_del(memory_monitor_timer);
        memory_monitor_timer = NULL;
    }
    
    // Print final memory state for comparison
    ui_print_memory_usage();
#endif
    
    // Delete sleep overlay if it exists
    if (sleep_overlay != NULL) {
        lv_obj_del(sleep_overlay);
        sleep_overlay = NULL;
    }
}

// Update your sleep button handler
static void sleep_button_event_handler(lv_event_t *e) {
    if (is_sleeping) {
        ui_exit_sleep_mode();
    } else {
        ui_enter_sleep_mode();
    }
}

static void exit_button_event_handler(lv_event_t *e) {
    log_info("Exit button pressed, shutting down application");
    
    // Clean up UI resources
    ui_cleanup();
    
    // Exit the application
    exit(0);
}

/**
 * @brief Creates the main application UI
 * @description Creates a tabview with Status, Analytics, and Logs tabs
 */
void create_ui(void)
{
    // Create a tabview object
    lv_obj_t *tabview = lv_tabview_create(lv_screen_active());
    lv_obj_set_size(tabview, lv_pct(100), lv_pct(100));
    
    // Remove padding and gap between tabs and content
    lv_obj_set_style_pad_all(tabview, 0, 0);
    
    // Create tabs
    lv_obj_t *tab_status = lv_tabview_add_tab(tabview, "Status");
    lv_obj_t *tab_analytics = lv_tabview_add_tab(tabview, "Analytics");
    lv_obj_t *tab_logs = lv_tabview_add_tab(tabview, "Logs");
    
    // Get the tab bar (btns container)
    lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tabview);
    
    // Create a sleep button
    lv_obj_t *sleep_btn = lv_btn_create(tab_btns);
    lv_obj_set_width(sleep_btn, 50);
    lv_obj_set_height(sleep_btn, LV_PCT(100));
    lv_obj_align(sleep_btn, LV_ALIGN_RIGHT_MID, -60, 0); 
    lv_obj_set_style_radius(sleep_btn, 0, 0);
    lv_obj_set_style_bg_color(sleep_btn, lv_color_hex(0x3366CC), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(sleep_btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(sleep_btn, lv_color_hex(0x1A478F), LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t *sleep_label = lv_label_create(sleep_btn);
    lv_label_set_text(sleep_label, LV_SYMBOL_POWER); 
    lv_obj_center(sleep_label);

    // Add event handler for sleep button
    lv_obj_add_event_cb(sleep_btn, sleep_button_event_handler, LV_EVENT_CLICKED, NULL);

    // Create an exit button
    lv_obj_t *exit_btn = lv_btn_create(tab_btns);
    lv_obj_set_width(exit_btn, 50);
    lv_obj_set_height(exit_btn, LV_PCT(100));
    lv_obj_align(exit_btn, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_radius(exit_btn, 0, 0);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(exit_btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xCC0000), LV_PART_MAIN | LV_STATE_PRESSED);
    
    // Add exit icon
    lv_obj_t *exit_label = lv_label_create(exit_btn);
    lv_label_set_text(exit_label, LV_SYMBOL_CLOSE);
    lv_obj_center(exit_label);
    
    // Add event handler for exit button
    lv_obj_add_event_cb(exit_btn, exit_button_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_set_style_pad_all(tab_status, 0, 0);

    // Create a horizontal container to hold both columns
    lv_obj_t *columns_container = lv_obj_create(tab_status);
    lv_obj_set_size(columns_container, lv_pct(100), lv_pct(100));
    lv_obj_align(columns_container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(columns_container, 0, 0);
    lv_obj_set_style_radius(columns_container, 0, 0);
    lv_obj_set_style_pad_all(columns_container, 0, 0);
    
    // Use a row layout for the columns container
    lv_obj_set_flex_flow(columns_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(columns_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Create left column (50% width)
    lv_obj_t *left_column = lv_obj_create(columns_container);
    lv_obj_set_size(left_column, lv_pct(50), lv_pct(100));
    lv_obj_set_style_border_width(left_column, 0, 0);
    lv_obj_set_style_radius(left_column, 0, 0);
    lv_obj_set_style_pad_all(left_column, 10, 0);
    
    // Create right column (50% width) - empty for now
    lv_obj_t *right_column = lv_obj_create(columns_container);
    lv_obj_set_size(right_column, lv_pct(50), lv_pct(100));
    lv_obj_set_style_border_width(right_column, 0, 0);
    lv_obj_set_style_radius(right_column, 0, 0);
    lv_obj_set_style_pad_all(right_column, 5, 0);
    
    // Set up right column as a vertical container (for future content)
    lv_obj_set_flex_flow(right_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right_column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(right_column, 8, 0);
    
    // Add a placeholder label to the right column
    lv_obj_t *placeholder = lv_label_create(right_column);
    lv_label_set_text(placeholder, "Future Content Area");
    lv_obj_set_style_text_color(placeholder, lv_color_hex(0x888888), 0); // Light gray text
    lv_obj_align(placeholder, LV_ALIGN_CENTER, 0, 0);

    // Create content for each tab
    create_status_tab(left_column);
    create_analytics_tab(tab_analytics);
    create_logs_tab(tab_logs);

    // Create inactivity timer to automatically enter sleep mode
    inactivity_timer = lv_timer_create(inactivity_timer_cb, DISPLAY_INACTIVITY_TIMEOUT_MS, NULL);
    update_timer = lv_timer_create(data_update_timer_cb, DATA_UPDATE_INTERVAL_MS, NULL);
    
#ifdef LV_CAMPER_DEBUG
    // Create memory monitoring timer
    memory_monitor_timer = lv_timer_create(memory_monitor_timer_cb, MEM_MONITOR_INTERVAL_MS, NULL);
    
    // Print initial memory state
    ui_print_memory_usage();
#endif
}
