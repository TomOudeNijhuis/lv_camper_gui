#include <stdbool.h>
#include <math.h>
#include "lvgl/lvgl.h"
#include "energy_temp_panel.h"
#include "../lib/logger.h"
#include "../data/data_manager.h"
#include "ui.h"

// Static variables for UI elements
static lv_obj_t*   temperature_label = NULL;
static lv_obj_t*   humidity_label    = NULL;
static lv_timer_t* update_timer      = NULL;
static lv_timer_t* update_long_timer = NULL;

static lv_chart_series_t* temp_series     = NULL;
static lv_chart_series_t* temp_min_series = NULL;
static lv_obj_t*          chart           = NULL;

static lv_obj_t*          power_label          = NULL;
static lv_obj_t*          battery_status_label = NULL;
static lv_chart_series_t* hourly_energy_series = NULL;
static lv_obj_t*          energy_chart         = NULL;

static lv_obj_t*          solar_power_label          = NULL;
static lv_obj_t*          solar_state_label          = NULL;
static lv_chart_series_t* solar_hourly_energy_series = NULL;
static lv_obj_t*          solar_energy_chart         = NULL;

void update_climate_chart_with_history(entity_history_t* history_data)
{
    if(chart != NULL && temp_series != NULL && temp_min_series != NULL && history_data != NULL)
    {
        // Clear existing data
        lv_chart_set_all_value(chart, temp_series, 0);
        lv_chart_set_all_value(chart, temp_min_series, 0);

        // Get the point count in the chart
        uint16_t point_count = lv_chart_get_point_count(chart);
        uint16_t data_count  = history_data->count;

        // Find the overall min and max temperature values
        float min_temp_overall = 100.0f;  // Start with high value
        float max_temp_overall = -100.0f; // Start with low value

        for(int i = 0; i < data_count; i++)
        {
            if(history_data->min[i] < min_temp_overall)
            {
                min_temp_overall = history_data->min[i];
            }
            if(history_data->max[i] > max_temp_overall)
            {
                max_temp_overall = history_data->max[i];
            }
        }

        // Add a 10% padding to the range to avoid data points touching the edges
        float range_padding = (max_temp_overall - min_temp_overall) * 0.1f;
        if(range_padding < 2.0f)
            range_padding = 2.0f; // At least 2 degrees padding

        float y_min = floor(min_temp_overall - range_padding);
        float y_max = ceil(max_temp_overall + range_padding);

        // Update the chart's Y-axis range
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, (int16_t)y_min, (int16_t)y_max);

        // Add dashed lines for min and max values
        static lv_obj_t*          min_line = NULL;
        static lv_obj_t*          max_line = NULL;
        static lv_point_precise_t min_line_points[2];
        static lv_point_precise_t max_line_points[2];

        // Remove previous lines if they exist
        if(min_line != NULL)
        {
            lv_obj_del(min_line);
            min_line = NULL;
        }
        if(max_line != NULL)
        {
            lv_obj_del(max_line);
            max_line = NULL;
        }

        // Get chart content dimensions and coordinates
        lv_coord_t chart_w = lv_obj_get_content_width(chart);
        lv_coord_t chart_h = lv_obj_get_content_height(chart);
        lv_area_t  chart_coords;
        lv_obj_get_content_coords(chart, &chart_coords);

        // Calculate points for the actual data area (not full chart width)
        float point_width = (float)chart_w / (point_count > 1 ? point_count - 1 : 1);
        float x_start     = 1;
        float x_end       = point_width * (data_count > 1 ? data_count - 1 : 1);

        // Create new min line (position it exactly at min temp value)
        min_line        = lv_line_create(chart);
        float min_y_pos = chart_h - ((min_temp_overall - y_min) * chart_h / (y_max - y_min));

        min_line_points[0].x = x_start;
        min_line_points[0].y = min_y_pos;
        min_line_points[1].x = x_end;
        min_line_points[1].y = min_y_pos;

        lv_line_set_points(min_line, min_line_points, 2);
        lv_obj_set_style_line_width(min_line, 1, 0);
        lv_obj_set_style_line_color(min_line, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_obj_set_style_line_dash_width(min_line, 3, 0);
        lv_obj_set_style_line_dash_gap(min_line, 3, 0);

        // Create new max line (position it slightly below the max value to avoid covering it)
        max_line = lv_line_create(chart);
        // Add a small offset (1px) to avoid covering the max value points
        float max_y_pos = chart_h - ((max_temp_overall - y_min) * chart_h / (y_max - y_min)) + 1;

        max_line_points[0].x = x_start;
        max_line_points[0].y = max_y_pos;
        max_line_points[1].x = x_end;
        max_line_points[1].y = max_y_pos;

        lv_line_set_points(max_line, max_line_points, 2);
        lv_obj_set_style_line_width(max_line, 1, 0);
        lv_obj_set_style_line_color(max_line, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_line_dash_width(max_line, 3, 0);
        lv_obj_set_style_line_dash_gap(max_line, 3, 0);

        // Fill chart with real data
        for(int i = 0; i < point_count && i < data_count; i++)
        {
            // Convert data from history to chart format
            // History data typically comes newest first, so we reverse it
            int idx = data_count - i - 1;
            if(idx >= 0)
            {
                int temp_value = (int)history_data->max[idx];
                lv_chart_set_next_value(chart, temp_series, temp_value);

                // Use the min data for our second series
                int min_temp = (int)history_data->min[idx];
                lv_chart_set_next_value(chart, temp_min_series, min_temp);
            }
        }

        // Update min/max labels if they exist, or create them if they don't
        static lv_obj_t* max_temp_label = NULL;
        static lv_obj_t* min_temp_label = NULL;

        if(max_temp_label == NULL)
        {
            max_temp_label = lv_label_create(chart);
            lv_obj_set_style_text_font(max_temp_label, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(max_temp_label, lv_palette_main(LV_PALETTE_RED), 0);
        }

        if(min_temp_label == NULL)
        {
            min_temp_label = lv_label_create(chart);
            lv_obj_set_style_text_font(min_temp_label, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(min_temp_label, lv_palette_main(LV_PALETTE_BLUE), 0);
        }

        // Update the label texts with actual min/max values
        lv_label_set_text_fmt(max_temp_label, "%.1f°C", max_temp_overall);
        lv_label_set_text_fmt(min_temp_label, "%.1f°C", min_temp_overall);

        // Position the labels
        lv_obj_align(max_temp_label, LV_ALIGN_TOP_LEFT, 5, -8);
        lv_obj_align(min_temp_label, LV_ALIGN_BOTTOM_LEFT, 5, 8);

        // Refresh the chart to show new data
        lv_chart_refresh(chart);

        log_debug(
            "Climate chart updated with %d historical temperature points (range: %.1f-%.1f°C)",
            data_count, min_temp_overall, max_temp_overall);
    }
}

bool fetch_climate(void)
{
    bool result = request_entity_history("inside", "temperature", "1h", 48);
    if(!result)
    {
        log_warning("Failed to request temperature history data");
    }
    else
    {
        log_debug("Requested temperature history data");
    }

    return result;
}

bool update_climate_chart(void)
{

    // Check if historical data is available
    entity_history_t* history_data = get_entity_history_data();
    if(history_data != NULL && history_data->count > 0)
    {
        update_climate_chart_with_history(history_data);
        free_entity_history_data(history_data);

        return true;
    }

    return false;
}

bool update_energy_chart_with_history(entity_history_t* history_data)
{
    if(energy_chart != NULL && hourly_energy_series != NULL && history_data != NULL)
    {
        // Clear existing data
        lv_chart_set_all_value(energy_chart, hourly_energy_series, 0);

        // Get the point count in the chart
        uint16_t point_count = lv_chart_get_point_count(energy_chart);
        uint16_t data_count  = history_data->count;

        // Calculate hourly Ah changes by comparing adjacent data points
        float hourly_ah[point_count];
        float prev_sample = history_data->max[0];
        for(int i = 0; i < point_count && i < data_count - 1; i++)
        {
            if(prev_sample > history_data->max[i + 1])
            {
                // Reset Ah value
                hourly_ah[i] = history_data->max[i + 1];
            }
            else
            {
                // Calculate the difference in Ah
                hourly_ah[i] = history_data->max[i + 1] - prev_sample;
            }
            prev_sample = history_data->max[i + 1];
        }

        // Find max Ah change to set chart range
        float max_ah_change = 0;
        for(int i = 0; i < point_count && i < data_count - 1; i++)
        {
            if(hourly_ah[i] > max_ah_change)
                max_ah_change = hourly_ah[i];
        }

        // Set chart range with 10% padding
        int range_max = ceil(max_ah_change * 1.1);
        if(range_max < 10)
            range_max = 10; // Minimum range of 10Ah

        // Update the chart's Y-axis range (0 to max, since we're showing consumed Ah)
        lv_chart_set_range(energy_chart, LV_CHART_AXIS_PRIMARY_Y, 0, range_max);

        // Add dashed line for max value
        static lv_obj_t*          max_line = NULL;
        static lv_point_precise_t max_line_points[2];

        // Remove previous line if it exists
        if(max_line != NULL)
        {
            lv_obj_del(max_line);
            max_line = NULL;
        }

        // Get chart content dimensions and coordinates
        lv_coord_t chart_w = lv_obj_get_content_width(energy_chart);
        lv_coord_t chart_h = lv_obj_get_content_height(energy_chart);
        lv_area_t  chart_coords;
        lv_obj_get_content_coords(energy_chart, &chart_coords);

        // Calculate points for the actual data area
        float point_width = (float)chart_w / (point_count > 1 ? point_count - 1 : 1);
        float x_start     = 1;
        float x_end       = chart_w - 1;

        // Create new max line (position it exactly at max value)
        max_line        = lv_line_create(energy_chart);
        float max_y_pos = chart_h - ((max_ah_change / range_max) * chart_h);

        max_line_points[0].x = x_start;
        max_line_points[0].y = max_y_pos;
        max_line_points[1].x = x_end;
        max_line_points[1].y = max_y_pos;

        lv_line_set_points(max_line, max_line_points, 2);
        lv_obj_set_style_line_width(max_line, 1, 0);
        lv_obj_set_style_line_color(max_line, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_line_dash_width(max_line, 3, 0);
        lv_obj_set_style_line_dash_gap(max_line, 3, 0);

        // Add max value label
        static lv_obj_t* max_value_label = NULL;

        if(max_value_label == NULL)
        {
            max_value_label = lv_label_create(energy_chart);
            lv_obj_set_style_text_font(max_value_label, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(max_value_label, lv_palette_main(LV_PALETTE_RED), 0);
        }

        // Update the label text with actual max value
        lv_label_set_text_fmt(max_value_label, "%.1f Ah", max_ah_change);
        lv_obj_align(max_value_label, LV_ALIGN_TOP_LEFT, 5, -8);

        // Fill chart with calculated Ah data
        for(int i = 0; i < point_count && i < data_count - 1; i++)
        {
            // Convert data from history to chart format
            // History data typically comes newest first, so we reverse it
            int idx = data_count - i - 2;
            if(idx >= 0)
            {
                // For zero or negative values (charging periods), display a minimal positive value
                // so it's visible in the chart
                int ah_value = (hourly_ah[idx] <= 0.1f) ? 1 : (int)hourly_ah[idx];
                lv_chart_set_next_value(energy_chart, hourly_energy_series, ah_value);
            }
        }

        // Refresh the chart to show new data
        lv_chart_refresh(energy_chart);

        log_debug("Energy chart updated with %d historical Ah consumption points", data_count - 1);
        return true;
    }
    return false;
}

bool fetch_battery_consumption(void)
{
    bool result = request_entity_history("SmartShunt", "consumed_ah", "1h", 49);
    if(!result)
    {
        log_warning("Failed to request battery consumption history data");
    }
    else
    {
        log_debug("Requested battery consumption history data");
    }
    return result;
}

bool update_energy_chart(void)
{
    // Check if historical data is available
    entity_history_t* history_data = get_entity_history_data();
    if(history_data != NULL && history_data->count > 0)
    {
        update_energy_chart_with_history(history_data);
        free_entity_history_data(history_data);
        return true;
    }

    return false;
}

bool update_solar_chart_with_history(entity_history_t* history_data)
{
    if(solar_energy_chart != NULL && solar_hourly_energy_series != NULL && history_data != NULL)
    {
        // Clear existing data
        lv_chart_set_all_value(solar_energy_chart, solar_hourly_energy_series, 0);

        // Get the point count in the chart
        uint16_t point_count = lv_chart_get_point_count(solar_energy_chart);
        uint16_t data_count  = history_data->count;

        // Find the overall min and max yield values
        float hourly_yield[point_count];
        float prev_sample = history_data->max[0];
        float max_yield   = 0;

        for(int i = 0; i < point_count && i < data_count; i++)
        {
            if(prev_sample > history_data->max[i + 1])
            {
                hourly_yield[i] = history_data->max[i + 1]; // Reset yield
            }
            else
            {
                hourly_yield[i] = history_data->max[i + 1] - prev_sample;
            }

            if(hourly_yield[i] > max_yield)
                max_yield = hourly_yield[i];

            prev_sample = history_data->max[i + 1];
        }

        // Set chart range with 10% padding
        int range_max = ceil(max_yield * 1.1);
        if(range_max < 100)
            range_max = 100; // Minimum range of 100Wh

        // Update the chart's Y-axis range
        lv_chart_set_range(solar_energy_chart, LV_CHART_AXIS_PRIMARY_Y, 0, range_max);

        // Add dashed line for max value
        static lv_obj_t*          max_line = NULL;
        static lv_point_precise_t max_line_points[2];

        // Remove previous line if it exists
        if(max_line != NULL)
        {
            lv_obj_del(max_line);
            max_line = NULL;
        }

        // Get chart content dimensions and coordinates
        lv_coord_t chart_w = lv_obj_get_content_width(solar_energy_chart);
        lv_coord_t chart_h = lv_obj_get_content_height(solar_energy_chart);
        lv_area_t  chart_coords;
        lv_obj_get_content_coords(solar_energy_chart, &chart_coords);

        // Calculate points for the actual data area
        float point_width = (float)chart_w / (point_count > 1 ? point_count - 1 : 1);
        float x_start     = 1;
        float x_end       = chart_w - 1;

        // Create new max line (position it exactly at max value)
        max_line        = lv_line_create(solar_energy_chart);
        float max_y_pos = chart_h - ((max_yield / range_max) * chart_h);

        max_line_points[0].x = x_start;
        max_line_points[0].y = max_y_pos;
        max_line_points[1].x = x_end;
        max_line_points[1].y = max_y_pos;

        lv_line_set_points(max_line, max_line_points, 2);
        lv_obj_set_style_line_width(max_line, 1, 0);
        lv_obj_set_style_line_color(max_line, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_obj_set_style_line_dash_width(max_line, 3, 0);
        lv_obj_set_style_line_dash_gap(max_line, 3, 0);

        // Add max value label
        static lv_obj_t* max_value_label = NULL;

        if(max_value_label == NULL)
        {
            max_value_label = lv_label_create(solar_energy_chart);
            lv_obj_set_style_text_font(max_value_label, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(max_value_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        }

        // Update the label text with actual max value
        lv_label_set_text_fmt(max_value_label, "%.1f Wh", max_yield);
        lv_obj_align(max_value_label, LV_ALIGN_TOP_LEFT, 5, -8);

        // Fill chart with real data
        for(int i = 0; i < point_count && i < data_count; i++)
        {
            // Convert data from history to chart format
            // History data typically comes newest first, so we reverse it
            int idx = data_count - i - 1;
            if(idx >= 0)
            {
                if(hourly_yield[idx] <=
                   0.1f) // Use a small threshold to handle near-zero floating point values
                {
                    // Use LV_CHART_POINT_NONE for zero values to prevent displaying any bar
                    lv_chart_set_next_value(solar_energy_chart, solar_hourly_energy_series,
                                            LV_CHART_POINT_NONE);
                }
                else
                {
                    int yield_value = (int)hourly_yield[idx];
                    lv_chart_set_next_value(solar_energy_chart, solar_hourly_energy_series,
                                            yield_value);
                }
            }
        }

        // Refresh the chart to show new data
        lv_chart_refresh(solar_energy_chart);

        log_debug("Solar chart updated with %d historical yield points", data_count);
        return true;
    }
    return false;
}

bool fetch_solar(void)
{
    bool result = request_entity_history("SmartSolar", "yield_today", "1h", 49);
    if(!result)
    {
        log_warning("Failed to request solar yield history data");
    }
    else
    {
        log_debug("Requested solar yield history data");
    }
    return result;
}

bool update_solar_chart(void)
{
    // Check if historical data is available
    entity_history_t* history_data = get_entity_history_data();
    if((history_data != NULL) && (history_data->count > 0))
    {
        update_solar_chart_with_history(history_data);
        free_entity_history_data(history_data);

        return true;
    }

    return false;
}

static void update_long_timer_cb(lv_timer_t* timer)
{
    static int fetch_state = 0;
    bool       result      = false;

    if(ui_is_sleeping())
        return;

    if(fetch_state == 0)
    {
        result = fetch_climate();
        if(result)
            fetch_state = 1;
    }
    else if(fetch_state == 1)
    {
        result = update_climate_chart();
        if(result)
            fetch_state = 2;
    }
    else if(fetch_state == 2)
    {
        result = fetch_solar();
        if(result)
            fetch_state = 3;
    }
    else if(fetch_state == 3)
    {
        result = update_solar_chart();
        if(result)
            fetch_state = 4;
    }
    else if(fetch_state == 4)
    {
        result = fetch_battery_consumption();
        if(result)
            fetch_state = 5;
    }
    else if(fetch_state == 5)
    {
        result = update_energy_chart();
        if(result)
            fetch_state = 0;
    }
}

/**
 * Timer callback to update energy and temperature data
 */
static void update_timer_cb(lv_timer_t* timer)
{
    bool result = request_data_fetch(FETCH_CLIMATE_INSIDE);
    if(!result)
    {
        log_warning("Failed to request climate data fetch");
    }
    result = request_data_fetch(FETCH_SMART_SOLAR);
    if(!result)
    {
        log_warning("Failed to request smart_solardata fetch");
    }
    result = request_data_fetch(FETCH_SMART_SHUNT);
    if(!result)
    {
        log_warning("Failed to request smart_shunt data fetch");
    }

    climate_sensor_t* climate_data = get_inside_climate_data();
    if(temperature_label != NULL)
    {
        lv_label_set_text_fmt(temperature_label, "%.1f °C", climate_data->temperature);
    }

    if(humidity_label != NULL)
    {
        lv_label_set_text_fmt(humidity_label, "%.1f%%", climate_data->humidity);
    }

    smart_shunt_t* shunt_data = get_smart_shunt_data();

    // Update battery energy label
    if(power_label != NULL)
    {
        lv_label_set_text_fmt(power_label, "%.1f W", shunt_data->current * shunt_data->voltage);
    }
    if(battery_status_label != NULL)
    {
        lv_label_set_text_fmt(battery_status_label, "%.2f %%", shunt_data->soc);
    }

    smart_solar_t* solar_data = get_smart_solar_data();

    // Update battery energy label
    if(solar_power_label != NULL)
    {
        lv_label_set_text_fmt(solar_power_label, "%.0f W", solar_data->solar_power);
    }

    if(solar_state_label != NULL)
    {
        lv_label_set_text_fmt(solar_state_label, "%s", solar_data->charge_state);
    }
}

void create_temperature_container(lv_obj_t* right_column)
{
    // Temperature and humidity section - container with flex layout
    lv_obj_t* temp_humid_container = lv_obj_create(right_column);
    lv_obj_set_size(temp_humid_container, LV_PCT(100), 160);
    lv_obj_set_style_border_width(temp_humid_container, 0, 0);
    lv_obj_set_style_radius(temp_humid_container, 0, 0);
    lv_obj_set_style_pad_all(temp_humid_container, 5, 0);
    lv_obj_set_scrollbar_mode(temp_humid_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(temp_humid_container, LV_OBJ_FLAG_SCROLLABLE);

    // Use row layout for the container
    lv_obj_set_flex_flow(temp_humid_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp_humid_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // Left column for labels
    lv_obj_t* labels_column = lv_obj_create(temp_humid_container);
    lv_obj_set_size(labels_column, LV_PCT(20), LV_PCT(100));
    lv_obj_set_style_border_width(labels_column, 0, 0);
    lv_obj_set_style_radius(labels_column, 0, 0);
    lv_obj_set_style_pad_all(labels_column, 0, 0);
    lv_obj_set_flex_flow(labels_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(labels_column, 5, 0);
    lv_obj_set_flex_align(labels_column, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(labels_column, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(labels_column, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* temperature_caption = lv_label_create(labels_column);
    lv_label_set_text(temperature_caption, "Temperature");

    // Temperature value with larger font (removed caption)
    temperature_label = lv_label_create(labels_column);
    lv_label_set_text(temperature_label, "--- °C");
    lv_obj_set_style_text_font(temperature_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(temperature_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(temperature_label, LV_PCT(100));

    // Spacer
    lv_obj_t* spacer = lv_obj_create(labels_column);
    lv_obj_set_height(spacer, 10);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_0, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_scrollbar_mode(spacer, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    // Humidity caption
    lv_obj_t* humidity_caption = lv_label_create(labels_column);
    lv_label_set_text(humidity_caption, "Humidity");

    // Humidity value with larger font (removed caption)
    humidity_label = lv_label_create(labels_column);
    lv_label_set_text(humidity_label, "--- %");
    lv_obj_set_style_text_font(humidity_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(humidity_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(humidity_label, LV_PCT(100));

    // Right column for chart
    lv_obj_t* chart_container = lv_obj_create(temp_humid_container);
    lv_obj_set_size(chart_container, LV_PCT(80), LV_PCT(100));
    lv_obj_set_style_border_width(chart_container, 0, 0);
    lv_obj_set_style_radius(chart_container, 0, 0);
    lv_obj_set_style_pad_all(chart_container, 5, 0);
    lv_obj_set_scrollbar_mode(chart_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(chart_container, LV_OBJ_FLAG_SCROLLABLE);

    // Add title to the chart
    lv_obj_t* chart_title = lv_label_create(chart_container);
    lv_label_set_text(chart_title, "Hourly Temperature (°C)");
    lv_obj_set_style_pad_all(chart_title, -5, 0);
    lv_obj_align(chart_title, LV_ALIGN_TOP_MID, 0, 0);

    // Create a chart
    chart = lv_chart_create(chart_container);
    lv_obj_set_size(chart, LV_PCT(95), LV_PCT(80));
    lv_obj_center(chart);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(chart, 5, 7);

    // Disable scrolling and scrollbar on the chart
    lv_obj_clear_flag(chart, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(chart, LV_SCROLLBAR_MODE_OFF);

    // Fix: Set proper size for chart indicators (points on the line)
    lv_obj_set_style_size(chart, 4, 4, LV_PART_INDICATOR); // Width and height for points

    // Range setup
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 40); // Temperature: 0-40°C

    // Set proper number of division lines for temperature scale
    lv_chart_set_div_line_count(chart, 4, 7); // 4 horizontal lines = 5 sections (0,10,20,30,40)

    // Set point count - increased to 48 hours
    lv_chart_set_point_count(chart, 48); // 48 data points (hourly for past 48 hours)

    // Add data series
    temp_series     = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED),
                                          LV_CHART_AXIS_PRIMARY_Y); // Max temp in red
    temp_min_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE),
                                          LV_CHART_AXIS_PRIMARY_Y); // Min temp in blue

    lv_chart_refresh(chart); // Required after direct set
}

/**
 * Creates container with battery power and hourly energy chart
 */
void create_energy_container(lv_obj_t* right_column)
{
    // Main container for battery energy and hourly chart
    lv_obj_t* energy_container = lv_obj_create(right_column);
    lv_obj_set_size(energy_container, LV_PCT(100), 160); // Same height as temperature container
    lv_obj_set_style_border_width(energy_container, 0, 0);
    lv_obj_set_style_radius(energy_container, 0, 0);
    lv_obj_set_style_pad_all(energy_container, 5, 0);
    lv_obj_set_scrollbar_mode(energy_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(energy_container, LV_OBJ_FLAG_SCROLLABLE);

    // Use row layout for the container
    lv_obj_set_flex_flow(energy_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(energy_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // Left column for battery energy display
    lv_obj_t* power_column = lv_obj_create(energy_container);
    lv_obj_set_size(power_column, LV_PCT(20), LV_PCT(100));
    lv_obj_set_style_border_width(power_column, 0, 0);
    lv_obj_set_style_radius(power_column, 0, 0);
    lv_obj_set_style_pad_all(power_column, 5, 0);
    lv_obj_set_flex_flow(power_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(power_column, 5, 0);
    lv_obj_set_flex_align(power_column, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(power_column, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(power_column, LV_OBJ_FLAG_SCROLLABLE);

    // Caption label
    lv_obj_t* power_caption = lv_label_create(power_column);
    lv_label_set_text(power_caption, "Power");

    // Battery power value label
    power_label = lv_label_create(power_column);
    lv_label_set_text(power_label, "--- W");
    lv_obj_set_style_text_font(power_label, &lv_font_montserrat_20, 0);

    // Spacer
    lv_obj_t* spacer = lv_obj_create(power_column);
    lv_obj_set_height(spacer, 10);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_0, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_scrollbar_mode(spacer, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    // Caption label
    lv_obj_t* status_caption = lv_label_create(power_column);
    lv_label_set_text(status_caption, "Status");

    // Battery status value label
    battery_status_label = lv_label_create(power_column);
    lv_label_set_text(battery_status_label, "-- %");
    lv_obj_set_style_text_font(battery_status_label, &lv_font_montserrat_20, 0);

    // Right column for hourly energy chart
    lv_obj_t* hourly_chart_container = lv_obj_create(energy_container);
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
    lv_obj_t* chart_title = lv_label_create(hourly_chart_container);
    lv_label_set_text(chart_title, "Hourly Battery Consumption (Ah)");
    lv_obj_set_style_pad_all(chart_title, -5, 0);
    lv_obj_align(chart_title, LV_ALIGN_TOP_MID, 0, 0);

    // Set range for the chart to show consumption (always positive)
    lv_chart_set_range(energy_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 20);

    // Add data series for hourly energy
    hourly_energy_series =
        lv_chart_add_series(energy_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    // Set the chart type to bar
    lv_chart_set_type(energy_chart, LV_CHART_TYPE_BAR);

    lv_chart_refresh(energy_chart); // Force refresh after all points are set
}

/**
 * Creates container with solar power and hourly energy chart
 */
void create_solar_container(lv_obj_t* right_column)
{
    // Main container for solar energy and hourly chart
    lv_obj_t* solar_container = lv_obj_create(right_column);
    lv_obj_set_size(solar_container, LV_PCT(100), 160);
    lv_obj_set_style_border_width(solar_container, 0, 0);
    lv_obj_set_style_radius(solar_container, 0, 0);
    lv_obj_set_style_pad_all(solar_container, 5, 0);
    lv_obj_set_scrollbar_mode(solar_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(solar_container, LV_OBJ_FLAG_SCROLLABLE);

    // Use row layout for the container
    lv_obj_set_flex_flow(solar_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(solar_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // Left column for solar energy display
    lv_obj_t* power_column = lv_obj_create(solar_container);
    lv_obj_set_size(power_column, LV_PCT(20), LV_PCT(100));
    lv_obj_set_style_border_width(power_column, 0, 0);
    lv_obj_set_style_radius(power_column, 0, 0);
    lv_obj_set_style_pad_all(power_column, 5, 0);
    lv_obj_set_flex_flow(power_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(power_column, 5, 0);
    lv_obj_set_flex_align(power_column, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(power_column, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(power_column, LV_OBJ_FLAG_SCROLLABLE);

    // Caption label
    lv_obj_t* power_caption = lv_label_create(power_column);
    lv_label_set_text(power_caption, "Power");

    // Battery power value label
    solar_power_label = lv_label_create(power_column);
    lv_label_set_text(solar_power_label, "--- W");
    lv_obj_set_style_text_font(solar_power_label, &lv_font_montserrat_20, 0);

    // Spacer
    lv_obj_t* spacer = lv_obj_create(power_column);
    lv_obj_set_height(spacer, 10);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_0, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_scrollbar_mode(spacer, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    // Caption label
    lv_obj_t* solar_state_caption = lv_label_create(power_column);
    lv_label_set_text(solar_state_caption, "State");

    // Battery power value label
    solar_state_label = lv_label_create(power_column);
    lv_label_set_text(solar_state_label, "---");
    lv_obj_set_style_text_font(solar_state_label, &lv_font_montserrat_20, 0);

    // Right column for hourly energy chart
    lv_obj_t* hourly_chart_container = lv_obj_create(solar_container);
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
    lv_obj_t* chart_title = lv_label_create(hourly_chart_container);
    lv_label_set_text(chart_title, "Hourly Solar Energy (Wh)");
    lv_obj_set_style_pad_all(chart_title, -5, 0);
    lv_obj_align(chart_title, LV_ALIGN_TOP_MID, 0, 0);

    // Set range for the chart to ensure all data points are visible
    lv_chart_set_range(solar_energy_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 300);

    // Add data series for hourly energy
    solar_hourly_energy_series = lv_chart_add_series(
        solar_energy_chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);

    lv_chart_refresh(solar_energy_chart); // Force refresh after all points are set
}

/**
 * Creates energy and temperature panel in the provided parent container
 */
void create_energy_temp_panel(lv_obj_t* right_column)
{
    // Use column layout
    lv_obj_set_flex_flow(right_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right_column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(right_column, 8, 0);
    lv_obj_set_scrollbar_mode(right_column, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(right_column, LV_OBJ_FLAG_SCROLLABLE);

    create_temperature_container(right_column);
    create_energy_container(right_column);
    create_solar_container(right_column);

    // Create a timer to update the values periodically
    update_timer      = lv_timer_create(update_timer_cb, 5000, NULL);
    update_long_timer = lv_timer_create(update_long_timer_cb, 3000, NULL);
    log_info("Energy and temperature panel created");
}

/**
 * Cleanup resources used by the energy and temperature panel
 */
void energy_temp_panel_cleanup(void)
{
    if(update_timer != NULL)
    {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }

    // Reset all static UI element pointers
    temperature_label = NULL;
    humidity_label    = NULL;
    chart             = NULL;
    temp_series       = NULL;
    temp_min_series   = NULL;

    // Reset energy container elements
    power_label          = NULL;
    battery_status_label = NULL;
    energy_chart         = NULL;
    hourly_energy_series = NULL;

    solar_power_label          = NULL;
    solar_state_label          = NULL;
    solar_hourly_energy_series = NULL;
    solar_energy_chart         = NULL;

    log_info("Energy and temperature panel cleaned up");
}
