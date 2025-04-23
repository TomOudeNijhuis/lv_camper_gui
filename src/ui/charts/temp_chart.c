#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "lvgl/lvgl.h"
#include "temp_chart.h"
#include "../energy_temp_panel.h"
#include "../../lib/logger.h"
#include "../../data/data_manager.h"
#include "../ui.h"
#include "../../main.h"

static lv_chart_series_t* internal_temp_series = NULL;
static lv_chart_series_t* external_temp_series = NULL;
static lv_obj_t*          chart                = NULL;

// Add static variables to store temperature history data
static float internal_temp_data[48] = {0};
static float external_temp_data[48] = {0};
static int   temp_data_count        = 0;
static bool  internal_data_valid    = false;
static bool  external_data_valid    = false;

// Add static variables for climate chart min/max lines and labels
static lv_obj_t* internal_max_line  = NULL;
static lv_obj_t* external_max_line  = NULL;
static lv_obj_t* internal_max_label = NULL;
static lv_obj_t* external_max_label = NULL;

static lv_obj_t*          internal_min_line  = NULL;
static lv_obj_t*          external_min_line  = NULL;
static lv_obj_t*          internal_min_label = NULL;
static lv_obj_t*          external_min_label = NULL;
static lv_point_precise_t internal_min_line_points[2];
static lv_point_precise_t internal_max_line_points[2];
static lv_point_precise_t external_max_line_points[2];
static lv_point_precise_t external_min_line_points[2];

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

    // Initialize data validity flags
    internal_data_valid = false;
    external_data_valid = false;
    temp_data_count     = 0;

    lv_chart_refresh(chart); // Required after direct set
}

// Add a new function to clean up all chart lines and labels
static void cleanup_temperature_chart_lines_and_labels(void)
{
    // Delete max lines and labels
    if(internal_max_line != NULL)
    {
        lv_obj_del(internal_max_line);
        internal_max_line = NULL;
    }

    if(external_max_line != NULL)
    {
        lv_obj_del(external_max_line);
        external_max_line = NULL;
    }

    if(internal_max_label != NULL)
    {
        lv_obj_del(internal_max_label);
        internal_max_label = NULL;
    }

    if(external_max_label != NULL)
    {
        lv_obj_del(external_max_label);
        external_max_label = NULL;
    }

    // Delete min lines and labels
    if(internal_min_line != NULL)
    {
        lv_obj_del(internal_min_line);
        internal_min_line = NULL;
    }

    if(external_min_line != NULL)
    {
        lv_obj_del(external_min_line);
        external_min_line = NULL;
    }

    if(internal_min_label != NULL)
    {
        lv_obj_del(internal_min_label);
        internal_min_label = NULL;
    }

    if(external_min_label != NULL)
    {
        lv_obj_del(external_min_label);
        external_min_label = NULL;
    }
}

void temp_chart_cleanup(void)
{
    // Use the dedicated cleanup function instead of repeating code
    cleanup_temperature_chart_lines_and_labels();

    // Note: chart and series will be deleted by LVGL when their parent is deleted
    chart                = NULL;
    internal_temp_series = NULL;
    external_temp_series = NULL;

    // Reset data validity flags
    internal_data_valid = false;
    external_data_valid = false;
    temp_data_count     = 0;

    log_debug("Temperature chart cleaned up");
}

// Split the update_climate_chart_with_history to handle one source at a time
void update_climate_chart_with_history(entity_history_t* history_data, bool is_internal)
{
    if(chart == NULL || history_data == NULL || !history_data->valid || history_data->mean == NULL)
    {
        log_warning("Invalid climate chart data received, skipping update");
        return;
    }

    // Store the data in our static arrays
    int data_count = history_data->count;
    if(data_count <= 0)
    {
        log_warning("Empty climate chart data received, skipping update");
        return;
    }

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
        temp_data_count     = data_count; // Update the count here
    }
    else
    {
        for(int i = 0; i < data_count; i++)
        {
            external_temp_data[i] = history_data->mean[i];
        }
        external_data_valid = true;
        if(temp_data_count < data_count) // Only update if larger than current
            temp_data_count = data_count;
    }

    // Refresh chart if at least one data source is valid
    if(internal_data_valid || external_data_valid)
    {
        log_debug("Refreshing climate chart with internal=%d, external=%d, count=%d",
                  internal_data_valid, external_data_valid, temp_data_count);
        refresh_climate_chart();
    }
}

// New function to render the chart with both data sets
void refresh_climate_chart(void)
{
    // If both data sources are invalid, reset the chart completely
    if(!internal_data_valid && !external_data_valid)
    {
        reset_climate_chart();
        return;
    }

    // Continue with existing refresh logic if at least one source is valid
    if(chart == NULL || temp_data_count == 0)
        return;

    // Safety check - ensure temp_data_count is reasonable
    if(temp_data_count > 48)
    {
        temp_data_count = 48;
        log_warning("Temperature data count was too high, capped at 48");
    }

    // Clear existing data
    if(internal_temp_series != NULL)
        lv_chart_set_all_value(chart, internal_temp_series, LV_CHART_POINT_NONE);
    if(external_temp_series != NULL)
        lv_chart_set_all_value(chart, external_temp_series, LV_CHART_POINT_NONE);

    // Ensure we have valid series
    if(internal_temp_series == NULL || external_temp_series == NULL)
    {
        log_warning("Temperature series is NULL, recreating chart series");
        // Recreate the series if they're NULL
        if(internal_temp_series == NULL)
        {
            internal_temp_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED),
                                                       LV_CHART_AXIS_PRIMARY_Y);
        }
        if(external_temp_series == NULL)
        {
            external_temp_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE),
                                                       LV_CHART_AXIS_PRIMARY_Y);
        }
    }

    // Get the point count in the chart
    uint16_t point_count = lv_chart_get_point_count(chart);

    // Find the overall min and max temperature values
    float min_temp_overall = 100.0f;  // Start with high value
    float max_temp_overall = -100.0f; // Start with low value
    bool  range_set        = false;

    // Find min/max for each series
    float internal_max_temp = -100.0f;
    float external_max_temp = -100.0f;
    float internal_min_temp = 100.0f;
    float external_min_temp = 100.0f;

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
            if(internal_temp_data[i] < internal_min_temp)
                internal_min_temp = internal_temp_data[i];
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
            if(external_temp_data[i] < external_min_temp)
                external_min_temp = external_temp_data[i];
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

    // Use the dedicated function to clean up all lines and labels
    cleanup_temperature_chart_lines_and_labels();

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
        internal_max_line = lv_line_create(chart);
        if(internal_max_line == NULL)
        {
            log_error("Failed to create internal max line");
            return;
        }

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
        if(internal_max_label == NULL)
        {
            log_error("Failed to create internal max label");
            return;
        }

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
        external_max_line = lv_line_create(chart);
        if(external_max_line == NULL)
        {
            log_error("Failed to create external max line");
            return;
        }

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
        if(external_max_label == NULL)
        {
            log_error("Failed to create external max label");
            return;
        }

        lv_obj_set_style_text_font(external_max_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(external_max_label, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_label_set_text_fmt(external_max_label, "%.1f°C", external_max_temp);
        lv_obj_align(external_max_label, LV_ALIGN_TOP_RIGHT, -5, -8);
    }

    // Add min value lines and labels for valid series
    if(internal_data_valid && internal_min_temp < 100.0f)
    {
        // Calculate y position for internal min line
        float internal_min_ratio = (internal_min_temp - y_min) / (y_max - y_min);
        float internal_min_y_pos = chart_h - (internal_min_ratio * chart_h);

        // Create min line for internal temperature
        internal_min_line = lv_line_create(chart);
        if(internal_min_line == NULL)
        {
            log_error("Failed to create internal min line");
            return;
        }

        internal_min_line_points[0].x = x_start;
        internal_min_line_points[0].y = internal_min_y_pos;
        internal_min_line_points[1].x = x_end;
        internal_min_line_points[1].y = internal_min_y_pos;

        lv_line_set_points(internal_min_line, internal_min_line_points, 2);
        lv_obj_set_style_line_width(internal_min_line, 1, 0);
        lv_obj_set_style_line_color(internal_min_line, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_line_dash_width(internal_min_line, 3, 0);
        lv_obj_set_style_line_dash_gap(internal_min_line, 3, 0);

        // Add min value label for internal temperature
        internal_min_label = lv_label_create(chart);
        if(internal_min_label == NULL)
        {
            log_error("Failed to create internal min label");
            return;
        }

        lv_obj_set_style_text_font(internal_min_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(internal_min_label, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text_fmt(internal_min_label, "%.1f°C", internal_min_temp);
        lv_obj_align(internal_min_label, LV_ALIGN_BOTTOM_LEFT, 5, 10);
    }

    if(external_data_valid && external_min_temp < 100.0f)
    {
        // Calculate y position for external min line
        float external_min_ratio = (external_min_temp - y_min) / (y_max - y_min);
        float external_min_y_pos = chart_h - (external_min_ratio * chart_h);

        // Create min line for external temperature
        external_min_line = lv_line_create(chart);
        if(external_min_line == NULL)
        {
            log_error("Failed to create external min line");
            return;
        }

        external_min_line_points[0].x = x_start;
        external_min_line_points[0].y = external_min_y_pos;
        external_min_line_points[1].x = x_end;
        external_min_line_points[1].y = external_min_y_pos;

        lv_line_set_points(external_min_line, external_min_line_points, 2);
        lv_obj_set_style_line_width(external_min_line, 1, 0);
        lv_obj_set_style_line_color(external_min_line, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_obj_set_style_line_dash_width(external_min_line, 3, 0);
        lv_obj_set_style_line_dash_gap(external_min_line, 3, 0);

        // Add min value label for external temperature
        external_min_label = lv_label_create(chart);
        if(external_min_label == NULL)
        {
            log_error("Failed to create external min label");
            return;
        }

        lv_obj_set_style_text_font(external_min_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(external_min_label, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_label_set_text_fmt(external_min_label, "%.1f°C", external_min_temp);
        lv_obj_align(external_min_label, LV_ALIGN_BOTTOM_RIGHT, -5, 10);
    }

    // Refresh the chart to show new data
    lv_chart_refresh(chart);

    log_debug("Climate chart updated with %d temperature points (range: %.1f-%.1f°C)",
              temp_data_count, min_temp_overall, max_temp_overall);
}

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

    // If data is NULL or invalid, clear internal data and refresh chart
    if(history_data == NULL || !history_data->valid || history_data->mean == NULL ||
       history_data->count <= 0 || history_data->sensor_name == NULL ||
       history_data->sensor_name[0] == '\0')
    {
        internal_data_valid = false;
        refresh_climate_chart();
        if(history_data != NULL)
            free_entity_history_data(history_data);
        log_warning("Invalid internal temperature data received, clearing chart");
        return false;
    }
    else if(strcmp(history_data->sensor_name, "inside") != 0)
    {
        log_warning("Invalid inside temperature data received, got '%s'",
                    history_data->sensor_name);
        return false;
    }

    // Process valid data
    update_climate_chart_with_history(history_data, true);
    free_entity_history_data(history_data);
    return true;
}

// Process external climate data
bool update_external_climate_chart(void)
{
    // Check if historical data is available
    entity_history_t* history_data = get_entity_history_data();

    // If data is NULL or invalid, clear external data and refresh chart
    if(history_data == NULL || !history_data->valid || history_data->mean == NULL ||
       history_data->count <= 0 || history_data->sensor_name == NULL ||
       history_data->sensor_name[0] == '\0')
    {
        external_data_valid = false;
        refresh_climate_chart();
        if(history_data != NULL)
            free_entity_history_data(history_data);
        log_warning("Invalid external temperature data received, clearing chart");
        return false;
    }
    else if(strcmp(history_data->sensor_name, "outside") != 0)
    {
        log_warning("Invalid outside temperature data received, got '%s'",
                    history_data->sensor_name);
        return false;
    }
    // Process valid data
    update_climate_chart_with_history(history_data, false);
    free_entity_history_data(history_data);
    return true;
}

// Reset the chart if data is invalid
void reset_climate_chart(void)
{
    if(chart == NULL)
        return;

    if(internal_temp_series != NULL)
        lv_chart_set_all_value(chart, internal_temp_series, LV_CHART_POINT_NONE);

    if(external_temp_series != NULL)
        lv_chart_set_all_value(chart, external_temp_series, LV_CHART_POINT_NONE);

    lv_chart_refresh(chart);

    // Use the dedicated cleanup function for lines and labels
    cleanup_temperature_chart_lines_and_labels();

    // Reset data validity flags
    internal_data_valid = false;
    external_data_valid = false;
    temp_data_count     = 0; // Also reset the data count!

    log_debug("Climate chart reset completely");
}