#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "lvgl/lvgl.h"
#include "energy_temp_panel.h"
#include "../lib/logger.h"
#include "../data/data_manager.h"
#include "ui.h"
#include "../main.h"
#include "lv_awesome_16.h"
#include "charts/temp_chart.h"
#include "charts/battery_chart.h"
#include "charts/solar_chart.h"

// Forward declarations for internal functions
static void update_timer_cb(lv_timer_t* timer);
static void update_long_timer_cb(lv_timer_t* timer);

// Static variables for UI elements
static lv_obj_t*   internal_temp_label  = NULL;
static lv_obj_t*   external_temp_label  = NULL;
static lv_obj_t*   humidity_label       = NULL;
static lv_obj_t*   power_label          = NULL;
static lv_obj_t*   battery_status_label = NULL;
static lv_obj_t*   solar_power_label    = NULL;
static lv_obj_t*   solar_state_icon     = NULL;
static lv_obj_t*   charging_icon        = NULL;
static lv_timer_t* update_timer         = NULL;
static lv_timer_t* update_long_timer    = NULL;

typedef enum
{
    HISTORY_TEMP_INSIDE,
    HISTORY_TEMP_OUTSIDE,
    HISTORY_TEMP_SOLAR,
    HISTORY_TEMP_BATTERY
} history_state_t;

/**
 * Timer callback to update energy and temperature data
 */
static void update_timer_cb(lv_timer_t* timer)
{
    bool result = request_data_fetch(FETCH_CLIMATE_INSIDE);
    if(!result)
    {
        log_warning("Failed to request inside climate data fetch");
    }

    result = request_data_fetch(FETCH_CLIMATE_OUTSIDE);
    if(!result)
    {
        log_warning("Failed to request outside climate data fetch");
    }

    result = request_data_fetch(FETCH_SMART_SOLAR);
    if(!result)
    {
        log_warning("Failed to request smart_solar data fetch");
    }

    result = request_data_fetch(FETCH_SMART_SHUNT);
    if(!result)
    {
        log_warning("Failed to request smart_shunt data fetch");
    }

    climate_sensor_t* inside_climate  = get_inside_climate_data();
    climate_sensor_t* outside_climate = get_outside_climate_data();

    if(internal_temp_label != NULL)
    {
        if(inside_climate->valid)
        {
            lv_label_set_text_fmt(internal_temp_label, "%.1f °C", inside_climate->temperature);
        }
        else
        {
            lv_label_set_text(internal_temp_label, "--- °C");
        }
    }

    if(external_temp_label != NULL)
    {
        if(outside_climate->valid)
        {
            lv_label_set_text_fmt(external_temp_label, "%.1f °C", outside_climate->temperature);
        }
        else
        {
            lv_label_set_text(external_temp_label, "--- °C");
        }
    }

    smart_shunt_t* shunt_data = get_smart_shunt_data();

    // Update battery energy label
    if(power_label != NULL)
    {
        if(shunt_data->valid)
        {
            lv_label_set_text_fmt(power_label, "%.1f W", shunt_data->current * shunt_data->voltage);
        }
        else
        {
            lv_label_set_text(power_label, "--- W");
        }
    }

    if(battery_status_label != NULL)
    {
        if(shunt_data->valid)
        {
            lv_label_set_text_fmt(battery_status_label, "%.1f%%", shunt_data->soc);
        }
        else
        {
            lv_label_set_text(battery_status_label, "--- %");
        }
    }

    smart_solar_t* solar_data = get_smart_solar_data();

    // Update solar power label
    if(solar_power_label != NULL)
    {
        if(solar_data->valid)
        {
            lv_label_set_text_fmt(solar_power_label, "%.0f W", solar_data->solar_power);
        }
        else
        {
            lv_label_set_text(solar_power_label, "--- W");
        }
    }

    // Update the solar state icons based on charge state
    if(solar_state_icon != NULL && charging_icon != NULL)
    {
        if(solar_data->valid)
        {
            // Set battery icon based on state of charge (example logic)
            if(shunt_data->valid)
            {
                float soc = shunt_data->soc;
                if(soc > 80.0f)
                {
                    lv_label_set_text(solar_state_icon, LV_SYMBOL_BATTERY_FULL);
                }
                else if(soc > 60.0f)
                {
                    lv_label_set_text(solar_state_icon, LV_SYMBOL_BATTERY_THREE_QUARTERS);
                }
                else if(soc > 40.0f)
                {
                    lv_label_set_text(solar_state_icon, LV_SYMBOL_BATTERY_HALF);
                }
                else if(soc > 20.0f)
                {
                    lv_label_set_text(solar_state_icon, LV_SYMBOL_BATTERY_QUARTER);
                }
                else
                {
                    lv_label_set_text(solar_state_icon, LV_SYMBOL_BATTERY_EMPTY);
                }
            }
            else
            {
                lv_label_set_text(solar_state_icon, "---");
            }

            // Set charging icon based on charge state - using different arrow sizes
            if(strcmp(solar_data->charge_state, "Bulk") == 0)
            {
                // Strongest charging - use regular arrow up
                lv_label_set_text(charging_icon, LV_SYMBOL_ARROW_UP);
                lv_obj_set_style_text_color(charging_icon, lv_color_hex(0xFF0000),
                                            0); // Red for bulk charging
            }
            else if(strcmp(solar_data->charge_state, "Absorption") == 0)
            {
                // Medium charging - use square arrow
                lv_label_set_text(charging_icon, LV_SYMBOL_ARROW_UP_SQUARE);
                lv_obj_set_style_text_color(charging_icon, lv_color_hex(0xFFCC00),
                                            0); // Yellow for absorption
            }
            else if(strcmp(solar_data->charge_state, "Float") == 0)
            {
                // Light charging - use thin arrow
                lv_label_set_text(charging_icon, LV_SYMBOL_ARROW_UP_THIN);
                lv_obj_set_style_text_color(charging_icon, lv_color_hex(0x00CC00),
                                            0); // Green for float charging
            }
            else // Off or other state
            {
                lv_label_set_text(charging_icon, "");
            }
        }
        else
        {
            lv_label_set_text(solar_state_icon, "");
            lv_label_set_text(charging_icon, "");
        }
    }
}

static void update_long_timer_cb(lv_timer_t* timer)
{
    static history_state_t fetch_state = HISTORY_TEMP_INSIDE;

    if(ui_is_sleeping())
    {
        return;
    }

    if(fetch_state == HISTORY_TEMP_INSIDE)
    {
        if(request_entity_history("inside", "temperature", "1h", 48))
            fetch_state = HISTORY_TEMP_OUTSIDE;
    }
    else if(fetch_state == HISTORY_TEMP_OUTSIDE)
    {
        if(request_entity_history("outside", "temperature", "1h", 48))
            fetch_state = HISTORY_TEMP_SOLAR;
    }
    else if(fetch_state == HISTORY_TEMP_SOLAR)
    {
        if(request_entity_history("SmartSolar", "yield_today", "1h", 49))
            fetch_state = HISTORY_TEMP_BATTERY;
    }
    else if(fetch_state == HISTORY_TEMP_BATTERY)
    {
        if(request_entity_history("SmartShunt", "consumed_ah", "1h", 49))
            fetch_state = HISTORY_TEMP_INSIDE;
    }

    // Check if historical data is available
    entity_history_t* history_data = get_entity_history_data();

    if(history_data != NULL)
    {
        if(history_data->valid)
        {
            if(strcmp(history_data->sensor_name, "inside") == 0)
            {
                update_climate_chart_with_history(history_data, true);
            }
            else if(strcmp(history_data->sensor_name, "outside") == 0)
            {
                update_climate_chart_with_history(history_data, false);
            }
            else if(strcmp(history_data->sensor_name, "SmartSolar") == 0)
            {
                update_solar_chart_with_history(history_data);
            }
            else if(strcmp(history_data->sensor_name, "SmartShunt") == 0)
            {
                update_energy_chart_with_history(history_data);
            }
            else
            {
                log_warning("Unknown sensor name in history data: %s", history_data->sensor_name);
            }
        }

        // Always free the history data when we're done with it
        free_entity_history_data(history_data);
    }
}

void create_temperature_container(lv_obj_t* right_column)
{
    // Temperature section - container with flex layout
    lv_obj_t* temp_container = lv_obj_create(right_column);
    lv_obj_set_size(temp_container, LV_PCT(100), 160);
    lv_obj_set_style_border_width(temp_container, 0, 0);
    lv_obj_set_style_radius(temp_container, 0, 0);
    lv_obj_set_style_pad_all(temp_container, 5, 0);
    lv_obj_set_scrollbar_mode(temp_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(temp_container, LV_OBJ_FLAG_SCROLLABLE);

    // Use row layout for the container
    lv_obj_set_flex_flow(temp_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // Left column for labels
    lv_obj_t* labels_column = lv_obj_create(temp_container);
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

    lv_obj_t* internal_caption = lv_label_create(labels_column);
    lv_label_set_text(internal_caption, "Internal");

    // Internal temperature value with larger font
    internal_temp_label = lv_label_create(labels_column);
    lv_label_set_text(internal_temp_label, "--- °C");
    lv_obj_set_style_text_font(internal_temp_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(internal_temp_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(internal_temp_label, LV_PCT(100));

    // Spacer
    lv_obj_t* spacer = lv_obj_create(labels_column);
    lv_obj_set_height(spacer, 10);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_0, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_scrollbar_mode(spacer, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    // External temperature caption
    lv_obj_t* external_caption = lv_label_create(labels_column);
    lv_label_set_text(external_caption, "External");

    // External temperature value with larger font
    external_temp_label = lv_label_create(labels_column);
    lv_label_set_text(external_temp_label, "--- °C");
    lv_obj_set_style_text_font(external_temp_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(external_temp_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(external_temp_label, LV_PCT(100));

    // Right column for chart
    lv_obj_t* chart_container = lv_obj_create(temp_container);
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

    initialize_temperature_chart(chart_container);
}

/**
 * Creates container with battery power and hourly energy chart
 */
void create_energy_container(lv_obj_t* right_column)
{
    // Main container for battery energy and hourly chart
    lv_obj_t* energy_container = lv_obj_create(right_column);
    lv_obj_set_size(energy_container, LV_PCT(100), 160);
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

    initialize_energy_chart(hourly_chart_container);
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

    // Caption label for state
    // lv_obj_t* solar_state_caption = lv_label_create(power_column);
    // lv_label_set_text(solar_state_caption, "State");

    // Create container for the icons
    lv_obj_t* icon_container = lv_obj_create(power_column);
    lv_obj_set_size(icon_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(icon_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(icon_container, 0, 0);
    lv_obj_set_style_pad_all(icon_container, 0, 0);
    lv_obj_set_flex_flow(icon_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(icon_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(icon_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(icon_container, LV_OBJ_FLAG_SCROLLABLE);

    // Battery state icon
    solar_state_icon = lv_label_create(icon_container);
    lv_label_set_text(solar_state_icon, "");
    lv_obj_set_style_text_font(solar_state_icon, &lv_awesome_16, 0);
    lv_obj_set_style_text_font(solar_state_icon, &lv_awesome_16, 0);

    // Charging icon
    charging_icon = lv_label_create(icon_container);
    lv_label_set_text(charging_icon, "");
    lv_obj_set_style_text_font(charging_icon, &lv_awesome_16, 0);

    // Right column for hourly energy chart
    lv_obj_t* hourly_chart_container = lv_obj_create(solar_container);
    lv_obj_set_size(hourly_chart_container, LV_PCT(80), LV_PCT(100));
    lv_obj_set_style_border_width(hourly_chart_container, 0, 0);
    lv_obj_set_style_radius(hourly_chart_container, 0, 0);
    lv_obj_set_style_pad_all(hourly_chart_container, 5, 0);
    lv_obj_set_scrollbar_mode(hourly_chart_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(hourly_chart_container, LV_OBJ_FLAG_SCROLLABLE);

    initialize_solar_chart(hourly_chart_container);
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
    update_timer      = lv_timer_create(update_timer_cb, DATA_UPDATE_INTERVAL_MS, NULL);
    update_long_timer = lv_timer_create(update_long_timer_cb, DATA_CHART_UPDATE_INTERVAL_MS, NULL);
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

    if(update_long_timer != NULL)
    {
        lv_timer_del(update_long_timer);
        update_long_timer = NULL;
    }

    // Reset all static UI element pointers
    internal_temp_label  = NULL;
    external_temp_label  = NULL;
    power_label          = NULL;
    battery_status_label = NULL;
    solar_power_label    = NULL;
    solar_state_icon     = NULL;
    charging_icon        = NULL;

    temp_chart_cleanup();
    battery_chart_cleanup();
    solar_chart_cleanup();

    log_info("Energy and temperature panel cleaned up");
}
