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

static int set_household_switch_status(const char *url, const char *status) {
    CURL *curl;
    CURLcode res;
    int result = 0;

    // Initialize libcurl
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize libcurl\n");
        return 1;
    }

    // Prepare the JSON payload
    char json_payload[128];
    snprintf(json_payload, sizeof(json_payload), "{\"switch_status\": \"%s\"}", status);

    // Set the URL
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Set the HTTP headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set the POST data
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);

    // Perform the request
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        result = 1;
    }

    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return result;
}

static void household_event_handler(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);

    const char *status = checked ? "ON" : "OFF";
    printf("Household switch changed to: %s\n", status);
    
    // Send the API request
    const char *api_url = "http://example.com/api/household_switch";
    if (set_household_switch_status(api_url, status) != 0) {
        fprintf(stderr, "Failed to update household switch status via API\n");
    }
}

static void pump_event_handler(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    printf("Pump switch changed to: %s\n", checked ? "ON" : "OFF");
    
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
}

static lv_obj_t* create_battery_gauge(lv_obj_t *parent, const char *title, float voltage) {
    // Create a container for the title and scale
    lv_obj_t *gauge_container = lv_obj_create(parent);
    lv_obj_set_size(gauge_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(gauge_container, 0, 0); // Remove padding
    lv_obj_set_style_border_width(gauge_container, 0, 0); // Remove border
    lv_obj_set_style_bg_opa(gauge_container, LV_OPA_TRANSP, 0); // Transparent background
    lv_obj_set_flex_flow(gauge_container, LV_FLEX_FLOW_COLUMN); // Vertical layout
    lv_obj_set_flex_align(gauge_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Add title label
    lv_obj_t *title_label = lv_label_create(gauge_container);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);

    // Create the scale
    lv_obj_t *scale_line = lv_scale_create(gauge_container);
    lv_obj_set_size(scale_line, 130, 130); // Adjust size to fit the container
    lv_obj_center(scale_line); // Center the scale in the container

    // Configure the scale
    lv_scale_set_mode(scale_line, LV_SCALE_MODE_ROUND_INNER);
    lv_obj_set_style_bg_opa(scale_line, LV_OPA_TRANSP, 0); // Transparent background
    lv_obj_set_style_border_width(scale_line, 0, 0); // Remove border

    // Configure ticks and labels
    lv_obj_set_style_transform_rotation(scale_line, 450, LV_PART_INDICATOR);
    lv_obj_set_style_translate_x(scale_line, 10, LV_PART_INDICATOR);
    lv_obj_set_style_length(scale_line, 15, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(scale_line, 5, LV_PART_INDICATOR);
    
    // Configure minor ticks
    lv_obj_set_style_length(scale_line, 10, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(scale_line, 5, LV_PART_ITEMS);
    lv_obj_set_style_line_opa(scale_line, LV_OPA_50, LV_PART_ITEMS);

    // Show value labels on the scale
    lv_scale_set_label_show(scale_line, true);

    // Configure ticks and range
    lv_scale_set_total_tick_count(scale_line, 11);
    lv_scale_set_major_tick_every(scale_line, 2);
    lv_scale_set_range(scale_line, 90, 140);
    
    static const char *custom_labels[] = {"9", "10", "11", "12", "13", "14", NULL};
    lv_scale_set_text_src(scale_line, custom_labels);

    // Set scale rotation and angle range
    lv_scale_set_angle_range(scale_line, 270);
    lv_scale_set_rotation(scale_line, 135);

    // Create a needle for the current value
    lv_obj_t *needle_line = lv_line_create(scale_line);
    lv_obj_set_style_line_width(needle_line, 3, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(needle_line, true, LV_PART_MAIN);

    update_battery_gauge(scale_line, needle_line, voltage);

    return scale_line;
}

void create_status_tab(lv_obj_t *left_column)
{
    // Set up left column as a vertical container
    lv_obj_set_flex_flow(left_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(left_column, 8, 0);

    // Create a container for the household switch, pump switch, and mains LED
    lv_obj_t *status_row_container = lv_obj_create(left_column);
    lv_obj_set_size(status_row_container, lv_pct(100), 80); // Adjust height as needed
    lv_obj_set_style_border_width(status_row_container, 0, 0); // Remove border
    lv_obj_set_style_pad_all(status_row_container, 5, 0);
    lv_obj_set_flex_flow(status_row_container, LV_FLEX_FLOW_ROW); // Horizontal layout
    lv_obj_set_flex_align(status_row_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Household switch with label
    lv_obj_t *household_container = lv_obj_create(status_row_container);
    lv_obj_set_size(household_container, lv_pct(30), lv_pct(100)); // Adjust size as needed
    lv_obj_set_style_border_width(household_container, 0, 0); // Remove border
    lv_obj_set_style_pad_all(household_container, 5, 0);
    lv_obj_set_flex_flow(household_container, LV_FLEX_FLOW_COLUMN); // Vertical layout
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
    lv_obj_set_size(mains_container, lv_pct(30), lv_pct(100)); // Adjust size as needed
    lv_obj_set_style_border_width(mains_container, 0, 0); // Remove border
    lv_obj_set_style_pad_all(mains_container, 5, 0);
    lv_obj_set_flex_flow(mains_container, LV_FLEX_FLOW_COLUMN); // Vertical layout
    lv_obj_set_flex_align(mains_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *mains_label = lv_label_create(mains_container);
    lv_label_set_text(mains_label, "Mains");
    lv_obj_set_style_text_font(mains_label, &lv_font_montserrat_16, 0);

    lv_obj_t *mains_led = lv_led_create(mains_container);
    lv_obj_set_size(mains_led, 20, 20);
    lv_led_set_color(mains_led, lv_color_hex(0x808080)); // Gray for unknown status
    lv_led_set_brightness(mains_led, 255); // Full brightness
    lv_led_on(mains_led); // Turn LED on
    
    // Define styles for the bars
    static lv_style_t style_bar_bg;
    static lv_style_t style_bar_indic;

    lv_style_init(&style_bar_bg);
    lv_style_set_border_color(&style_bar_bg, lv_palette_main(LV_PALETTE_BLUE)); // Border color
    lv_style_set_border_width(&style_bar_bg, 2);                               // Border width
    lv_style_set_pad_all(&style_bar_bg, 6);                                    // Padding to make the indicator smaller
    lv_style_set_radius(&style_bar_bg, 6);                                     // Rounded corners

    lv_style_init(&style_bar_indic);
    lv_style_set_bg_opa(&style_bar_indic, LV_OPA_COVER);                       // Indicator opacity
    lv_style_set_bg_color(&style_bar_indic, lv_palette_main(LV_PALETTE_BLUE)); // Indicator color
    lv_style_set_radius(&style_bar_indic, 3);                                  // Rounded corners for the indicator

    // Water level bar
    lv_obj_t *water_label = lv_label_create(left_column);
    lv_label_set_text(water_label, "Fresh Water");

    lv_obj_t *water_bar = lv_bar_create(left_column);
    lv_obj_remove_style_all(water_bar); // Remove all default styles
    lv_obj_add_style(water_bar, &style_bar_bg, 0); // Apply background style
    lv_obj_add_style(water_bar, &style_bar_indic, LV_PART_INDICATOR); // Apply indicator style
    lv_obj_set_size(water_bar, lv_pct(100), 30);
    lv_bar_set_range(water_bar, 0, 100);
    lv_bar_set_value(water_bar, 60, LV_ANIM_OFF); // Example value

    // Waste level bar
    lv_obj_t *waste_label = lv_label_create(left_column);
    lv_label_set_text(waste_label, "Waste Water");

    lv_obj_t *waste_bar = lv_bar_create(left_column);
    lv_obj_remove_style_all(waste_bar); // Remove all default styles
    lv_obj_add_style(waste_bar, &style_bar_bg, 0); // Apply background style
    lv_obj_add_style(waste_bar, &style_bar_indic, LV_PART_INDICATOR); // Apply indicator style
    lv_obj_set_size(waste_bar, lv_pct(100), 30);
    lv_bar_set_range(waste_bar, 0, 100);
    lv_bar_set_value(waste_bar, 40, LV_ANIM_OFF); // Example value

    // Create a container for voltage info with row layout
    lv_obj_t *voltage_container = lv_obj_create(left_column);
    lv_obj_set_size(voltage_container, lv_pct(100), 180);
    lv_obj_set_style_pad_all(voltage_container, 3, 0);
    lv_obj_set_style_border_width(voltage_container, 0, 0);

    // Use a row layout for voltages - this places containers side by side
    lv_obj_set_flex_flow(voltage_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(voltage_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Create starter container (left side)
    lv_obj_t *starter_container = lv_obj_create(voltage_container);
    lv_obj_set_size(starter_container, lv_pct(48), lv_pct(95));
    lv_obj_set_style_pad_all(starter_container, 0, 0); // Remove padding
    lv_obj_set_style_border_width(starter_container, 0, 0); // Remove border
    lv_obj_set_style_radius(starter_container, 0, 0); // Remove radius
    lv_obj_set_style_bg_opa(starter_container, LV_OPA_TRANSP, 0); // Transparent background

    // Create household container
    lv_obj_t *household_v_container = lv_obj_create(voltage_container);
    lv_obj_set_size(household_v_container, lv_pct(48), lv_pct(95));
    lv_obj_set_style_pad_all(household_v_container, 0, 0); // Remove padding
    lv_obj_set_style_border_width(household_v_container, 0, 0); // Remove border
    lv_obj_set_style_radius(household_v_container, 0, 0); // Remove radius
    lv_obj_set_style_bg_opa(household_v_container, LV_OPA_TRANSP, 0); // Transparent background

    // Create battery gauges using our new function
    lv_obj_t *starter_scale = create_battery_gauge(starter_container, "Starter Battery", 12.6);
    lv_obj_t *household_scale = create_battery_gauge(household_v_container, "Household Battery", 12.4);
}