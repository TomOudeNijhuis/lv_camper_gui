#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "lvgl/lvgl.h"
#include "solar_chart.h"
#include "chart_utils.h"
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

// Add static variables for timestamp display
static char      first_timestamp[32] = {0};
static char      last_timestamp[32]  = {0};
static lv_obj_t* start_time_label    = NULL;
static lv_obj_t* end_time_label      = NULL;
static bool      timestamps_valid    = false;

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
        solar_energy_chart, lv_palette_main(LV_PALETTE_PURPLE), LV_CHART_AXIS_PRIMARY_Y);

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
        for(int i = 0; i < point_count; i++) hourly_yield[i] = 0.0f;
        float prev_sample = history_data->max[0];
        float max_yield   = 0;
        const int yield_count = (data_count - 1 < point_count) ? (data_count - 1) : point_count;

        for(int i = 0; i < yield_count; i++)
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
            chart_create_line(solar_energy_chart, "solar max line",
                              lv_palette_main(LV_PALETTE_PURPLE), solar_max_line_points);
        if(solar_max_line == NULL)
        {
            return false;
        }

        // Add max value label using helper function
        solar_max_label = chart_create_value_label(solar_energy_chart, "solar max label",
                                                   lv_palette_main(LV_PALETTE_PURPLE), max_yield,
                                                   "Wh", LV_ALIGN_TOP_LEFT, 5, -8);
        if(solar_max_label == NULL)
        {
            return false;
        }

        // Fill chart left-to-right: oldest yield first, newest yield last.
        for(int i = 0; i < yield_count; i++)
        {
            if(hourly_yield[i] <= 0.1f) // small threshold for near-zero floats
            {
                lv_chart_set_next_value(solar_energy_chart, solar_hourly_energy_series,
                                        LV_CHART_POINT_NONE);
            }
            else
            {
                int yield_value = (int)(hourly_yield[i] * 10); // fixed-point, 1 decimal
                lv_chart_set_next_value(solar_energy_chart, solar_hourly_energy_series,
                                        yield_value);
            }
        }

        // Extract timestamps from history data for timeline display.
        // API returns oldest first: timestamps[0] = oldest, timestamps[N-1] = newest.
        if(history_data->timestamps != NULL && data_count > 0)
        {
            strncpy(first_timestamp, history_data->timestamps[0],
                    sizeof(first_timestamp) - 1);
            strncpy(last_timestamp, history_data->timestamps[data_count - 1],
                    sizeof(last_timestamp) - 1);
            first_timestamp[sizeof(first_timestamp) - 1] = '\0';
            last_timestamp[sizeof(last_timestamp) - 1]   = '\0';
            timestamps_valid                             = true;
        }

        // Update timestamp labels if we have valid timestamps
        if(timestamps_valid)
        {
            char formatted_start[16] = {0};
            char formatted_end[16]   = {0};

            chart_format_timestamp(first_timestamp, formatted_start, sizeof(formatted_start));
            chart_format_timestamp(last_timestamp, formatted_end, sizeof(formatted_end));

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
        lv_chart_refresh(solar_energy_chart);

        log_debug("Solar chart updated with %d historical yield points", data_count);
        return true;
    }
    return false;
}

void solar_chart_cleanup(void)
{
    // Use the dedicated cleanup function instead of repeating code
    cleanup_solar_chart_lines_and_labels();

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
    solar_hourly_energy_series = NULL;
    solar_energy_chart         = NULL;

    log_debug("Solar chart cleaned up");
}
