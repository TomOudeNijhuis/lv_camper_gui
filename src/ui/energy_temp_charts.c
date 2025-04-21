#include <stdbool.h>
#include <math.h>
#include "lvgl/lvgl.h"
#include "energy_temp_panel.h"
#include "../lib/logger.h"
#include "../data/data_manager.h"
#include "ui.h"
#include "../main.h"

// Chart update functions
void update_climate_chart_with_history(entity_history_t* history_data, bool is_internal);
void refresh_climate_chart(void);
void reset_climate_chart(void);
bool update_energy_chart_with_history(entity_history_t* history_data);
bool update_solar_chart_with_history(entity_history_t* history_data);

// Data fetching functions
bool fetch_internal_climate(void);
bool fetch_external_climate(void);
bool fetch_battery_consumption(void);
bool fetch_solar(void);

// Combined fetch and update functions
bool update_internal_climate_chart(void);
bool update_external_climate_chart(void);
bool update_energy_chart(void);
bool update_solar_chart(void);

static lv_chart_series_t* internal_temp_series = NULL;
static lv_chart_series_t* external_temp_series = NULL;
static lv_obj_t*          chart                = NULL;

static lv_chart_series_t* hourly_energy_series = NULL;
static lv_obj_t*          energy_chart         = NULL;

static lv_chart_series_t* solar_hourly_energy_series = NULL;
static lv_obj_t*          solar_energy_chart         = NULL;

// Add static variables to store temperature history data
static float internal_temp_data[48] = {0};
static float external_temp_data[48] = {0};
static int   temp_data_count        = 0;
static bool  internal_data_valid    = false;
static bool  external_data_valid    = false;

// Add static variables for min/max lines and labels
static lv_obj_t*          internal_max_line  = NULL;
static lv_obj_t*          external_max_line  = NULL;
static lv_obj_t*          internal_max_label = NULL;
static lv_obj_t*          external_max_label = NULL;
static lv_point_precise_t internal_max_line_points[2];
static lv_point_precise_t external_max_line_points[2];

// Split the update_climate_chart_with_history to handle one source at a time
void update_climate_chart_with_history(entity_history_t* history_data, bool is_internal)
{
    if(chart == NULL || history_data == NULL || !history_data->valid)
        return;

    // Store the data in our static arrays
    int data_count = history_data->count;
    if(data_count > 48)
        data_count = 48;

    // Copy mean values to appropriate array
    if(is_internal)
    {
        for(int i = 0; i < data_count; i++)
        {
            internal_temp_data[i] = history_data->mean[i];
        }
        internal_data_valid = true;
        temp_data_count     = data_count;
    }
    else
    {
        for(int i = 0; i < data_count; i++)
        {
            external_temp_data[i] = history_data->mean[i];
        }
        external_data_valid = true;
    }

    // Refresh chart if at least one data source is valid
    if(internal_data_valid || external_data_valid)
    {
        refresh_climate_chart();
    }
}

// New function to render the chart with both data sets
void refresh_climate_chart(void)
{
    // Only need one valid data source to show the chart
    if(chart == NULL || (!internal_data_valid && !external_data_valid) || temp_data_count == 0)
        return;

    // Clear existing data
    lv_chart_set_all_value(chart, internal_temp_series, LV_CHART_POINT_NONE);
    lv_chart_set_all_value(chart, external_temp_series, LV_CHART_POINT_NONE);

    // Get the point count in the chart
    uint16_t point_count = lv_chart_get_point_count(chart);

    // Find the overall min and max temperature values
    float min_temp_overall = 100.0f;  // Start with high value
    float max_temp_overall = -100.0f; // Start with low value
    bool  range_set        = false;

    // Find min/max for each series
    float internal_max_temp = -100.0f;
    float external_max_temp = -100.0f;

    // Consider internal temperature values if valid
    if(internal_data_valid)
    {
        for(int i = 0; i < temp_data_count; i++)
        {
            if(internal_temp_data[i] < min_temp_overall)
                min_temp_overall = internal_temp_data[i];
            if(internal_temp_data[i] > max_temp_overall)
                max_temp_overall = internal_temp_data[i];
            if(internal_temp_data[i] > internal_max_temp)
                internal_max_temp = internal_temp_data[i];
        }
        range_set = true;
    }

    // Consider external temperature values if valid
    if(external_data_valid)
    {
        for(int i = 0; i < temp_data_count; i++)
        {
            if(external_temp_data[i] < min_temp_overall)
                min_temp_overall = external_temp_data[i];
            if(external_temp_data[i] > max_temp_overall)
                max_temp_overall = external_temp_data[i];
            if(external_temp_data[i] > external_max_temp)
                external_max_temp = external_temp_data[i];
        }
        range_set = true;
    }

    // If no range was set, use default range
    if(!range_set)
    {
        min_temp_overall = 15.0f;
        max_temp_overall = 25.0f;
    }

    // Add a 10% padding to the range to avoid data points touching the edges
    float range_padding = (max_temp_overall - min_temp_overall) * 0.1f;
    if(range_padding < 2.0f)
        range_padding = 2.0f; // At least 2 degrees padding

    float y_min = floor(min_temp_overall - range_padding);
    float y_max = ceil(max_temp_overall + range_padding);

    // Update the chart's Y-axis range with fixed-point values (multiplied by 10)
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, (int16_t)(y_min * 10),
                       (int16_t)(y_max * 10));

    // Remove previous lines and labels if they exist - SAFER VERSION
    // First, collect objects to delete in an array
    lv_obj_t* objects_to_delete[10] = {NULL}; // Adjust size if needed
    int       delete_count          = 0;

    uint32_t child_cnt = lv_obj_get_child_cnt(chart);
    for(uint32_t i = 0; i < child_cnt; i++)
    {
        lv_obj_t* child = lv_obj_get_child(chart, i);
        if(child &&
           (lv_obj_check_type(child, &lv_line_class) || lv_obj_check_type(child, &lv_label_class)))
        {
            if(delete_count < 10)
            { // Ensure we don't overflow our array
                objects_to_delete[delete_count++] = child;
            }
        }
    }

    // Now safely delete the collected objects
    for(int i = 0; i < delete_count; i++)
    {
        if(objects_to_delete[i])
        {
            lv_obj_del(objects_to_delete[i]);
        }
    }

    // Reset pointers after deletion
    internal_max_line  = NULL;
    external_max_line  = NULL;
    internal_max_label = NULL;
    external_max_label = NULL;

    // Fill chart with available data
    for(int i = 0; i < point_count && i < temp_data_count; i++)
    {
        // Convert data from history to chart format
        // History data typically comes newest first, so we reverse it
        int idx = temp_data_count - i - 1;
        if(idx >= 0)
        {
            // Internal temperature if valid
            if(internal_data_valid)
            {
                int internal_temp = (int)(internal_temp_data[idx] * 10); // Convert to fixed point
                lv_chart_set_next_value(chart, internal_temp_series, internal_temp);
            }
            else
            {
                lv_chart_set_next_value(chart, internal_temp_series, LV_CHART_POINT_NONE);
            }

            // External temperature if valid
            if(external_data_valid)
            {
                int external_temp = (int)(external_temp_data[idx] * 10); // Convert to fixed point
                lv_chart_set_next_value(chart, external_temp_series, external_temp);
            }
            else
            {
                lv_chart_set_next_value(chart, external_temp_series, LV_CHART_POINT_NONE);
            }
        }
    }

    // Get chart content dimensions
    lv_coord_t chart_w = lv_obj_get_content_width(chart);
    lv_coord_t chart_h = lv_obj_get_content_height(chart);

    // Calculate points for the data area
    float x_start = 1;
    float x_end   = chart_w - 1;

    // Add max value lines and labels for valid series
    if(internal_data_valid && internal_max_temp > -100.0f)
    {
        // Calculate y position for internal max line
        float internal_max_ratio = (internal_max_temp - y_min) / (y_max - y_min);
        float internal_max_y_pos = chart_h - (internal_max_ratio * chart_h);

        // Create max line for internal temperature
        internal_max_line             = lv_line_create(chart);
        internal_max_line_points[0].x = x_start;
        internal_max_line_points[0].y = internal_max_y_pos;
        internal_max_line_points[1].x = x_end;
        internal_max_line_points[1].y = internal_max_y_pos;

        lv_line_set_points(internal_max_line, internal_max_line_points, 2);
        lv_obj_set_style_line_width(internal_max_line, 1, 0);
        lv_obj_set_style_line_color(internal_max_line, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_line_dash_width(internal_max_line, 3, 0);
        lv_obj_set_style_line_dash_gap(internal_max_line, 3, 0);

        // Add max value label for internal temperature
        internal_max_label = lv_label_create(chart);
        lv_obj_set_style_text_font(internal_max_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(internal_max_label, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text_fmt(internal_max_label, "%.1f°C", internal_max_temp);
        lv_obj_align(internal_max_label, LV_ALIGN_TOP_LEFT, 5, -8);
    }

    if(external_data_valid && external_max_temp > -100.0f)
    {
        // Calculate y position for external max line
        float external_max_ratio = (external_max_temp - y_min) / (y_max - y_min);
        float external_max_y_pos = chart_h - (external_max_ratio * chart_h);

        // Create max line for external temperature
        external_max_line             = lv_line_create(chart);
        external_max_line_points[0].x = x_start;
        external_max_line_points[0].y = external_max_y_pos;
        external_max_line_points[1].x = x_end;
        external_max_line_points[1].y = external_max_y_pos;

        lv_line_set_points(external_max_line, external_max_line_points, 2);
        lv_obj_set_style_line_width(external_max_line, 1, 0);
        lv_obj_set_style_line_color(external_max_line, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_obj_set_style_line_dash_width(external_max_line, 3, 0);
        lv_obj_set_style_line_dash_gap(external_max_line, 3, 0);

        // Add max value label for external temperature
        external_max_label = lv_label_create(chart);
        lv_obj_set_style_text_font(external_max_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(external_max_label, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_label_set_text_fmt(external_max_label, "%.1f°C", external_max_temp);
        lv_obj_align(external_max_label, LV_ALIGN_TOP_RIGHT, -5, -8);
    }

    // Refresh the chart to show new data
    lv_chart_refresh(chart);

    log_debug("Climate chart updated with %d temperature points (range: %.1f-%.1f°C)",
              temp_data_count, min_temp_overall, max_temp_overall);
}

// Split the fetch process into two separate functions
bool fetch_internal_climate(void)
{
    bool result = request_entity_history("inside", "temperature", "1h", 48);
    if(!result)
    {
        log_warning("Failed to request internal temperature history data");
    }
    else
    {
        log_debug("Requested internal temperature history data");
    }
    return result;
}

bool fetch_external_climate(void)
{
    bool result = request_entity_history("outside", "temperature", "1h", 48);
    if(!result)
    {
        log_warning("Failed to request external temperature history data");
    }
    else
    {
        log_debug("Requested external temperature history data");
    }
    return result;
}

// Process internal climate data
bool update_internal_climate_chart(void)
{
    // Check if historical data is available
    entity_history_t* history_data = get_entity_history_data();
    if(history_data != NULL && history_data->valid)
    {
        update_climate_chart_with_history(history_data, true);
        free_entity_history_data(history_data);
        return true;
    }

    return false;
}

// Process external climate data
bool update_external_climate_chart(void)
{
    // Check if historical data is available
    entity_history_t* history_data = get_entity_history_data();
    if(history_data != NULL && history_data->valid)
    {
        update_climate_chart_with_history(history_data, false);
        free_entity_history_data(history_data);
        return true;
    }

    return false;
}

// Reset the chart if data is invalid
void reset_climate_chart(void)
{
    if(chart != NULL && internal_temp_series != NULL && external_temp_series != NULL)
    {
        lv_chart_set_all_value(chart, internal_temp_series, LV_CHART_POINT_NONE);
        lv_chart_set_all_value(chart, external_temp_series, LV_CHART_POINT_NONE);
        lv_chart_refresh(chart);

        // Remove any labels if they exist
        lv_obj_t* children = lv_obj_get_child(chart, 0);
        while(children)
        {
            if(lv_obj_check_type(children, &lv_label_class))
            {
                lv_obj_del(children);
            }
            children = lv_obj_get_child(chart, lv_obj_get_index(children) + 1);
        }

        // Reset data validity flags
        internal_data_valid = false;
        external_data_valid = false;
    }
}

bool update_energy_chart_with_history(entity_history_t* history_data)
{
    if(energy_chart != NULL && hourly_energy_series != NULL && history_data != NULL &&
       history_data->valid)
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
        lv_chart_set_range(energy_chart, LV_CHART_AXIS_PRIMARY_Y, 0, range_max * 10);

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
                int ah_value =
                    (hourly_ah[idx] <= 0.1f)
                        ? 10
                        : (int)(hourly_ah[idx] * 10); // Convert to fixed point with 1 decimal
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
    if(history_data != NULL && history_data->valid)
    {
        update_energy_chart_with_history(history_data);
        free_entity_history_data(history_data);
        return true;
    }

    // Clear chart when data is invalid
    if(energy_chart != NULL && hourly_energy_series != NULL)
    {
        lv_chart_set_all_value(energy_chart, hourly_energy_series, LV_CHART_POINT_NONE);
        lv_chart_refresh(energy_chart);

        // Reset max value label if it exists
        lv_obj_t* children = lv_obj_get_child(energy_chart, 0);
        while(children)
        {
            if(lv_obj_check_type(children, &lv_label_class))
            {
                lv_obj_del(children);
            }
            children = lv_obj_get_child(energy_chart, lv_obj_get_index(children) + 1);
        }
    }

    return false;
}

bool update_solar_chart_with_history(entity_history_t* history_data)
{
    if(solar_energy_chart != NULL && solar_hourly_energy_series != NULL && history_data != NULL &&
       history_data->valid)
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
        lv_chart_set_range(solar_energy_chart, LV_CHART_AXIS_PRIMARY_Y, 0, range_max * 10);

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
                    int yield_value =
                        (int)(hourly_yield[idx] * 10); // Convert to fixed point with 1 decimal
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
    if((history_data != NULL) && (history_data->count > 0) && history_data->valid)
    {
        update_solar_chart_with_history(history_data);
        free_entity_history_data(history_data);

        return true;
    }

    // Clear chart when data is invalid
    if(solar_energy_chart != NULL && solar_hourly_energy_series != NULL)
    {
        lv_chart_set_all_value(solar_energy_chart, solar_hourly_energy_series, LV_CHART_POINT_NONE);
        lv_chart_refresh(solar_energy_chart);
    }

    return false;
}

void update_long_timer_cb(lv_timer_t* timer)
{
    static int fetch_state    = 0;
    bool       result         = false;
    static int fetch_interval = 0;
    static int fetch_valid    = 0;

    if(ui_is_sleeping())
    {
        fetch_state    = 0;
        fetch_interval = 0;
        return;
    }

    if(fetch_interval > 0)
    {
        fetch_interval -= 1;
        return;
    }

    if(fetch_state == 0)
    {
        result = fetch_internal_climate();
        if(result)
            fetch_valid = 1;

        fetch_state = 1;
    }
    else if(fetch_state == 1)
    {
        result = update_internal_climate_chart();
        if(result)
            fetch_valid += 1;

        fetch_state = 2;
    }
    else if(fetch_state == 2)
    {
        result = fetch_external_climate();
        if(result)
            fetch_valid += 1;

        fetch_state = 3;
    }
    else if(fetch_state == 3)
    {
        result = update_external_climate_chart();
        if(result)
            fetch_valid += 1;

        fetch_state = 4;
    }
    else if(fetch_state == 4)
    {
        result = fetch_solar();
        if(result)
            fetch_valid += 1;

        fetch_state = 5;
    }
    else if(fetch_state == 5)
    {
        result = update_solar_chart();
        if(result)
            fetch_valid += 1;

        fetch_state = 6;
    }
    else if(fetch_state == 6)
    {
        result = fetch_battery_consumption();
        if(result)
            fetch_valid += 1;

        fetch_state = 7;
    }
    else if(fetch_state == 7)
    {
        result = update_energy_chart();
        if(result)
            fetch_valid += 1;

        fetch_state = 0;
        fetch_valid = 0;
        if(fetch_valid >= 5)
            fetch_interval = 5;
        else
            fetch_interval = 0;
    }
}

void initialize_temperature_chart(lv_obj_t* chart_container)
{
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

    // Range setup - with fixed point (multiplied by 10)
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 400); // Temperature: 0-40°C (x10)

    // Set proper number of division lines for temperature scale
    lv_chart_set_div_line_count(chart, 4, 7); // 4 horizontal lines = 5 sections (0,10,20,30,40)

    // Set point count - 48 hours
    lv_chart_set_point_count(chart, 48); // 48 data points (hourly for past 48 hours)

    // Add data series for internal and external temperatures
    internal_temp_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED),
                                               LV_CHART_AXIS_PRIMARY_Y); // Internal temp in red
    external_temp_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE),
                                               LV_CHART_AXIS_PRIMARY_Y); // External temp in blue

    lv_chart_refresh(chart); // Required after direct set
}

void initialize_energy_chart(lv_obj_t* chart_container)
{
    // Create a chart for hourly energy
    energy_chart = lv_chart_create(chart_container);
    lv_obj_set_size(energy_chart, LV_PCT(95), LV_PCT(80));
    lv_obj_center(energy_chart);
    lv_chart_set_type(energy_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_div_line_count(energy_chart, 5, 7);
    lv_obj_set_style_pad_column(energy_chart, 2, 0);
    lv_chart_set_point_count(energy_chart, 48);

    // Add title to the chart - indicate fixed-point values
    lv_obj_t* chart_title = lv_label_create(chart_container);
    lv_label_set_text(chart_title, "Hourly Battery Consumption (Ah)");
    lv_obj_set_style_pad_all(chart_title, -5, 0);
    lv_obj_align(chart_title, LV_ALIGN_TOP_MID, 0, 0);

    // Set range for the chart to show consumption (always positive) - use fixed point
    lv_chart_set_range(energy_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 200); // 0-20 Ah (x10)

    // Add data series for hourly energy
    hourly_energy_series =
        lv_chart_add_series(energy_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    // Set the chart type to bar
    lv_chart_set_type(energy_chart, LV_CHART_TYPE_BAR);

    lv_chart_refresh(energy_chart); // Force refresh after all points are set
}

void initialize_solar_chart(lv_obj_t* chart_container)
{
    // Create a chart for hourly energy
    solar_energy_chart = lv_chart_create(chart_container);
    lv_obj_set_size(solar_energy_chart, LV_PCT(95), LV_PCT(80));
    lv_obj_center(solar_energy_chart);
    lv_chart_set_type(solar_energy_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_div_line_count(solar_energy_chart, 5, 7);
    lv_obj_set_style_pad_column(solar_energy_chart, 2, 0);
    lv_chart_set_point_count(solar_energy_chart, 48);

    // Add title to the chart - indicate fixed-point values
    lv_obj_t* chart_title = lv_label_create(chart_container);
    lv_label_set_text(chart_title, "Hourly Solar Energy (Wh)");
    lv_obj_set_style_pad_all(chart_title, -5, 0);
    lv_obj_align(chart_title, LV_ALIGN_TOP_MID, 0, 0);

    // Set range for the chart to ensure all data points are visible - use fixed point
    lv_chart_set_range(solar_energy_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 3000); // 0-300 Wh (x10)

    // Add data series for hourly energy
    solar_hourly_energy_series = lv_chart_add_series(
        solar_energy_chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);

    lv_chart_refresh(solar_energy_chart); // Force refresh after all points are set
}

void chart_cleanup()
{
    chart                      = NULL;
    internal_temp_series       = NULL;
    external_temp_series       = NULL;
    energy_chart               = NULL;
    hourly_energy_series       = NULL;
    solar_hourly_energy_series = NULL;
    solar_energy_chart         = NULL;

    // Reset data validity flags
    internal_data_valid = false;
    external_data_valid = false;
}