#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "lvgl/lvgl.h"
#include "battery_chart.h"
#include "../energy_temp_panel.h"
#include "../../lib/logger.h"
#include "../../data/data_manager.h"
#include "../ui.h"
#include "../../main.h"

static lv_chart_series_t* hourly_energy_series = NULL;
static lv_obj_t*          energy_chart         = NULL;

// Add static variables for energy chart max line and label
static lv_obj_t*          energy_max_line  = NULL;
static lv_obj_t*          energy_max_label = NULL;
static lv_point_precise_t energy_max_line_points[2];

// Add static variables for timestamp display
static char      first_timestamp[32] = {0};
static char      last_timestamp[32]  = {0};
static lv_obj_t* start_time_label    = NULL;
static lv_obj_t* end_time_label      = NULL;
static bool      timestamps_valid    = false;

// Format timestamp to display date and time
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

static void cleanup_energy_chart_lines_and_labels(void)
{
    if(energy_max_line != NULL)
    {
        lv_obj_del(energy_max_line);
        energy_max_line = NULL;
    }

    if(energy_max_label != NULL)
    {
        lv_obj_del(energy_max_label);
        energy_max_label = NULL;
    }
}

static lv_obj_t* create_energy_line(lv_obj_t* parent, const char* line_name, lv_color_t color,
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

static lv_obj_t* create_energy_label(lv_obj_t* parent, const char* label_name, lv_color_t color,
                                     float value, lv_align_t alignment, lv_coord_t x_offset,
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
    lv_label_set_text_fmt(label, "%.1f Ah", value);
    lv_obj_align(label, alignment, x_offset, y_offset);

    return label;
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

    lv_chart_refresh(energy_chart); // Required after direct set
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

        // Use the dedicated function to clean up all lines and labels
        cleanup_energy_chart_lines_and_labels();

        // Get chart content dimensions and coordinates
        lv_coord_t chart_w = lv_obj_get_content_width(energy_chart);
        lv_coord_t chart_h = lv_obj_get_content_height(energy_chart);

        // Calculate points for the actual data area
        float x_start = 1;
        float x_end   = chart_w - 1;

        // Create new max line using helper function
        float max_y_pos = chart_h - ((max_ah_change / range_max) * chart_h);

        energy_max_line_points[0].x = x_start;
        energy_max_line_points[0].y = max_y_pos;
        energy_max_line_points[1].x = x_end;
        energy_max_line_points[1].y = max_y_pos;

        energy_max_line =
            create_energy_line(energy_chart, "energy max line", lv_palette_main(LV_PALETTE_RED),
                               energy_max_line_points);
        if(energy_max_line == NULL)
        {
            return false;
        }

        // Add max value label using helper function
        energy_max_label =
            create_energy_label(energy_chart, "energy max label", lv_palette_main(LV_PALETTE_RED),
                                max_ah_change, LV_ALIGN_TOP_LEFT, 5, -8);
        if(energy_max_label == NULL)
        {
            return false;
        }

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

        // Extract timestamps from history data for timeline display
        if(history_data->timestamps != NULL && data_count > 0)
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
            // Reset to empty if no valid timestamps
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
        lv_chart_refresh(energy_chart);

        log_debug("Energy chart updated with %d historical Ah consumption points", data_count - 1);
        return true;
    }
    return false;
}

void battery_chart_cleanup(void)
{
    // Use the dedicated cleanup function instead of repeating code
    cleanup_energy_chart_lines_and_labels();

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

    // Reset timestamps
    timestamps_valid   = false;
    first_timestamp[0] = '\0';
    last_timestamp[0]  = '\0';

    // Note: chart and series will be deleted by LVGL when their parent is deleted
    energy_chart         = NULL;
    hourly_energy_series = NULL;

    log_debug("Battery chart cleaned up");
}