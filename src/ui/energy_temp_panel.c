#include <stdbool.h>
#include <math.h>
#include "lvgl/lvgl.h"
#include "energy_temp_panel.h"
#include "../lib/logger.h"
#include "../data/data_manager.h"

// Static variables for UI elements
static lv_obj_t *temperature_label = NULL;
static lv_obj_t *humidity_label = NULL; 
static lv_timer_t *update_timer = NULL;
static lv_chart_series_t *temp_series = NULL; // Temperature data series
static lv_chart_series_t *temp_min_series = NULL; // Min temperature data series
static lv_obj_t *chart = NULL; // Chart object

static lv_obj_t *power_label = NULL;
static lv_obj_t *battery_status_label = NULL;
static lv_chart_series_t *hourly_energy_series = NULL;
static lv_obj_t *energy_chart = NULL;

static lv_obj_t *solar_power_label = NULL;
static lv_chart_series_t *solar_hourly_energy_series = NULL;
static lv_obj_t *solar_energy_chart = NULL;
/**
 * Updates energy and temperature data displayed in the panel
 */
void update_temp_data(void) {
    // Get latest data from data manager
    float temperature = 23.5f; // Placeholder for temperature
    float temperature_min = temperature - 1.5f; // Simulate min temperature
    float humidity = 45.0f; // Placeholder for humidity (only for label)

    if (temperature_label != NULL) {
        lv_label_set_text_fmt(temperature_label, "%.1f °C", temperature);
    }
    
    if (humidity_label != NULL) {
        lv_label_set_text_fmt(humidity_label, "%.1f%%", humidity);
    }
    
    // Update chart with new temperature data only
    if (chart != NULL && temp_series != NULL && temp_min_series != NULL) {
        // Add new max temperature data point
        lv_chart_set_next_value(chart, temp_series, (int)temperature);
        
        // Add new min temperature data point
        lv_chart_set_next_value(chart, temp_min_series, (int)temperature_min);
        
        // Refresh the chart to show new data
        lv_chart_refresh(chart);
    }
}

void update_energy_data(void) {
    int battery_power = 350; // Placeholder for battery energy
    int battery_status = 99;

    // Update battery energy label
    if (power_label != NULL) {
        lv_label_set_text_fmt(power_label, "%d W", battery_power);
    }
    if (battery_status_label != NULL) {
        lv_label_set_text_fmt(battery_status_label, "%d %%", battery_status);
    }
    
    // Update hourly energy chart - in a real app, we would only update once per hour
    if (energy_chart != NULL && hourly_energy_series != NULL) {
        // For demo purposes, just add a random value
        int hourly_energy = lv_rand(50, 250); // Random Wh value
        lv_chart_set_next_value(energy_chart, hourly_energy_series, hourly_energy);
        lv_chart_refresh(energy_chart);
    }
}

void update_solar_data(void) {
    int solar_power = 210; // Placeholder for battery energy

    // Update battery energy label
    if (solar_power_label != NULL) {
        lv_label_set_text_fmt(solar_power_label, "%d W", solar_power);
    }
    
    // Update hourly energy chart - in a real app, we would only update once per hour
    if (solar_hourly_energy_series != NULL && solar_energy_chart != NULL) {
        // For demo purposes, just add a random value
        int hourly_energy = lv_rand(50, 250); // Random Wh value
        lv_chart_set_next_value(solar_energy_chart, solar_hourly_energy_series, hourly_energy);
        lv_chart_refresh(solar_energy_chart);
    }
}

/**
 * Timer callback to update energy and temperature data
 */
static void update_timer_cb(lv_timer_t *timer) {
    update_temp_data();
    update_energy_data();
    update_solar_data();
}

void create_temperature_container(lv_obj_t *right_column) {
    // Temperature and humidity section - container with flex layout
    lv_obj_t *temp_humid_container = lv_obj_create(right_column);
    lv_obj_set_size(temp_humid_container, LV_PCT(100), 160);
    lv_obj_set_style_border_width(temp_humid_container, 0, 0);
    lv_obj_set_style_radius(temp_humid_container, 0, 0);
    lv_obj_set_style_pad_all(temp_humid_container, 5, 0);
    lv_obj_set_scrollbar_mode(temp_humid_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(temp_humid_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Use row layout for the container
    lv_obj_set_flex_flow(temp_humid_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp_humid_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Left column for labels
    lv_obj_t *labels_column = lv_obj_create(temp_humid_container);
    lv_obj_set_size(labels_column, LV_PCT(20), LV_PCT(100));
    lv_obj_set_style_border_width(labels_column, 0, 0);
    lv_obj_set_style_radius(labels_column, 0, 0);
    lv_obj_set_style_pad_all(labels_column, 0, 0);
    lv_obj_set_flex_flow(labels_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(labels_column, 5, 0);
    lv_obj_set_flex_align(labels_column, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(labels_column, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(labels_column, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *temperature_caption = lv_label_create(labels_column);
    lv_label_set_text(temperature_caption, "Temperature");
    
    // Temperature value with larger font (removed caption)
    temperature_label = lv_label_create(labels_column);
    lv_label_set_text(temperature_label, "--- °C");
    lv_obj_set_style_text_font(temperature_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(temperature_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(temperature_label, LV_PCT(100));
    
    // Spacer
    lv_obj_t *spacer = lv_obj_create(labels_column);
    lv_obj_set_height(spacer, 10);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_0, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_scrollbar_mode(spacer, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    // Humidity caption
    lv_obj_t *humidity_caption = lv_label_create(labels_column);
    lv_label_set_text(humidity_caption, "Humidity");

    // Humidity value with larger font (removed caption)
    humidity_label = lv_label_create(labels_column);
    lv_label_set_text(humidity_label, "--- %");
    lv_obj_set_style_text_font(humidity_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(humidity_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(humidity_label, LV_PCT(100));
    
    // Right column for chart
    lv_obj_t *chart_container = lv_obj_create(temp_humid_container);
    lv_obj_set_size(chart_container, LV_PCT(80), LV_PCT(100));
    lv_obj_set_style_border_width(chart_container, 0, 0);
    lv_obj_set_style_radius(chart_container, 0, 0);
    lv_obj_set_style_pad_all(chart_container, 5, 0);
    lv_obj_set_scrollbar_mode(chart_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(chart_container, LV_OBJ_FLAG_SCROLLABLE);

    // Add title to the chart
    lv_obj_t *chart_title = lv_label_create(chart_container);
    lv_label_set_text(chart_title, "Hourly Temperature (°C)");
    lv_obj_set_style_pad_all(chart_title, -5, 0);
    lv_obj_align(chart_title, LV_ALIGN_TOP_MID, 0, 0);

    // Create a chart
    chart = lv_chart_create(chart_container);
    lv_obj_set_size(chart, LV_PCT(95), LV_PCT(80));
    lv_obj_center(chart);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(chart, 5, 7);
    
    // Fix: Set proper size for chart indicators (points on the line)
    lv_obj_set_style_size(chart, 4, 4, LV_PART_INDICATOR); // Width and height for points
    
    // Range setup
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 40); // Temperature: 0-40°C
    
    // Set point count - increased to 48 hours
    lv_chart_set_point_count(chart, 48); // 48 data points (hourly for past 48 hours)
    
    // Add data series
    temp_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y); // Max temp in red
    temp_min_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y); // Min temp in blue
    
    // Initialize chart with dummy data
    for(int i = 0; i < 48; i++) {
        // Base temperature pattern that simulates day/night cycle
        float base_temp = 22.0f + 3.0f * sinf(i * 3.14159f / 12.0f);
        
        // Set initial max temperature data points (with some variation)
        float max_temp = base_temp + lv_rand(0, 20) / 10.0f;
        lv_chart_set_next_value(chart, temp_series, (int)max_temp);
        
        // Set initial min temperature data points (3-5 degrees lower than max)
        float min_temp = max_temp - 3.0f - lv_rand(0, 20) / 10.0f;
        lv_chart_set_next_value(chart, temp_min_series, (int)min_temp);
    }
    
    lv_chart_refresh(chart); // Required after direct set
    
    // Initial update
    update_temp_data();
}

/**
 * Creates container with battery power and hourly energy chart
 */
void create_energy_container(lv_obj_t *right_column) {
    // Main container for battery energy and hourly chart
    lv_obj_t *energy_container = lv_obj_create(right_column);
    lv_obj_set_size(energy_container, LV_PCT(100), 160); // Same height as temperature container
    lv_obj_set_style_border_width(energy_container, 0, 0);
    lv_obj_set_style_radius(energy_container, 0, 0);
    lv_obj_set_style_pad_all(energy_container, 5, 0);
    lv_obj_set_scrollbar_mode(energy_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(energy_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Use row layout for the container
    lv_obj_set_flex_flow(energy_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(energy_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Left column for battery energy display
    lv_obj_t *power_column = lv_obj_create(energy_container);
    lv_obj_set_size(power_column, LV_PCT(20), LV_PCT(100));
    lv_obj_set_style_border_width(power_column, 0, 0);
    lv_obj_set_style_radius(power_column, 0, 0);
    lv_obj_set_style_pad_all(power_column, 5, 0);
    lv_obj_set_flex_flow(power_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(power_column, 5, 0);
    lv_obj_set_flex_align(power_column, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(power_column, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(power_column, LV_OBJ_FLAG_SCROLLABLE);
    
    // Caption label
    lv_obj_t *power_caption = lv_label_create(power_column);
    lv_label_set_text(power_caption, "Power");
    
    // Battery power value label
    power_label = lv_label_create(power_column);
    lv_label_set_text(power_label, "--- W");
    lv_obj_set_style_text_font(power_label, &lv_font_montserrat_20, 0);

    // Caption label
    lv_obj_t *status_caption = lv_label_create(power_column);
    lv_label_set_text(status_caption, "Status");

    // Battery status value label
    battery_status_label = lv_label_create(power_column);
    lv_label_set_text(battery_status_label, "-- %");
    lv_obj_set_style_text_font(battery_status_label, &lv_font_montserrat_20, 0);

    // Right column for hourly energy chart
    lv_obj_t *hourly_chart_container = lv_obj_create(energy_container);
    lv_obj_set_size(hourly_chart_container, LV_PCT(80), LV_PCT(100));
    lv_obj_set_style_border_width(hourly_chart_container, 0, 0);
    lv_obj_set_style_radius(hourly_chart_container, 0, 0);
    lv_obj_set_style_pad_all(hourly_chart_container, 5, 0);
    lv_obj_set_scrollbar_mode(hourly_chart_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(hourly_chart_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create a chart for hourly energy
    energy_chart = lv_chart_create(hourly_chart_container);
    lv_obj_set_size(energy_chart, LV_PCT(95), LV_PCT(80));
    lv_obj_center(energy_chart);
    lv_chart_set_type(energy_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_div_line_count(energy_chart, 5, 7);
    lv_obj_set_style_pad_column(energy_chart, 2, 0);
    lv_chart_set_point_count(energy_chart, 48);

    // Add title to the chart
    lv_obj_t *chart_title = lv_label_create(hourly_chart_container);
    lv_label_set_text(chart_title, "Hourly Battery Energy (Wh)");
    lv_obj_set_style_pad_all(chart_title, -5, 0);
    lv_obj_align(chart_title, LV_ALIGN_TOP_MID, 0, 0);
    
    // Set range for the chart to ensure all data points are visible
    lv_chart_set_range(energy_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 300);
    
    // Add data series for hourly energy
    hourly_energy_series = lv_chart_add_series(
        energy_chart, 
        lv_palette_main(LV_PALETTE_GREEN), 
        LV_CHART_AXIS_PRIMARY_Y
    );
    
    // Initialize chart with dummy data - using 24 hours
    for (int i = 0; i < 48; i++) {
        // Generate random energy between 50 and 250 Wh
        int energy = lv_rand(50, 250);
        lv_chart_set_next_value(energy_chart, hourly_energy_series, energy);
    }
    
    lv_chart_refresh(energy_chart); // Force refresh after all points are set

    update_energy_data();
}

void create_solar_container(lv_obj_t *right_column) {
    // Main container for solar energy and hourly chart
    lv_obj_t *solar_container = lv_obj_create(right_column);
    lv_obj_set_size(solar_container, LV_PCT(100), 160);
    lv_obj_set_style_border_width(solar_container, 0, 0);
    lv_obj_set_style_radius(solar_container, 0, 0);
    lv_obj_set_style_pad_all(solar_container, 5, 0);
    lv_obj_set_scrollbar_mode(solar_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(solar_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Use row layout for the container
    lv_obj_set_flex_flow(solar_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(solar_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Left column for solar energy display
    lv_obj_t *power_column = lv_obj_create(solar_container);
    lv_obj_set_size(power_column, LV_PCT(20), LV_PCT(100));
    lv_obj_set_style_border_width(power_column, 0, 0);
    lv_obj_set_style_radius(power_column, 0, 0);
    lv_obj_set_style_pad_all(power_column, 5, 0);
    lv_obj_set_flex_flow(power_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(power_column, 5, 0);
    lv_obj_set_flex_align(power_column, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(power_column, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(power_column, LV_OBJ_FLAG_SCROLLABLE);
    
    // Caption label
    lv_obj_t *power_caption = lv_label_create(power_column);
    lv_label_set_text(power_caption, "Power");
    
    // Battery power value label
    solar_power_label = lv_label_create(power_column);
    lv_label_set_text(solar_power_label, "--- W");
    lv_obj_set_style_text_font(solar_power_label, &lv_font_montserrat_20, 0);

    // Right column for hourly energy chart
    lv_obj_t *hourly_chart_container = lv_obj_create(solar_container);
    lv_obj_set_size(hourly_chart_container, LV_PCT(80), LV_PCT(100));
    lv_obj_set_style_border_width(hourly_chart_container, 0, 0);
    lv_obj_set_style_radius(hourly_chart_container, 0, 0);
    lv_obj_set_style_pad_all(hourly_chart_container, 5, 0);
    lv_obj_set_scrollbar_mode(hourly_chart_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(hourly_chart_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create a chart for hourly energy
    solar_energy_chart = lv_chart_create(hourly_chart_container);
    lv_obj_set_size(solar_energy_chart, LV_PCT(95), LV_PCT(80));
    lv_obj_center(solar_energy_chart);
    lv_chart_set_type(solar_energy_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_div_line_count(solar_energy_chart, 5, 7);
    lv_obj_set_style_pad_column(solar_energy_chart, 2, 0);
    lv_chart_set_point_count(solar_energy_chart, 48);

    // Add title to the chart
    lv_obj_t *chart_title = lv_label_create(hourly_chart_container);
    lv_label_set_text(chart_title, "Hourly Solar Energy (Wh)");
    lv_obj_set_style_pad_all(chart_title, -5, 0);
    lv_obj_align(chart_title, LV_ALIGN_TOP_MID, 0, 0);
    
    // Set range for the chart to ensure all data points are visible
    lv_chart_set_range(solar_energy_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 300);
    
    // Add data series for hourly energy
    solar_hourly_energy_series = lv_chart_add_series(
        solar_energy_chart, 
        lv_palette_main(LV_PALETTE_GREEN), 
        LV_CHART_AXIS_PRIMARY_Y
    );
    
    // Initialize chart with dummy data
    for (int i = 0; i < 48; i++) {
        // Generate random energy between 50 and 250 Wh
        int energy = lv_rand(50, 250);
        lv_chart_set_next_value(solar_energy_chart, solar_hourly_energy_series, energy);
    }
    
    lv_chart_refresh(solar_energy_chart); // Force refresh after all points are set

    update_energy_data();
}

/**
 * Creates energy and temperature panel in the provided parent container
 */
void create_energy_temp_panel(lv_obj_t *right_column) {
    // Use column layout
    lv_obj_set_flex_flow(right_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right_column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(right_column, 8, 0);
    lv_obj_set_scrollbar_mode(right_column, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(right_column, LV_OBJ_FLAG_SCROLLABLE);

    create_temperature_container(right_column);
    create_energy_container(right_column);
    create_solar_container(right_column);

    // Create a timer to update the values periodically
    update_timer = lv_timer_create(update_timer_cb, 5000, NULL);  // Update every 5 seconds

    log_info("Energy and temperature panel created");
}

/**
 * Cleanup resources used by the energy and temperature panel
 */
void energy_temp_panel_cleanup(void) {
    if (update_timer != NULL) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    
    // Reset all static UI element pointers
    temperature_label = NULL;
    humidity_label = NULL;
    chart = NULL;
    temp_series = NULL;
    temp_min_series = NULL;
    
    // Reset energy container elements
    power_label = NULL;
    battery_status_label = NULL;
    energy_chart = NULL;
    hourly_energy_series = NULL;
    
    solar_power_label = NULL;
    solar_hourly_energy_series = NULL;
    solar_energy_chart = NULL;

    log_info("Energy and temperature panel cleaned up");
}
