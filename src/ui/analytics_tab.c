#include "analytics_tab.h"
#include "../lib/logger.h"
#include "lvgl/lvgl.h"

/**
 * @brief Create Analytics tab content
 * @param tab_analytics Parent tab object
 */
void create_analytics_tab(lv_obj_t *parent)
{
    // Add content to Analytics tab
    lv_obj_t *analytics_label = lv_label_create(parent);
    lv_label_set_text(analytics_label, "Energy & Resource Analytics");
    lv_obj_align(analytics_label, LV_ALIGN_TOP_MID, 0, 20);
    
    // Create a chart
    lv_obj_t *chart = lv_chart_create(parent);
    lv_obj_set_size(chart, lv_pct(90), lv_pct(60));
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 0);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_point_count(chart, 12);
    lv_chart_set_div_line_count(chart, 5, 5);
    
    // Add series to the chart
    lv_chart_series_t *ser1 = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_series_t *ser2 = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    
    // Set sample data
    for (int i = 0; i < 12; i++) {
        lv_chart_set_next_value(chart, ser1, lv_rand(10, 90));
        lv_chart_set_next_value(chart, ser2, lv_rand(10, 90));
    }
    
    // Add legend
    lv_obj_t *legend_container = lv_obj_create(parent);
    lv_obj_set_size(legend_container, lv_pct(90), 40);
    lv_obj_align(legend_container, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_flex_flow(legend_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(legend_container, 5, 0);
    
    // Legend items
    lv_obj_t *legend_item1 = lv_obj_create(legend_container);
    lv_obj_set_size(legend_item1, 120, 30);
    lv_obj_t *legend_color1 = lv_obj_create(legend_item1);
    lv_obj_set_size(legend_color1, 15, 15);
    lv_obj_set_style_bg_color(legend_color1, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(legend_color1, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_t *legend_text1 = lv_label_create(legend_item1);
    lv_label_set_text(legend_text1, "Battery");
    lv_obj_align(legend_text1, LV_ALIGN_LEFT_MID, 30, 0);
    
    lv_obj_t *legend_item2 = lv_obj_create(legend_container);
    lv_obj_set_size(legend_item2, 120, 30);
    lv_obj_t *legend_color2 = lv_obj_create(legend_item2);
    lv_obj_set_size(legend_color2, 15, 15);
    lv_obj_set_style_bg_color(legend_color2, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align(legend_color2, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_t *legend_text2 = lv_label_create(legend_item2);
    lv_label_set_text(legend_text2, "Solar");
    lv_obj_align(legend_text2, LV_ALIGN_LEFT_MID, 30, 0);
}
