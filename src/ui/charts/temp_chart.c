#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "lvgl/lvgl.h"
#include "temp_chart.h"
#include "../energy_temp_panel.h"
#include "../../lib/logger.h"
#include "../../data/data_manager.h"
#include "../ui.h"
#include "../../main.h"

#define TEMP_CHART_POINTS 48

static lv_chart_series_t* internal_temp_series = NULL;
static lv_chart_series_t* external_temp_series = NULL;
static lv_obj_t*          chart                = NULL;

// Add static variables to store temperature history data
static int32_t internal_temp_data[TEMP_CHART_POINTS] = {0};
static int32_t external_temp_data[TEMP_CHART_POINTS] = {0};
static bool    internal_data_valid                   = false;
static bool    external_data_valid                   = false;

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

// Add static variables for timestamp display
static char      first_timestamp[32] = {0};
static char      last_timestamp[32]  = {0};
static lv_obj_t* start_time_label    = NULL;
static lv_obj_t* end_time_label      = NULL;
static bool      timestamps_valid    = false;

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

    lv_chart_set_point_count(chart, TEMP_CHART_POINTS);

    // Add data series for internal and external temperatures
    internal_temp_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN),
                                               LV_CHART_AXIS_PRIMARY_Y); // Internal temp in red
    external_temp_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE),
                                               LV_CHART_AXIS_PRIMARY_Y); // External temp in blue

    // Initialize data validity flags
    internal_data_valid = false;
    external_data_valid = false;

    // Create timestamp labels for the chart timeline
    start_time_label = lv_label_create(chart_container);
    lv_obj_set_style_text_font(start_time_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(start_time_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(start_time_label, LV_ALIGN_BOTTOM_LEFT, 5, 0);
    lv_label_set_text(start_time_label, "");

    end_time_label = lv_label_create(chart_container);
    lv_obj_set_style_text_font(end_time_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(end_time_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(end_time_label, LV_ALIGN_BOTTOM_RIGHT, -5, 0);
    lv_label_set_text(end_time_label, "");

    lv_chart_refresh(chart); // Required after direct set
}

// Update function to format timestamp with date
static void format_chart_timestamp(const char* iso_timestamp, char* output, size_t output_size)
{
    if(iso_timestamp == NULL || output == NULL ||
       output_size < 12) // Need more space for DD-MM HH:MM
    {
        strcpy(output, "");
        return;
    }

    // ISO format: YYYY-MM-DDThh:mm:ss
    if(strlen(iso_timestamp) >= 16 && iso_timestamp[4] == '-' && iso_timestamp[7] == '-' &&
       iso_timestamp[10] == 'T')
    {
        // Extract day (positions 8-9)
        char day[3] = {iso_timestamp[8], iso_timestamp[9], '\0'};

        // Extract month (positions 5-6)
        char month[3] = {iso_timestamp[5], iso_timestamp[6], '\0'};

        // Extract time (positions 11-16 for HH:MM)
        char time[6] = {iso_timestamp[11], iso_timestamp[12], ':',
                        iso_timestamp[14], iso_timestamp[15], '\0'};

        // Format as "DD-MM HH:MM"
        snprintf(output, output_size, "%s-%s %s", day, month, time);
    }
    else
    {
        // Fallback if timestamp isn't in expected format
        strncpy(output, iso_timestamp, output_size - 1);
        output[output_size - 1] = '\0';
    }
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

// Add a function to safely create a line with error handling
static lv_obj_t* create_temperature_line(lv_obj_t* parent, const char* line_name, lv_color_t color,
                                         lv_point_precise_t* points)
{
    if(parent == NULL || points == NULL)
    {
        log_error("Cannot create %s: Invalid parent or points", line_name);
        return NULL;
    }

    lv_obj_t* line = lv_line_create(parent);
    if(line == NULL)
    {
        log_error("Failed to create %s", line_name);
        return NULL;
    }

    lv_line_set_points(line, points, 2);
    lv_obj_set_style_line_width(line, 1, 0);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_dash_width(line, 3, 0);
    lv_obj_set_style_line_dash_gap(line, 3, 0);

    return line;
}

// Update create_temperature_label to use int32_t
static lv_obj_t* create_temperature_label(lv_obj_t* parent, const char* label_name,
                                          lv_color_t color, int32_t temperature_fixed,
                                          lv_align_t alignment, lv_coord_t x_offset,
                                          lv_coord_t y_offset)
{
    if(parent == NULL)
    {
        log_error("Cannot create %s: Invalid parent", label_name);
        return NULL;
    }

    lv_obj_t* label = lv_label_create(parent);
    if(label == NULL)
    {
        log_error("Failed to create %s", label_name);
        return NULL;
    }

    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label, color, 0);
    // Convert from fixed point (x10) to float for display
    float temperature = temperature_fixed / 10.0f;
    lv_label_set_text_fmt(label, "%.1f°C", temperature);
    lv_obj_align(label, alignment, x_offset, y_offset);

    return label;
}

void temp_chart_cleanup(void)
{
    // Use the dedicated cleanup function instead of repeating code
    cleanup_temperature_chart_lines_and_labels();

    // Clean up timestamp labels
    if(start_time_label != NULL)
    {
        lv_obj_del(start_time_label);
        start_time_label = NULL;
    }

    if(end_time_label != NULL)
    {
        lv_obj_del(end_time_label);
        end_time_label = NULL;
    }

    // Note: chart and series will be deleted by LVGL when their parent is deleted
    chart                = NULL;
    internal_temp_series = NULL;
    external_temp_series = NULL;

    // Reset data validity flags
    internal_data_valid = false;
    external_data_valid = false;
    timestamps_valid    = false;
    first_timestamp[0]  = '\0';
    last_timestamp[0]   = '\0';

    log_debug("Temperature chart cleaned up");
}

// Split the update_climate_chart_with_history to handle one source at a time
void update_climate_chart_with_history(entity_history_t* history_data, bool is_internal)
{
    int data_count = history_data->count;
    if(data_count > TEMP_CHART_POINTS)
    {
        data_count = TEMP_CHART_POINTS;
        log_warning("Data count is %d, capping to %d", history_data->count, TEMP_CHART_POINTS);
    }
    if(chart == NULL || history_data->mean == NULL || data_count <= 0)
    {
        if(is_internal)
        {
            internal_data_valid = false;
        }
        else
        {
            external_data_valid = false;
        }
        refresh_climate_chart();

        log_warning("Invalid climate chart data received, skipping update");
        return;
    }

    // Extract timestamps from history data (only need to do this once, using internal data)
    if(is_internal && history_data->timestamps != NULL && data_count > 0)
    {
        // First timestamp is oldest data (array is in reverse chronological order)
        strncpy(first_timestamp, history_data->timestamps[data_count - 1],
                sizeof(first_timestamp) - 1);
        // Last timestamp is newest data (index 0)
        strncpy(last_timestamp, history_data->timestamps[0], sizeof(last_timestamp) - 1);
        first_timestamp[sizeof(first_timestamp) - 1] = '\0';
        last_timestamp[sizeof(last_timestamp) - 1]   = '\0';
        timestamps_valid                             = true;
    }

    // Copy mean values to appropriate array
    if(is_internal)
    {
        // Copy available data
        for(int i = 0; i < data_count; i++)
        {
            internal_temp_data[i] = (int32_t)(history_data->mean[i] * 10);
        }

        // Fill remaining elements with LV_CHART_POINT_NONE
        for(int i = data_count; i < TEMP_CHART_POINTS; i++)
        {
            internal_temp_data[i] = LV_CHART_POINT_NONE;
        }

        internal_data_valid = true;
    }
    else
    {
        // Copy available data
        for(int i = 0; i < data_count; i++)
        {
            external_temp_data[i] = (int32_t)(history_data->mean[i] * 10);
        }

        // Fill remaining elements with LV_CHART_POINT_NONE
        for(int i = data_count; i < TEMP_CHART_POINTS; i++)
        {
            external_temp_data[i] = LV_CHART_POINT_NONE;
        }

        external_data_valid = true;
    }

    // Refresh chart if at least one data source is valid
    if(internal_data_valid || external_data_valid)
    {
        log_debug("Refreshing climate chart with internal=%d, external=%d", internal_data_valid,
                  external_data_valid);
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
    if(chart == NULL)
        return;

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
            internal_temp_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN),
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

    // Find the overall min and max temperature values (using fixed point)
    int32_t min_temp_overall = 1000;  // Start with high value (100.0°C)
    int32_t max_temp_overall = -1000; // Start with low value (-100.0°C)
    bool    range_set        = false;

    // Find min/max for each series (using fixed point)
    int32_t internal_max_temp = -1000;
    int32_t external_max_temp = -1000;
    int32_t internal_min_temp = 1000;
    int32_t external_min_temp = 1000;

    // Consider internal temperature values if valid
    if(internal_data_valid)
    {
        for(int i = 0; i < TEMP_CHART_POINTS; i++)
        {
            // Skip invalid/no-data points
            if(internal_temp_data[i] == LV_CHART_POINT_NONE)
                continue;

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
        for(int i = 0; i < TEMP_CHART_POINTS; i++)
        {
            // Skip invalid/no-data points
            if(external_temp_data[i] == LV_CHART_POINT_NONE)
                continue;

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
        min_temp_overall = 150; // 15.0°C in fixed point
        max_temp_overall = 250; // 25.0°C in fixed point
    }

    // Add a 10% padding to the range to avoid data points touching the edges
    int32_t range         = max_temp_overall - min_temp_overall;
    int32_t range_padding = range / 10; // 10% padding
    if(range_padding < 20)              // At least 2 degrees padding (20 in fixed point)
        range_padding = 20;

    int32_t y_min = min_temp_overall - range_padding;
    int32_t y_max = max_temp_overall + range_padding;

    // Update the chart's Y-axis range
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);

    // Use the dedicated function to clean up all lines and labels
    cleanup_temperature_chart_lines_and_labels();

    // Fill chart with available data
    for(int i = 0; i < point_count && i < TEMP_CHART_POINTS; i++)
    {
        // Convert data from history to chart format
        // History data typically comes newest first, so we reverse it
        int idx = TEMP_CHART_POINTS - i - 1;
        if(idx >= 0)
        {
            // Internal temperature if valid
            if(internal_data_valid)
                lv_chart_set_next_value(chart, internal_temp_series, internal_temp_data[idx]);
            else
                lv_chart_set_next_value(chart, internal_temp_series, LV_CHART_POINT_NONE);

            // External temperature if valid
            if(external_data_valid)
                lv_chart_set_next_value(chart, external_temp_series, external_temp_data[idx]);
            else
                lv_chart_set_next_value(chart, external_temp_series, LV_CHART_POINT_NONE);
        }
    }

    // Get chart content dimensions
    lv_coord_t chart_w = lv_obj_get_content_width(chart);
    lv_coord_t chart_h = lv_obj_get_content_height(chart);

    // Calculate points for the data area
    float x_start = 1;
    float x_end   = chart_w - 1;

    // Add max value lines and labels for valid series
    if(internal_data_valid && internal_max_temp > -1000)
    {
        // Calculate y position for internal max line using fixed point value
        float internal_max_ratio = (float)(internal_max_temp - y_min) / (y_max - y_min);
        float internal_max_y_pos = chart_h - (internal_max_ratio * chart_h);

        // Setup line points
        internal_max_line_points[0].x = x_start;
        internal_max_line_points[0].y = internal_max_y_pos;
        internal_max_line_points[1].x = x_end;
        internal_max_line_points[1].y = internal_max_y_pos;

        // Create max line for internal temperature using helper function
        internal_max_line =
            create_temperature_line(chart, "internal max line", lv_palette_main(LV_PALETTE_GREEN),
                                    internal_max_line_points);

        // Add max value label for internal temperature using helper function
        internal_max_label =
            create_temperature_label(chart, "internal max label", lv_palette_main(LV_PALETTE_GREEN),
                                     internal_max_temp, LV_ALIGN_TOP_LEFT, 5, -8);
    }

    if(external_data_valid && external_max_temp > -1000)
    {
        // Calculate y position for external max line using fixed point value
        float external_max_ratio = (float)(external_max_temp - y_min) / (y_max - y_min);
        float external_max_y_pos = chart_h - (external_max_ratio * chart_h);

        // Setup line points
        external_max_line_points[0].x = x_start;
        external_max_line_points[0].y = external_max_y_pos;
        external_max_line_points[1].x = x_end;
        external_max_line_points[1].y = external_max_y_pos;

        // Create max line for external temperature using helper function
        external_max_line = create_temperature_line(
            chart, "external max line", lv_palette_main(LV_PALETTE_BLUE), external_max_line_points);

        // Add max value label for external temperature using helper function
        external_max_label =
            create_temperature_label(chart, "external max label", lv_palette_main(LV_PALETTE_BLUE),
                                     external_max_temp, LV_ALIGN_TOP_RIGHT, -5, -8);
    }

    // Add min value lines and labels for valid series
    if(internal_data_valid && internal_min_temp < 1000)
    {
        // Calculate y position for internal min line using fixed point value
        float internal_min_ratio = (float)(internal_min_temp - y_min) / (y_max - y_min);
        float internal_min_y_pos = chart_h - (internal_min_ratio * chart_h);

        // Setup line points
        internal_min_line_points[0].x = x_start;
        internal_min_line_points[0].y = internal_min_y_pos;
        internal_min_line_points[1].x = x_end;
        internal_min_line_points[1].y = internal_min_y_pos;

        // Create min line for internal temperature using helper function
        internal_min_line =
            create_temperature_line(chart, "internal min line", lv_palette_main(LV_PALETTE_GREEN),
                                    internal_min_line_points);

        // Add min value label for internal temperature using helper function
        internal_min_label =
            create_temperature_label(chart, "internal min label", lv_palette_main(LV_PALETTE_GREEN),
                                     internal_min_temp, LV_ALIGN_BOTTOM_LEFT, 5, 10);
    }

    if(external_data_valid && external_min_temp < 1000)
    {
        // Calculate y position for external min line using fixed point value
        float external_min_ratio = (float)(external_min_temp - y_min) / (y_max - y_min);
        float external_min_y_pos = chart_h - (external_min_ratio * chart_h);

        // Setup line points
        external_min_line_points[0].x = x_start;
        external_min_line_points[0].y = external_min_y_pos;
        external_min_line_points[1].x = x_end;
        external_min_line_points[1].y = external_min_y_pos;

        // Create min line for external temperature using helper function
        external_min_line = create_temperature_line(
            chart, "external min line", lv_palette_main(LV_PALETTE_BLUE), external_min_line_points);

        // Add min value label for external temperature using helper function
        external_min_label =
            create_temperature_label(chart, "external min label", lv_palette_main(LV_PALETTE_BLUE),
                                     external_min_temp, LV_ALIGN_BOTTOM_RIGHT, -5, 10);
        if(external_min_label == NULL)
        {
            return;
        }
    }

    // Update timestamp labels if we have valid timestamps
    if(timestamps_valid)
    {
        char formatted_start[16] = {0};
        char formatted_end[16]   = {0};

        format_chart_timestamp(first_timestamp, formatted_start, sizeof(formatted_start));
        format_chart_timestamp(last_timestamp, formatted_end, sizeof(formatted_end));

        if(start_time_label != NULL)
        {
            lv_label_set_text(start_time_label, formatted_start);
        }

        if(end_time_label != NULL)
        {
            lv_label_set_text(end_time_label, formatted_end);
        }
    }
    else
    {
        // Reset to placeholder if no valid timestamps
        if(start_time_label != NULL)
        {
            lv_label_set_text(start_time_label, "");
        }

        if(end_time_label != NULL)
        {
            lv_label_set_text(end_time_label, "");
        }
    }

    // Refresh the chart to show new data
    lv_chart_refresh(chart);

    // Convert to float for logging
    float min_temp = min_temp_overall / 10.0f;
    float max_temp = max_temp_overall / 10.0f;
    log_debug("Climate chart updated (range: %.1f-%.1f°C)", min_temp, max_temp);
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

    // Reset timestamp labels
    if(start_time_label != NULL)
    {
        lv_label_set_text(start_time_label, "");
    }

    if(end_time_label != NULL)
    {
        lv_label_set_text(end_time_label, "");
    }

    // Reset timestamp validity
    timestamps_valid   = false;
    first_timestamp[0] = '\0';
    last_timestamp[0]  = '\0';

    // Reset data validity flags
    internal_data_valid = false;
    external_data_valid = false;

    log_debug("Climate chart reset completely");
}