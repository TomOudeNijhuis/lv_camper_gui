/*******************************************************************
 *
 * ui.c - User interface implementation for the Camper GUI
 *
 ******************************************************************/
#include "ui.h"
#include "status_tab.h"

/**
 * @brief Create Analytics tab content
 * @param tab_analytics Parent tab object
 */
static void create_analytics_tab(lv_obj_t *tab_analytics)
{
    // Add content to Analytics tab
    lv_obj_t *analytics_label = lv_label_create(tab_analytics);
    lv_label_set_text(analytics_label, "Energy & Resource Analytics");
    lv_obj_align(analytics_label, LV_ALIGN_TOP_MID, 0, 20);
    
    // Create a chart
    lv_obj_t *chart = lv_chart_create(tab_analytics);
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
    lv_obj_t *legend_container = lv_obj_create(tab_analytics);
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

/**
 * @brief Create Logs tab content
 * @param tab_logs Parent tab object
 */
static void create_logs_tab(lv_obj_t *tab_logs)
{
    // Add content to Logs tab
    lv_obj_t *logs_label = lv_label_create(tab_logs);
    lv_label_set_text(logs_label, "System Logs");
    lv_obj_align(logs_label, LV_ALIGN_TOP_MID, 0, 20);
    
    // Create a text area for logs
    lv_obj_t *ta = lv_textarea_create(tab_logs);
    lv_obj_set_size(ta, lv_pct(90), lv_pct(70));
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, 10);
    lv_textarea_set_placeholder_text(ta, "System logs will appear here...");
    lv_textarea_set_one_line(ta, false);
    lv_obj_set_style_text_align(ta, LV_TEXT_ALIGN_LEFT, 0);
    
    // Add some example log entries
    const char *log_entries = 
        "10:15:23 System started\n"
        "10:15:24 Battery status: Good\n"
        "10:15:25 WiFi connected\n"
        "10:15:30 Water sensor initialized\n"
        "10:16:05 Solar panel connected\n"
        "10:17:12 Temperature: Indoor 22°C, Outdoor 18°C\n"
        "10:20:45 Battery charging from solar: 250W\n"
        "10:25:33 Water tank level: 60%\n";
    
    lv_textarea_set_text(ta, log_entries);
    
    // Add a refresh button
    lv_obj_t *refresh_btn = lv_btn_create(tab_logs);
    lv_obj_set_size(refresh_btn, 120, 40);
    lv_obj_align(refresh_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_t *refresh_label = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_label, "Refresh");
    lv_obj_center(refresh_label);
}

/**
 * @brief Creates the main application UI
 * @description Creates a tabview with Status, Analytics, and Logs tabs
 */
void create_ui(void)
{
    // Create a tabview object
    lv_obj_t *tabview = lv_tabview_create(lv_screen_active());
    lv_obj_set_size(tabview, lv_pct(100), lv_pct(100));
    
    // Remove padding and gap between tabs and content
    lv_obj_set_style_pad_all(tabview, 0, 0);
    
    // Create tabs
    lv_obj_t *tab_status = lv_tabview_add_tab(tabview, "Status");
    lv_obj_t *tab_analytics = lv_tabview_add_tab(tabview, "Analytics");
    lv_obj_t *tab_logs = lv_tabview_add_tab(tabview, "Logs");
    
    // Create content for each tab
    create_status_tab(tab_status);
    create_analytics_tab(tab_analytics);
    create_logs_tab(tab_logs);
}
