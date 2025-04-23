#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "lvgl/lvgl.h"
#include "solar_chart.h"
#include "../energy_temp_panel.h"
#include "../../lib/logger.h"
#include "../../data/data_manager.h"
#include "../ui.h"
#include "../../main.h"

static lv_chart_series_t* solar_hourly_energy_series = NULL;
static lv_obj_t*          solar_energy_chart         = NULL;

// Add static variables for solar chart max line and label
static lv_obj_t*          solar_max_line  = NULL;
static lv_obj_t*          solar_max_label = NULL;
static lv_point_precise_t solar_max_line_points[2];

// Add a new function to clean up all chart lines and labels
static void cleanup_solar_chart_lines_and_labels(void)
{
    if(solar_max_line != NULL)
    {
        lv_obj_del(solar_max_line);
        solar_max_line = NULL;
    }

    if(solar_max_label != NULL)
    {
        lv_obj_del(solar_max_label);
        solar_max_label = NULL;
    }
}

// Add a function to safely create a line with error handling
static lv_obj_t* create_solar_line(lv_obj_t* parent, const char* line_name, lv_color_t color,
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
static lv_obj_t* create_solar_label(lv_obj_t* parent, const char* label_name, lv_color_t color,
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
    lv_label_set_text_fmt(label, "%.1f Wh", value);
    lv_obj_align(label, alignment, x_offset, y_offset);

    return label;
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

        // Safety check - ensure we have enough data points to calculate differences
        if(data_count < 2)
        {
            log_warning("Not enough solar data points to calculate yield (%d points)", data_count);
            return false;
        }

        // Find the overall min and max yield values
        float hourly_yield[point_count];
        float prev_sample = history_data->max[0];
        float max_yield   = 0;

        // Fix: Only iterate up to data_count-1 to prevent accessing beyond array bounds
        for(int i = 0; i < point_count && i < data_count - 1; i++)
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

        // Use the dedicated function to clean up all lines and labels
        cleanup_solar_chart_lines_and_labels();

        // Get chart content dimensions and coordinates
        lv_coord_t chart_w = lv_obj_get_content_width(solar_energy_chart);
        lv_coord_t chart_h = lv_obj_get_content_height(solar_energy_chart);

        // Calculate points for the actual data area
        float x_start = 1;
        float x_end   = chart_w - 1;

        // Create new max line using helper function
        float max_y_pos = chart_h - ((max_yield / range_max) * chart_h);

        solar_max_line_points[0].x = x_start;
        solar_max_line_points[0].y = max_y_pos;
        solar_max_line_points[1].x = x_end;
        solar_max_line_points[1].y = max_y_pos;

        solar_max_line =
            create_solar_line(solar_energy_chart, "solar max line",
                              lv_palette_main(LV_PALETTE_GREEN), solar_max_line_points);
        if(solar_max_line == NULL)
        {
            return false;
        }

        // Add max value label using helper function
        solar_max_label = create_solar_label(solar_energy_chart, "solar max label",
                                             lv_palette_main(LV_PALETTE_GREEN), max_yield,
                                             LV_ALIGN_TOP_LEFT, 5, -8);
        if(solar_max_label == NULL)
        {
            return false;
        }

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
    if(history_data == NULL)
        return false;

    bool success = false;

    // Safely check sensor name (avoid segfault if strings are not properly initialized)
    if(history_data->valid && history_data->count > 0 && history_data->sensor_name != NULL &&
       history_data->sensor_name[0] != '\0' && history_data->max != NULL &&
       strcmp(history_data->sensor_name, "SmartSolar") == 0)
    {
        success = update_solar_chart_with_history(history_data);
    }
    else
    {
        // Clear chart when data is invalid - with improved null checks
        if(solar_energy_chart != NULL && solar_hourly_energy_series != NULL)
        {
            // Safely clear chart data
            lv_chart_set_all_value(solar_energy_chart, solar_hourly_energy_series,
                                   LV_CHART_POINT_NONE);
            lv_chart_refresh(solar_energy_chart);

            // Use the dedicated function to clean up all lines and labels
            cleanup_solar_chart_lines_and_labels();

            log_debug("Solar chart cleared due to invalid data");
        }
    }

    free_entity_history_data(history_data);
    return success;
}

void solar_chart_cleanup(void)
{
    // Use the dedicated cleanup function instead of repeating code
    cleanup_solar_chart_lines_and_labels();

    // Note: chart and series will be deleted by LVGL when their parent is deleted
    solar_hourly_energy_series = NULL;
    solar_energy_chart         = NULL;

    log_debug("Solar chart cleaned up");
}
