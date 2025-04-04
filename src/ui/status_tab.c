/*******************************************************************
 *
 * status_tab.c - Status tab UI implementation for the Camper GUI
 *
 ******************************************************************/
#include "status_tab.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h> 
#include "../lib/logger.h"
#include "../lib/http_client.h"
#include "lvgl/lvgl.h"

// Updated function in status_tab.c
static int set_household_switch_status(const char *url, const char *status) {
    // Prepare the JSON payload
    char json_payload[128];
    snprintf(json_payload, sizeof(json_payload), "{\"switch_status\": \"%s\"}", status);
    
    // Make the POST request
    http_response_t response = http_post_json(url, json_payload, 10);
    
    // Log the result
    if (!response.success) {
        log_error("Failed to update switch status: %s", response.error);
        if (response.body && *response.body) {
            log_error("Response body: %s", response.body);
        }
    } else {
        log_info("Switch status updated successfully");
        log_debug("Response: %s", response.body);
    }
    
    // Free the response
    http_response_free(&response);
    
    return response.success ? 0 : 1;
}

static void household_event_handler(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);

    const char *status = checked ? "ON" : "OFF";
    log_info("Household switch changed to: %s", status);
    
    // Send the API request
    const char *api_url = "http://example.com/api/household_switch";
    if (set_household_switch_status(api_url, status) != 0) {
        log_error("Failed to update household switch status via API");
    }
}

static void pump_event_handler(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    log_info("Pump switch changed to: %s", checked ? "ON" : "OFF");
    
    // Here you would add the actual API call or functionality
    // If checked is true, the switch is ON; if false, it's OFF
}

static void update_battery_gauge(lv_obj_t *scale_line, lv_obj_t *needle_line, float voltage) {
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

    // Retrieve label pointer from the scale's user data
    lv_obj_t *voltage_label = lv_obj_get_user_data(scale_line);
    if(voltage_label) {
        // Update the text to show current voltage
        lv_label_set_text_fmt(voltage_label, "%.1fV", voltage);
    }
}

static lv_obj_t* create_battery_gauge(lv_obj_t *parent, const char *title, float voltage) {
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

    lv_style_init(&style_section_red);
    lv_style_set_arc_color(&style_section_red, lv_color_hex(0xFF0000));
    lv_style_set_arc_width(&style_section_red, 3);

    lv_style_init(&style_section_orange);
    lv_style_set_arc_color(&style_section_orange, lv_color_hex(0xFF8000));
    lv_style_set_arc_width(&style_section_orange, 3);

    lv_style_init(&style_section_green);
    lv_style_set_arc_color(&style_section_green, lv_color_hex(0x00C853));
    lv_style_set_arc_width(&style_section_green, 3);

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

    // Create the voltage label
    lv_obj_t *voltage_label = lv_label_create(gauge_container);
    lv_label_set_text_fmt(voltage_label, "%.1fV", voltage);
    lv_obj_set_style_text_font(voltage_label, &lv_font_montserrat_16, 0);

    // Align the voltage label so it appears on the missing portion (top) of the arc
    // Adjust the final (y) offset as needed
    lv_obj_align_to(voltage_label, scale_line, LV_ALIGN_CENTER, 0, +55);

    // Store the label pointer so we can update it later
    lv_obj_set_user_data(scale_line, voltage_label);

    // Initialize the gauge to the correct voltage
    update_battery_gauge(scale_line, needle_line, voltage);

    return gauge_container;
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
    
    // Create styles for the bar
    lv_style_t *style_bar_bg = malloc(sizeof(lv_style_t));
    lv_style_t *style_bar_indic = malloc(sizeof(lv_style_t));

    lv_style_init(style_bar_bg);
    lv_style_set_border_color(style_bar_bg, color); 
    lv_style_set_border_width(style_bar_bg, 2);
    lv_style_set_pad_all(style_bar_bg, 6);
    lv_style_set_radius(style_bar_bg, 6); 

    lv_style_init(style_bar_indic);
    lv_style_set_bg_opa(style_bar_indic, LV_OPA_COVER); 
    lv_style_set_bg_color(style_bar_indic, color); 
    lv_style_set_radius(style_bar_indic, 3);

    // Create the bar
    lv_obj_t *bar = lv_bar_create(container);
    lv_obj_remove_style_all(bar);
    lv_obj_add_style(bar, style_bar_bg, 0); 
    lv_obj_add_style(bar, style_bar_indic, LV_PART_INDICATOR);
    lv_obj_set_size(bar, lv_pct(100), 30);  
    lv_bar_set_range(bar, 0, 100); 
    lv_bar_set_value(bar, initial_value, LV_ANIM_OFF);

    return bar;
}

void create_status_tab(lv_obj_t *left_column)
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
    lv_obj_add_state(household_switch, LV_STATE_CHECKED); // Default to ON
    lv_obj_set_style_bg_color(household_switch, lv_color_hex(0x008800), LV_PART_INDICATOR | LV_STATE_CHECKED); // Green when ON
    lv_obj_add_event_cb(household_switch, household_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    // Pump switch with label
    lv_obj_t *pump_container = lv_obj_create(status_row_container);
    lv_obj_set_size(pump_container, lv_pct(30), lv_pct(100)); // Adjust size as needed
    lv_obj_set_style_border_width(pump_container, 0, 0); // Remove border
    lv_obj_set_style_pad_all(pump_container, 5, 0);
    lv_obj_set_flex_flow(pump_container, LV_FLEX_FLOW_COLUMN); // Vertical layout
    lv_obj_set_flex_align(pump_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *pump_label = lv_label_create(pump_container);
    lv_label_set_text(pump_label, "Pump");
    lv_obj_set_style_text_font(pump_label, &lv_font_montserrat_16, 0);

    lv_obj_t *pump_switch = lv_switch_create(pump_container);
    lv_obj_set_style_bg_color(pump_switch, lv_color_hex(0x008800), LV_PART_INDICATOR | LV_STATE_CHECKED); // Green when ON
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
    lv_obj_t *water_bar = create_level_bar(left_column, "Fresh Water", 60, lv_palette_main(LV_PALETTE_BLUE));
    lv_obj_t *waste_bar = create_level_bar(left_column, "Waste Water", 40, lv_palette_main(LV_PALETTE_ORANGE));
 
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

    // Create battery gauges using our new function
    lv_obj_t *starter_scale = create_battery_gauge(voltage_container, "Starter Battery", 12.6);
    lv_obj_t *household_scale = create_battery_gauge(voltage_container, "Household Battery", 12.4);
}