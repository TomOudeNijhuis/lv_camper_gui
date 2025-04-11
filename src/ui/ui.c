/*******************************************************************
 *
 * ui.c - User interface implementation for the Camper GUI
 *
 ******************************************************************/
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <SDL2/SDL.h>

#include "ui.h"
#include "status_column.h"
#include "logs_tab.h"
#include "analytics_tab.h"
#include "energy_temp_panel.h"  // Add this new include
#include "../lib/logger.h"
#include "lvgl/lvgl.h"
#include "../lib/lv_sdl_disp.h"
#include "../main.h"
#include "../data/data_manager.h"
#include "../lib/mem_debug.h"
#include "lv_awesome_16.h"


// Add at the top with other static variables
static bool is_sleeping = false;
static lv_obj_t *sleep_overlay = NULL;
static lv_timer_t *inactivity_timer = NULL;
static bool is_night_mode = false;
#ifdef LV_CAMPER_DEBUG
static lv_timer_t *memory_monitor_timer = NULL;
static lv_timer_t *leak_check_timer = NULL;
#endif

extern SDL_Window *window;
extern SDL_Renderer *renderer;

// Forward declaration
static void on_wake_event(lv_event_t *e);
static void inactivity_timer_cb(lv_timer_t *timer);
static void brightness_button_event_handler(lv_event_t *e);  // New handler declaration
static void exit_timer_cb(lv_timer_t *timer);  // Forward declaration of the new exit timer callback
static void apply_style_recursive(lv_obj_t *obj, lv_style_t *text_style, lv_style_t *card_style, bool dark_mode);

#ifdef LV_CAMPER_DEBUG
/**
 * Callback for memory leak checking
 */
static void leak_check_timer_cb(lv_timer_t *timer) {
    // Print current memory usage statistics
    ui_print_memory_usage();
    mem_debug_print_stats();
    
    // Check for potential memory leaks
    int leaks = mem_debug_check_leaks();
    if (leaks > 0) {
        log_warning("Potential memory leaks detected: %d blocks not properly freed", leaks);
    }
}

/**
 * Timer callback to periodically monitor memory usage
 */
static void memory_monitor_timer_cb(lv_timer_t *timer) {
    static size_t last_used = 0;
    
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    
    size_t current_used = mon.total_size - mon.free_size;
    
    // Log if memory usage changed significantly
    if (labs((long)(current_used - last_used)) > MEM_CHANGE_THRESHOLD_BYTES) {
        log_debug("LVGL memory usage changed: %+ld bytes (%u bytes total used, %u%% fragmentation)",
                 (long)(current_used - last_used),
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
    
    log_info("LVGL memory usage statistics:");
    log_info("  Total size: %u bytes", mon.total_size);
    log_info("  Free memory: %u bytes (%u%%)", mon.free_size, 100 - mon.used_pct);
    log_info("  Used memory: %u bytes (%u%%)", mon.total_size - mon.free_size, mon.used_pct);
    log_info("  Largest free block: %u bytes", mon.free_biggest_size);
    log_info("  Fragmentation: %u%%", mon.frag_pct);
}

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
 * Apply dark or light theme to the UI
 */
static void apply_theme_mode(bool dark_mode) {
    // Get the current theme
    lv_theme_t *theme = lv_theme_get_from_obj(lv_screen_active());
    
    // Set color scheme based on mode
    lv_color_t primary_color = dark_mode ? lv_color_hex(0x3050C8) : lv_color_hex(0x3078FF);
    lv_color_t bg_color = dark_mode ? lv_color_hex(0x202020) : lv_color_hex(0xEEEEEE);
    lv_color_t text_color = dark_mode ? lv_color_hex(0xDDDDDD) : lv_color_hex(0x333333);
    lv_color_t card_bg_color = dark_mode ? lv_color_hex(0x303030) : lv_color_hex(0xFFFFFF);
    
    // Apply colors to screen
    lv_obj_set_style_bg_color(lv_screen_active(), bg_color, 0);
    
    // Create a style for text
    static lv_style_t style_text;
    lv_style_init(&style_text);
    lv_style_set_text_color(&style_text, text_color);
    
    // Create a style for cards/containers
    static lv_style_t style_card;
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, card_bg_color);
    lv_style_set_border_color(&style_card, dark_mode ? 
                              lv_color_hex(0x404040) : lv_color_hex(0xDDDDDD));
    
    // Apply the style to all children using the public API
    uint32_t child_count = lv_obj_get_child_count(lv_screen_active());
    for(uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(lv_screen_active(), i);
        if(child == NULL) continue;
        
        // Skip non-visible children
        if(lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;
        
        // Apply text style to all children recursively
        apply_style_recursive(child, &style_text, &style_card, dark_mode);
    }
    
    log_info("Applied %s mode theme", dark_mode ? "dark" : "light");
}

/**
 * Recursively apply styles to an object and its children
 */
static void apply_style_recursive(lv_obj_t *obj, lv_style_t *text_style, 
                                 lv_style_t *card_style, bool dark_mode) {
    // Apply card style to containers
    if(lv_obj_check_type(obj, &lv_obj_class) || 
       lv_obj_check_type(obj, &lv_tabview_class)) {
        lv_obj_add_style(obj, card_style, 0);
    }
    
    // Apply text style to labels
    if(lv_obj_check_type(obj, &lv_label_class)) {
        lv_obj_add_style(obj, text_style, 0);
    }

    // Handle gauge widgets (if available in your LVGL version)
    if(lv_obj_has_class(obj, &lv_chart_class) || 
       lv_obj_has_class(obj, &lv_bar_class) ||
       lv_obj_has_class(obj, &lv_scale_class)) {
        
        lv_color_t indicator_color = dark_mode ? lv_color_white() : lv_color_black();
        
        // Apply colors to indicators
        lv_obj_set_style_line_color(obj, indicator_color, LV_PART_INDICATOR);
        lv_obj_set_style_line_color(obj, indicator_color, LV_PART_ITEMS);
        
        // Set text color for any labels within the chart/gauge
        lv_obj_set_style_text_color(obj, indicator_color, LV_PART_ITEMS);

        lv_obj_set_style_text_color(obj, indicator_color, LV_PART_MAIN);
     
    }
    
    // Handle sliders that might use scales
    if(lv_obj_check_type(obj, &lv_slider_class) || lv_obj_check_type(obj, &lv_bar_class)) {
        lv_color_t indicator_color = dark_mode ? lv_color_white() : lv_color_black();
        lv_obj_set_style_border_color(obj, indicator_color, LV_PART_INDICATOR);
        lv_obj_set_style_border_color(obj, indicator_color, LV_PART_KNOB);
        
        // Set tick color if slider has them
        lv_obj_set_style_line_color(obj, indicator_color, LV_PART_ITEMS);
    }

    // Apply to children recursively
    uint32_t i;
    for(i = 0; i < lv_obj_get_child_count(obj); i++) {
        apply_style_recursive(lv_obj_get_child(obj, i), text_style, card_style, dark_mode);
    }
}

/**
 * Handles toggling between day and night mode
 */
static void brightness_button_event_handler(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    
    // Toggle the mode
    is_night_mode = !is_night_mode;
       
    // Update the button icon
    if (is_night_mode) {
        lv_label_set_text(label, LV_SYMBOL_MOON);
    } else {
        lv_label_set_text(label, LV_SYMBOL_SUN);
    }

    apply_theme_mode(is_night_mode);
}

/**
 * Cleanup UI resources - should be called before exiting the application
 */
void ui_cleanup(void) {
    log_debug("Cleaning up UI resources");
    
    // Shutdown the background fetcher
    shutdown_background_fetcher();
    
    status_column_cleanup();
    energy_temp_panel_cleanup();  // Add cleanup for energy panel

    // Delete timers
    if (inactivity_timer != NULL) {
        lv_timer_del(inactivity_timer);
        inactivity_timer = NULL;
    }
    
    // Delete sleep overlay if it exists
    if (sleep_overlay != NULL) {
        lv_obj_del(sleep_overlay);
        sleep_overlay = NULL;
    }

    #ifdef LV_CAMPER_DEBUG
    // Delete memory monitoring timer if it exists
    if (memory_monitor_timer != NULL) {
        lv_timer_del(memory_monitor_timer);
        memory_monitor_timer = NULL;
    }

    if (leak_check_timer != NULL) {
        lv_timer_del(leak_check_timer);
        leak_check_timer = NULL;
    }
    
    // Check for memory leaks
    mem_debug_check_leaks();
    
    // Print final memory state for comparison
    ui_print_memory_usage();
    mem_debug_print_stats();

    #endif
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
    log_info("Exit button pressed, showing shutdown popup");
    
    // Create a popup message
    lv_obj_t *popup = lv_obj_create(lv_screen_active());
    lv_obj_set_size(popup, LV_PCT(50), LV_PCT(30));
    lv_obj_center(popup);
    lv_obj_set_style_bg_color(popup, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_color(popup, lv_color_hex(0x666666), 0);
    lv_obj_set_style_border_width(popup, 2, 0);
    lv_obj_set_style_radius(popup, 10, 0);
    lv_obj_set_style_shadow_width(popup, 20, 0);
    lv_obj_set_style_shadow_opa(popup, LV_OPA_50, 0);
    
    // Add a label with the shutdown message
    lv_obj_t *label = lv_label_create(popup);
    lv_label_set_text(label, "Stopping background workers...");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 30);
    
    // Create a spinner to show activity
    lv_obj_t *spinner = lv_spinner_create(popup);
    lv_obj_set_size(spinner, 50, 50);
    lv_obj_align(spinner, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    // Force immediate redraw to show popup
    lv_refr_now(NULL);
    
    // Create a timer to exit after a short delay (1500ms)
    lv_timer_t *exit_timer = lv_timer_create(exit_timer_cb, 1500, NULL);
    
    // Store the popup reference in the timer user data
    lv_timer_set_user_data(exit_timer, popup);
}

/**
 * Timer callback to exit the application after showing the popup
 */
static void exit_timer_cb(lv_timer_t *timer) {
    // Delete the popup
    lv_obj_t *popup = (lv_obj_t *)lv_timer_get_user_data(timer);
    if (popup) {
        lv_obj_del(popup);
    }
    
    // Clean up UI resources
    ui_cleanup();
    
    // Exit the application
    log_info("Exiting application");
    exit(0);
}

/**
 * @brief Creates the main application UI
 * @description Creates a tabview with Status, Analytics, and Logs tabs
 */
void create_ui(void)
{
#ifdef LV_CAMPER_DEBUG
    // Initialize memory debugging
    mem_debug_init();
#endif

    // Create a tabview object
    lv_obj_t *tabview = lv_tabview_create(lv_screen_active());
    lv_obj_set_size(tabview, lv_pct(100), lv_pct(100));

    // Remove padding and gap between tabs and content
    lv_obj_set_style_pad_all(tabview, 0, 0);
    
    // Create tabs
    lv_obj_t *tab_status = lv_tabview_add_tab(tabview, "Status"); 
    // lv_obj_t *tab_analytics = lv_tabview_add_tab(tabview, "Analytics");
    lv_obj_t *tab_logs = lv_tabview_add_tab(tabview, "Logs");

    // Get the tab bar (btns container)
    lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tabview);
    
    // Create brightness toggle button (day/night mode)
    lv_obj_t *brightness_btn = lv_btn_create(tab_btns);
    lv_obj_set_width(brightness_btn, 50);
    lv_obj_set_height(brightness_btn, LV_PCT(100));
    lv_obj_align(brightness_btn, LV_ALIGN_RIGHT_MID, -115, 0);  // Position left of sleep button
    lv_obj_set_style_radius(brightness_btn, 0, 0);
    lv_obj_set_style_bg_color(brightness_btn, lv_color_hex(0xFFA500), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(brightness_btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(brightness_btn, lv_color_hex(0xE67300), LV_PART_MAIN | LV_STATE_PRESSED);
    
    // Add brightness icon (sun for day mode, which is default)
    lv_obj_t *brightness_label = lv_label_create(brightness_btn);
    lv_obj_set_style_text_font(brightness_label, &lv_awesome_16, 0);  // Use the custom font
    lv_label_set_text(brightness_label, LV_SYMBOL_SUN);
    lv_obj_center(brightness_label);
    
    // Add event handler for brightness button
    lv_obj_add_event_cb(brightness_btn, brightness_button_event_handler, LV_EVENT_CLICKED, NULL);
    
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
    
    // Create content for each tab
    create_status_column(left_column);    
    create_energy_temp_panel(right_column);
    // create_analytics_tab(tab_analytics);
    create_logs_tab(tab_logs);

    // Create inactivity timer to automatically enter sleep mode
    inactivity_timer = lv_timer_create(inactivity_timer_cb, DISPLAY_INACTIVITY_TIMEOUT_MS, NULL);
    
#ifdef LV_CAMPER_DEBUG
    // Create memory monitoring timer
    memory_monitor_timer = lv_timer_create(memory_monitor_timer_cb, MEM_MONITOR_INTERVAL_MS, NULL);
    
    // Create memory leak check timer (runs less frequently to minimize overhead)
    leak_check_timer = lv_timer_create(leak_check_timer_cb, MEM_MONITOR_INTERVAL_MS * 5, NULL);
#endif
}
