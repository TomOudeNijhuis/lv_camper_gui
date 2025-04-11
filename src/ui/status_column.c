/*******************************************************************
 *
 * status_column.c - Status tab UI implementation for the Camper GUI
 *
 ******************************************************************/
#include "status_column.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../lib/logger.h"
#include "../lib/http_client.h"
#include "lvgl/lvgl.h"
#include "../data/sensor_types.h"
#include "../data/data_manager.h"
#include "../main.h"
#include "ui.h"

static lv_obj_t *ui_household_switch = NULL;
static lv_obj_t *ui_pump_switch = NULL;
static lv_obj_t *ui_mains_led = NULL;
static lv_obj_t *ui_water_bar = NULL;
static lv_obj_t *ui_waste_bar = NULL;
static lv_obj_t *ui_starter_battery = NULL;
static lv_obj_t *ui_starter_battery_needle = NULL;
static lv_obj_t *ui_household_battery = NULL;
static lv_obj_t *ui_household_battery_needle = NULL;
static lv_timer_t *update_timer = NULL;

void update_status_ui(camper_sensor_t *camper_data);

static void household_event_handler(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);

    const char *status = checked ? "ON" : "OFF";
    log_info("Household switch changed to: %s", status);
    
    // Request background action instead of blocking UI
    request_camper_action("household_state", status);
}

static void pump_event_handler(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);

    const char *status = checked ? "ON" : "OFF";
    log_info("Pump switch changed to: %s", status);
    
    // Request background action instead of blocking UI
    request_camper_action("pump_state", status);
}

static void update_battery_gauge(lv_obj_t *scale_line, lv_obj_t *needle_line, float voltage) {
    lv_obj_t *voltage_label = lv_obj_get_user_data(scale_line);

    // Hide the needle when voltage is 0 (invalid/missing reading)
    if (voltage <= 1.0f) {
        lv_obj_add_flag(needle_line, LV_OBJ_FLAG_HIDDEN);

        if(voltage_label) {
            lv_label_set_text(voltage_label, "-----");
        }
        return;
    } else {
        lv_obj_clear_flag(needle_line, LV_OBJ_FLAG_HIDDEN);
    }

    // Set the needle to show the current voltage
    lv_scale_set_line_needle_value(scale_line, needle_line, 60, voltage*10);

    // Update the needle and label color based on the voltage level
    if (voltage < 11.0) {
        // Critical - red
        lv_obj_set_style_line_color(needle_line, lv_color_hex(0xFF0000), 0);
    } else if (voltage < 11.8) {
        // Warning - orange
        lv_obj_set_style_line_color(needle_line, lv_color_hex(0xFF8000), 0);
    } else {
        // Good - green
        lv_obj_set_style_line_color(needle_line, lv_color_hex(0x00C853), 0);
    }

    if(voltage_label) {
        // Update the text to show current voltage
        lv_label_set_text_fmt(voltage_label, "%.1fV", voltage);
    }
}

/**
 * Timer callback for data updates
 */
static void data_update_timer_cb(lv_timer_t *timer) {
    if (!ui_is_sleeping()) {
        // Request a background fetch instead of blocking
        bool result = request_data_fetch(FETCH_CAMPER_DATA);
        if (!result) {
            log_warning("Failed to request data fetch");
        }
        
        // Update UI with latest data regardless of whether new data is being fetched
        update_status_ui(get_camper_data());
    }
}

static lv_obj_t* create_battery_gauge(lv_obj_t *parent, const char *title, float voltage, lv_obj_t **out_needle) {
    // Create a container for everything
    lv_obj_t *gauge_container = lv_obj_create(parent);
    lv_obj_set_size(gauge_container, lv_pct(48), 180);
    lv_obj_set_style_bg_opa(gauge_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gauge_container, 0, 0);
    lv_obj_set_scrollbar_mode(gauge_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(gauge_container, LV_OBJ_FLAG_SCROLLABLE);

    // Use absolute positioning in this container
    lv_obj_set_layout(gauge_container, LV_LAYOUT_NONE);

    // Add a title label near the top
    lv_obj_t *title_label = lv_label_create(gauge_container);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, -10);

    // Create the scale
    lv_obj_t *scale_line = lv_scale_create(gauge_container);
    lv_obj_set_size(scale_line, 130, 130);
    lv_obj_align(scale_line, LV_ALIGN_CENTER, 0, 10);

    // Configure the scale (as before)
    lv_scale_set_mode(scale_line, LV_SCALE_MODE_ROUND_INNER);
    lv_obj_set_style_bg_opa(scale_line, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scale_line, 0, 0);

    // Configure ticks and labels
    lv_obj_set_style_transform_rotation(scale_line, 450, LV_PART_INDICATOR);
    lv_obj_set_style_translate_x(scale_line, 10, LV_PART_INDICATOR);
    lv_obj_set_style_length(scale_line, 15, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(scale_line, 5, LV_PART_INDICATOR);
    lv_obj_set_style_length(scale_line, 10, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(scale_line, 5, LV_PART_ITEMS);
    lv_obj_set_style_line_opa(scale_line, LV_OPA_50, LV_PART_ITEMS);
    lv_scale_set_label_show(scale_line, true);
    lv_scale_set_total_tick_count(scale_line, 11);
    lv_scale_set_major_tick_every(scale_line, 2);
    lv_scale_set_range(scale_line, 90, 140);
    static const char *custom_labels[] = {"9", "10", "11", "12", "13", "14", NULL};
    lv_scale_set_text_src(scale_line, custom_labels);
    lv_scale_set_angle_range(scale_line, 270);
    lv_scale_set_rotation(scale_line, 135);

    // Create styles & sections (same as before)
    static lv_style_t style_section_red;
    static lv_style_t style_section_orange;
    static lv_style_t style_section_green;
    static bool styles_initialized = false;

    if (!styles_initialized) {
        lv_style_init(&style_section_red);
        lv_style_set_arc_color(&style_section_red, lv_color_hex(0xFF0000));
        lv_style_set_arc_width(&style_section_red, 3);

        lv_style_init(&style_section_orange);
        lv_style_set_arc_color(&style_section_orange, lv_color_hex(0xFF8000));
        lv_style_set_arc_width(&style_section_orange, 3);

        lv_style_init(&style_section_green);
        lv_style_set_arc_color(&style_section_green, lv_color_hex(0x00C853));
        lv_style_set_arc_width(&style_section_green, 3);

        styles_initialized = true;
    }

    lv_scale_section_t *section1 = lv_scale_add_section(scale_line);
    lv_scale_section_set_range(section1, 90, 110);
    lv_scale_section_set_style(section1, LV_PART_MAIN, &style_section_red);

    lv_scale_section_t *section2 = lv_scale_add_section(scale_line);
    lv_scale_section_set_range(section2, 110, 118);
    lv_scale_section_set_style(section2, LV_PART_MAIN, &style_section_orange);

    lv_scale_section_t *section3 = lv_scale_add_section(scale_line);
    lv_scale_section_set_range(section3, 118, 140);
    lv_scale_section_set_style(section3, LV_PART_MAIN, &style_section_green);

    lv_obj_set_style_arc_rounded(scale_line, true, LV_PART_MAIN);

    // Create the needle
    lv_obj_t *needle_line = lv_line_create(scale_line);
    lv_obj_set_style_line_width(needle_line, 3, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(needle_line, true, LV_PART_MAIN);
    *out_needle = needle_line;

    // Create the voltage label
    lv_obj_t *voltage_label = lv_label_create(gauge_container);
    lv_label_set_text_fmt(voltage_label, "%.1fV", voltage);
    lv_obj_set_style_text_font(voltage_label, &lv_font_montserrat_16, 0);

    // Align the voltage label so it appears on the missing portion (top) of the arc
    // Adjust the final (y) offset as needed
    lv_obj_align_to(voltage_label, scale_line, LV_ALIGN_CENTER, 0, 55);

    // Store the label pointer so we can update it later
    lv_obj_set_user_data(scale_line, voltage_label);

    // Initialize the gauge to the correct voltage
    update_battery_gauge(scale_line, needle_line, voltage);

    return scale_line;
}

static lv_obj_t* create_level_bar(lv_obj_t *parent, const char *label_text, int initial_value, lv_color_t color) {
    // Create container with padding
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, lv_pct(100), LV_SIZE_CONTENT); 
    lv_obj_set_style_pad_top(container, 15, 0);
    lv_obj_set_style_pad_bottom(container, 15, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Add label
    lv_obj_t *label = lv_label_create(container);
    lv_label_set_text(label, label_text);
    
    // Create the bar
    lv_obj_t *bar = lv_bar_create(container);
    lv_obj_set_size(bar, lv_pct(100), 30);  
    lv_bar_set_range(bar, 0, 100); 
    
    // Set bar styles directly with the provided color
    lv_obj_set_style_border_color(bar, color, 0);
    lv_obj_set_style_border_width(bar, 2, 0);
    lv_obj_set_style_pad_all(bar, 6, 0);
    lv_obj_set_style_radius(bar, 6, 0);

    lv_obj_set_style_bg_color(bar, lv_color_hex(0xE0E0E0), 0); // Light gray background
    
    // Set indicator style with the provided color
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
    
    // Set initial value
    lv_bar_set_value(bar, initial_value, LV_ANIM_OFF);

    return bar;
}

void create_status_column(lv_obj_t *left_column)
{
    // Set up left column as a vertical container
    lv_obj_set_flex_flow(left_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(left_column, 8, 0);

    // Create a container for the household switch, pump switch, and mains LED
    lv_obj_t *status_row_container = lv_obj_create(left_column);
    lv_obj_set_size(status_row_container, lv_pct(100), 80);
    lv_obj_set_style_border_width(status_row_container, 0, 0);
    lv_obj_set_style_pad_all(status_row_container, 5, 0);
    lv_obj_set_flex_flow(status_row_container, LV_FLEX_FLOW_ROW); 
    lv_obj_set_flex_align(status_row_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Household switch with label
    lv_obj_t *household_container = lv_obj_create(status_row_container);
    lv_obj_set_size(household_container, lv_pct(30), lv_pct(100));
    lv_obj_set_style_border_width(household_container, 0, 0);
    lv_obj_set_style_pad_all(household_container, 5, 0);
    lv_obj_set_flex_flow(household_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(household_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *household_label = lv_label_create(household_container);
    lv_label_set_text(household_label, "Household");
    lv_obj_set_style_text_font(household_label, &lv_font_montserrat_16, 0);

    lv_obj_t *household_switch = lv_switch_create(household_container);
    lv_obj_set_style_bg_color(household_switch, lv_color_hex(0x008800), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(household_switch, household_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    // Pump switch with label
    lv_obj_t *pump_container = lv_obj_create(status_row_container);
    lv_obj_set_size(pump_container, lv_pct(30), lv_pct(100));
    lv_obj_set_style_border_width(pump_container, 0, 0);
    lv_obj_set_style_pad_all(pump_container, 5, 0);
    lv_obj_set_flex_flow(pump_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(pump_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *pump_label = lv_label_create(pump_container);
    lv_label_set_text(pump_label, "Pump");
    lv_obj_set_style_text_font(pump_label, &lv_font_montserrat_16, 0);

    lv_obj_t *pump_switch = lv_switch_create(pump_container);
    lv_obj_set_style_bg_color(pump_switch, lv_color_hex(0x008800), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(pump_switch, pump_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    // Mains LED with label
    lv_obj_t *mains_container = lv_obj_create(status_row_container);
    lv_obj_set_size(mains_container, lv_pct(30), lv_pct(100)); 
    lv_obj_set_style_border_width(mains_container, 0, 0);
    lv_obj_set_style_pad_all(mains_container, 5, 0);
    lv_obj_set_flex_flow(mains_container, LV_FLEX_FLOW_COLUMN); 
    lv_obj_set_flex_align(mains_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *mains_label = lv_label_create(mains_container);
    lv_label_set_text(mains_label, "Mains");
    lv_obj_set_style_text_font(mains_label, &lv_font_montserrat_16, 0);

    lv_obj_t *mains_led = lv_led_create(mains_container);
    lv_obj_set_size(mains_led, 20, 20);
    lv_led_set_color(mains_led, lv_color_hex(0x808080)); // Gray for unknown status
    lv_led_set_brightness(mains_led, 255);
    lv_led_off(mains_led);
    
    // Create water and waste bars with appropriate colors
    lv_obj_t *water_bar = create_level_bar(left_column, "Fresh Water", 0, lv_palette_main(LV_PALETTE_BLUE));
    lv_obj_t *waste_bar = create_level_bar(left_column, "Waste Water", 0, lv_palette_main(LV_PALETTE_ORANGE));
 
    // Create a container for voltage info with row layout
    lv_obj_t *voltage_container = lv_obj_create(left_column);
    lv_obj_set_size(voltage_container, lv_pct(100), 180);
    lv_obj_set_style_pad_all(voltage_container, 3, 0);
    lv_obj_set_style_border_width(voltage_container, 0, 0);
    lv_obj_set_scrollbar_mode(voltage_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(voltage_container, LV_OBJ_FLAG_SCROLLABLE);

    // Use a row layout for voltages - this places containers side by side
    lv_obj_set_flex_flow(voltage_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(voltage_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui_household_switch = household_switch;
    ui_pump_switch = pump_switch;
    ui_mains_led = mains_led;
    ui_water_bar = water_bar;
    ui_waste_bar = waste_bar;
    
    // Store references to battery gauges
    ui_starter_battery = create_battery_gauge(voltage_container, "Starter Voltage", 0, &ui_starter_battery_needle);
    ui_household_battery = create_battery_gauge(voltage_container, "Household Voltage", 0, &ui_household_battery_needle);

    update_timer = lv_timer_create(data_update_timer_cb, DATA_UPDATE_INTERVAL_MS, NULL);
}

void status_column_cleanup(void) {
    if (update_timer != NULL) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
}

void update_status_ui(camper_sensor_t *camper_data)
{
    // Update household switch
    if (ui_household_switch) {
        bool current_state = lv_obj_has_state(ui_household_switch, LV_STATE_CHECKED);
        if (current_state != camper_data->household_state) {
            if (camper_data->household_state) {
                lv_obj_add_state(ui_household_switch, LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(ui_household_switch, LV_STATE_CHECKED);
            }
        }
    }

    // Update pump switch
    if (ui_pump_switch) {
        bool current_state = lv_obj_has_state(ui_pump_switch, LV_STATE_CHECKED);
        if (current_state != camper_data->pump_state) {
            if (camper_data->pump_state) {
                lv_obj_add_state(ui_pump_switch, LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(ui_pump_switch, LV_STATE_CHECKED);
            }
        }
    }

    // Update mains LED
    if (ui_mains_led) {
        // Mains is detected if voltage is above 200V (assuming European 230V system)
        if (camper_data->mains_voltage > 6.0f) {
            lv_led_on(ui_mains_led);
            lv_led_set_color(ui_mains_led, lv_color_hex(0x00FF00)); // Green when mains connected
        } else {
            lv_led_off(ui_mains_led);
            lv_led_set_color(ui_mains_led, lv_color_hex(0x808080)); // Gray when disconnected
        }
    }

    // Update water level bar
    if (ui_water_bar) {
        lv_bar_set_value(ui_water_bar, camper_data->water_state, LV_ANIM_ON);
        
        // Add warning indicator if water level is low
        if (camper_data->water_state < WATER_LOW_THRESHOLD) {
            lv_obj_set_style_bg_color(ui_water_bar, lv_color_hex(0xFF8000), LV_PART_INDICATOR);
        } else {
            lv_obj_set_style_bg_color(ui_water_bar, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
        }
    }

    // Update waste level bar
    if (ui_waste_bar) {
        lv_bar_set_value(ui_waste_bar, camper_data->waste_state, LV_ANIM_ON);
        
        // Add warning indicator if waste level is high
        if (camper_data->waste_state > WASTE_HIGH_THRESHOLD) {
            lv_obj_set_style_bg_color(ui_waste_bar, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
        } else {
            lv_obj_set_style_bg_color(ui_waste_bar, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_INDICATOR);
        }
    }

    // Update battery gauges
    if (ui_starter_battery && ui_starter_battery_needle) {
        update_battery_gauge(ui_starter_battery, ui_starter_battery_needle, camper_data->starter_voltage);
    }

    if (ui_household_battery && ui_household_battery_needle) {
        update_battery_gauge(ui_household_battery, ui_household_battery_needle, camper_data->household_voltage);
    }
}
