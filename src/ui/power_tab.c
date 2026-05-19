/*******************************************************************
 *
 * power_tab.c - Power tab UI for the Camper GUI
 *
 * Builds the Power tab content. The charts are driven by the timers
 * owned by energy_temp_panel.c, so this file holds no state.
 *
 ******************************************************************/
#include "power_tab.h"
#include "charts/battery_chart.h"
#include "charts/solar_chart.h"

void create_power_tab_content(lv_obj_t* power_tab)
{
    lv_obj_set_style_pad_all(power_tab, 0, 0);

    lv_obj_t* power_row = lv_obj_create(power_tab);
    lv_obj_set_size(power_row, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(power_row, 0, 0);
    lv_obj_set_style_radius(power_row, 0, 0);
    lv_obj_set_style_pad_all(power_row, 0, 0);
    lv_obj_set_scrollbar_mode(power_row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(power_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(power_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(power_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    /* Left half: two stacked chart containers. */
    lv_obj_t* power_left = lv_obj_create(power_row);
    lv_obj_set_size(power_left, LV_PCT(50), LV_PCT(100));
    lv_obj_set_style_border_width(power_left, 0, 0);
    lv_obj_set_style_radius(power_left, 0, 0);
    lv_obj_set_style_pad_all(power_left, 5, 0);
    lv_obj_set_scrollbar_mode(power_left, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(power_left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(power_left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(power_left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* battery_chart_container = lv_obj_create(power_left);
    lv_obj_set_size(battery_chart_container, LV_PCT(100), LV_PCT(50));
    lv_obj_set_style_border_width(battery_chart_container, 0, 0);
    lv_obj_set_style_radius(battery_chart_container, 0, 0);
    lv_obj_set_style_pad_all(battery_chart_container, 5, 0);
    lv_obj_set_scrollbar_mode(battery_chart_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(battery_chart_container, LV_OBJ_FLAG_SCROLLABLE);
    initialize_energy_chart(battery_chart_container);

    lv_obj_t* solar_chart_container = lv_obj_create(power_left);
    lv_obj_set_size(solar_chart_container, LV_PCT(100), LV_PCT(50));
    lv_obj_set_style_border_width(solar_chart_container, 0, 0);
    lv_obj_set_style_radius(solar_chart_container, 0, 0);
    lv_obj_set_style_pad_all(solar_chart_container, 5, 0);
    lv_obj_set_scrollbar_mode(solar_chart_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(solar_chart_container, LV_OBJ_FLAG_SCROLLABLE);
    initialize_solar_chart(solar_chart_container);

    /* Right half: empty placeholder. */
    lv_obj_t* power_right = lv_obj_create(power_row);
    lv_obj_set_size(power_right, LV_PCT(50), LV_PCT(100));
    lv_obj_set_style_bg_opa(power_right, LV_OPA_0, 0);
    lv_obj_set_style_border_width(power_right, 0, 0);
    lv_obj_set_style_radius(power_right, 0, 0);
    lv_obj_set_style_pad_all(power_right, 0, 0);
    lv_obj_set_scrollbar_mode(power_right, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(power_right, LV_OBJ_FLAG_SCROLLABLE);
}
