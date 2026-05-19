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
static void update_camper_timer_cb(lv_timer_t* timer)
{
    static uint32_t last_update_time = 0;
    uint32_t        current_time     = lv_tick_get();

    // Skip update if less than 60 seconds (1 minute) has passed since last update
    if(last_update_time != 0 && current_time - last_update_time < 60 * 1000)
    {
        // pass
    }
    else
    {
        // Update the timestamp
        last_update_time = current_time;

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
    }

    if(ui_is_sleeping())
    {
        return;
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
                lv_label_set_text(solar_state_icon, "");
            }

            // Set charging icon based on charge state - using different arrow sizes
            if(strcmp(solar_data->charge_state, "bulk") == 0)
            {
                // Strongest charging - use regular arrow up
                lv_label_set_text(charging_icon, LV_SYMBOL_ARROW_UP);
                lv_obj_set_style_text_color(charging_icon, lv_color_hex(0xFF0000),
                                            0); // Red for bulk charging
            }
            else if(strcmp(solar_data->charge_state, "absorption") == 0)
            {
                // Medium charging - use square arrow
                lv_label_set_text(charging_icon, LV_SYMBOL_ARROW_UP_SQUARE);
                lv_obj_set_style_text_color(charging_icon, lv_color_hex(0xFFCC00),
                                            0); // Yellow for absorption
            }
            else if(strcmp(solar_data->charge_state, "float") == 0)
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
    static history_state_t fetch_state      = HISTORY_TEMP_INSIDE;
    static uint32_t        last_update_time = 0;
    uint32_t               current_time     = lv_tick_get();

    // Skip update if not enough time has passed since last update
    if(last_update_time != 0 && current_time - last_update_time < 15 * 60 * 1000)
    {
        // pass
    }
    else
    {
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
            {
                // Update the timestamp
                // FIXME: Not perfect when a request fails, but good enough for now
                last_update_time = current_time;

                fetch_state = HISTORY_TEMP_INSIDE;
            }
        }
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

/* Small helper: transparent fixed-height spacer for vertical flex columns. */
static void add_vspacer(lv_obj_t* parent, int32_t height)
{
    lv_obj_t* s = lv_obj_create(parent);
    lv_obj_set_size(s, LV_PCT(100), height);
    lv_obj_set_style_bg_opa(s, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    lv_obj_set_scrollbar_mode(s, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
}

/* Small helper: a centred 1/3-width vertical sub-column inside the
 * summary row. Used for the three columns in the Status tab's summary
 * panel. */
static lv_obj_t* make_summary_subcol(lv_obj_t* parent)
{
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_set_size(col, LV_PCT(33), LV_PCT(100));
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_radius(col, 0, 0);
    lv_obj_set_style_pad_all(col, 5, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 5, 0);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(col, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    return col;
}

/* Small helper: caption + value pair (caption styled with palette colour,
 * value styled with montserrat_20). Returns the value label. */
static lv_obj_t* add_caption_value(lv_obj_t* parent, const char* caption_text,
                                   lv_palette_t caption_palette,
                                   const char* initial_value)
{
    lv_obj_t* caption = lv_label_create(parent);
    lv_label_set_text(caption, caption_text);
    lv_obj_set_style_text_color(caption, lv_palette_main(caption_palette), 0);

    lv_obj_t* value = lv_label_create(parent);
    lv_label_set_text(value, initial_value);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_20, 0);
    return value;
}

/**
 * Build the Status tab right column.
 *
 * Layout (column flex on the parent, already configured by ui.c):
 *  - Top   60%: temperature chart container (enlarged).
 *  - Bottom 40%: summary panel with two sub-columns of labels.
 *
 * This function also creates the two shared timers that drive label
 * updates and history fetches for both this column and the Power tab.
 */
void create_status_right_column(lv_obj_t* right_column)
{
    /* --- Top: enlarged temperature chart ------------------------------- */
    lv_obj_t* temp_chart_container = lv_obj_create(right_column);
    lv_obj_set_size(temp_chart_container, LV_PCT(100), LV_PCT(60));
    lv_obj_set_style_border_width(temp_chart_container, 0, 0);
    lv_obj_set_style_radius(temp_chart_container, 0, 0);
    lv_obj_set_style_pad_all(temp_chart_container, 5, 0);
    lv_obj_set_scrollbar_mode(temp_chart_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(temp_chart_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* chart_title = lv_label_create(temp_chart_container);
    lv_label_set_text(chart_title, "Hourly Temperature (°C)");
    lv_obj_set_style_pad_all(chart_title, -5, 0);
    lv_obj_align(chart_title, LV_ALIGN_TOP_MID, 0, 0);

    initialize_temperature_chart(temp_chart_container);

    /* --- Bottom: summary panel ----------------------------------------- */
    lv_obj_t* summary_container = lv_obj_create(right_column);
    lv_obj_set_size(summary_container, LV_PCT(100), LV_PCT(40));
    lv_obj_set_style_border_width(summary_container, 0, 0);
    lv_obj_set_style_radius(summary_container, 0, 0);
    lv_obj_set_style_pad_all(summary_container, 5, 0);
    lv_obj_set_scrollbar_mode(summary_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(summary_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(summary_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(summary_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Column 1: temperatures */
    lv_obj_t* temps_column = make_summary_subcol(summary_container);
    internal_temp_label = add_caption_value(temps_column, "Internal",
                                            LV_PALETTE_GREEN, "--- °C");
    lv_obj_set_style_text_align(internal_temp_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(internal_temp_label, LV_PCT(100));
    add_vspacer(temps_column, 25);
    external_temp_label = add_caption_value(temps_column, "External",
                                            LV_PALETTE_BLUE, "--- °C");
    lv_obj_set_style_text_align(external_temp_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(external_temp_label, LV_PCT(100));

    /* Column 2: battery */
    lv_obj_t* battery_column = make_summary_subcol(summary_container);
    power_label = add_caption_value(battery_column, "Bat Power",
                                    LV_PALETTE_ORANGE, "--- W");
    add_vspacer(battery_column, 25);
    battery_status_label = add_caption_value(battery_column, "Status",
                                             LV_PALETTE_ORANGE, "-- %");

    /* Column 3: solar (with battery glyph + charging arrow at the bottom) */
    lv_obj_t* solar_column = make_summary_subcol(summary_container);
    solar_power_label = add_caption_value(solar_column, "Solar Power",
                                          LV_PALETTE_PURPLE, "--- W");
    add_vspacer(solar_column, 25);

    /* Battery glyph + charging arrow share a row-flex so they sit
     * side-by-side and centred. Caption matches the other "value" rows
     * so the icons land on the same horizontal line as the SoC %. */
    lv_obj_t* state_caption = lv_label_create(solar_column);
    lv_label_set_text(state_caption, "State");
    lv_obj_set_style_text_color(state_caption, lv_palette_main(LV_PALETTE_PURPLE), 0);

    lv_obj_t* icon_container = lv_obj_create(solar_column);
    lv_obj_set_size(icon_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(icon_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(icon_container, 0, 0);
    lv_obj_set_style_pad_all(icon_container, 0, 0);
    lv_obj_set_flex_flow(icon_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(icon_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(icon_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(icon_container, LV_OBJ_FLAG_SCROLLABLE);

    solar_state_icon = lv_label_create(icon_container);
    lv_label_set_text(solar_state_icon, "");
    lv_obj_set_style_text_font(solar_state_icon, &lv_awesome_16, 0);

    charging_icon = lv_label_create(icon_container);
    lv_label_set_text(charging_icon, "");
    lv_obj_set_style_text_font(charging_icon, &lv_awesome_16, 0);

    /* --- Timers (shared with the Power tab; teardown in cleanup) ------- */
    update_timer = lv_timer_create(update_camper_timer_cb,
                                   DATA_OTHER_UPDATE_INTERVAL_MS, NULL);
    update_long_timer = lv_timer_create(update_long_timer_cb,
                                        DATA_CHART_UPDATE_INTERVAL_MS, NULL);
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
