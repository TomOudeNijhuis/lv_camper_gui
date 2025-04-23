#include <stdbool.h>
#include <math.h>
#include <string.h>

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

// Add a new function to clean up all chart lines and labels
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

// Add a function to safely create a line with error handling
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

// Add a function to safely create a label with error handling
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

    lv_chart_refresh(energy_chart); // Force refresh after all points are set
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
    if(history_data == NULL)
        return false;

    bool success = false;

    // Safely check sensor name
    if(history_data->valid && history_data->sensor_name != NULL &&
       history_data->sensor_name[0] != '\0' &&
       strcmp(history_data->sensor_name, "SmartShunt") == 0 && history_data->max != NULL &&
       history_data->count > 0)
    {
        success = update_energy_chart_with_history(history_data);
    }
    else
    {
        // Clear chart when data is invalid
        if(energy_chart != NULL && hourly_energy_series != NULL)
        {
            // Safely clear chart data
            lv_chart_set_all_value(energy_chart, hourly_energy_series, LV_CHART_POINT_NONE);
            lv_chart_refresh(energy_chart);

            // Use the dedicated cleanup function instead of repeating code
            cleanup_energy_chart_lines_and_labels();

            log_debug("Energy chart cleared due to invalid data");
        }
    }

    free_entity_history_data(history_data);
    return success;
}

void battery_chart_cleanup(void)
{
    // Use the dedicated cleanup function instead of repeating code
    cleanup_energy_chart_lines_and_labels();

    // Note: chart and series will be deleted by LVGL when their parent is deleted
    energy_chart         = NULL;
    hourly_energy_series = NULL;

    log_debug("Battery chart cleaned up");
}