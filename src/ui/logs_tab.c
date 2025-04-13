#include "logs_tab.h"
#include "../lib/logger.h"
#include "lvgl/lvgl.h"
#include "../main.h"
#include "ui.h"

static lv_obj_t*   logs_container;
static lv_timer_t* refresh_timer;
static lv_obj_t*   level_buttons[5]; // Store button references to update their appearance

static void refresh_logs_cb(lv_timer_t* timer)
{
    if(!ui_is_sleeping())
    {
        logger_update_ui(logs_container);
    }
}

static void update_button_states(log_level_t active_level)
{
    // Update appearance of all level buttons to reflect which levels are active
    for(int i = 0; i < 5; i++)
    {
        if(i >= active_level)
        {
            // This level and higher are active - make button look selected
            lv_obj_add_state(level_buttons[i], LV_STATE_CHECKED);
        }
        else
        {
            // This level is inactive - make button look unselected
            lv_obj_clear_state(level_buttons[i], LV_STATE_CHECKED);
        }
    }
}

static void log_level_button_event_cb(lv_event_t* e)
{
    int level = (int)(intptr_t)lv_event_get_user_data(e);
    logger_set_level((log_level_t)level);
    update_button_states((log_level_t)level);
    logger_update_ui(logs_container);
}

static void clear_button_event_cb(lv_event_t* e)
{
    logger_clear();

    // Clear all child objects from the container
    lv_obj_clean(logs_container);

    // Update the UI after clearing
    logger_update_ui(logs_container);
}

void create_logs_tab(lv_obj_t* parent)
{
    // Create a container for logs with scrolling
    logs_container = lv_obj_create(parent);
    lv_obj_set_size(logs_container, lv_pct(100), lv_pct(85));
    lv_obj_set_flex_flow(logs_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(logs_container, 10, 0);
    lv_obj_set_style_pad_row(logs_container, 2, 0);

    // Add scrollbar
    lv_obj_set_style_bg_opa(logs_container, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(logs_container, LV_SCROLLBAR_MODE_AUTO);

    // Create filter buttons
    lv_obj_t* btn_container = lv_obj_create(parent);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn_container, lv_pct(100), lv_pct(15));
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // Make container completely transparent with no borders
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_shadow_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);

    // Create level filter buttons with "and above" naming convention
    const char* btn_texts[] = {"Debug+", "Info+", "Warning+", "Error+", "Critical"};

    for(int i = 0; i < 5; i++)
    {
        lv_obj_t* btn    = lv_btn_create(btn_container);
        level_buttons[i] = btn; // Store button reference for state updates

        lv_obj_set_size(btn, lv_pct(15), 40);
        lv_obj_set_style_bg_color(btn, log_level_colors[i], 0);

        // Make buttons look like toggle buttons
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);

        // Adjust color when pressed to look more selected
        lv_obj_set_style_bg_color(btn, lv_color_darken(log_level_colors[i], LV_OPA_30),
                                  LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_border_width(btn, 3, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, btn_texts[i]);
        lv_obj_center(label);

        // Add filter callback
        lv_obj_add_event_cb(btn, log_level_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    // Add a clear button
    lv_obj_t* clear_btn = lv_btn_create(btn_container);
    lv_obj_set_size(clear_btn, lv_pct(15), 40);

    lv_obj_t* clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "Clear");
    lv_obj_center(clear_label);

    lv_obj_add_event_cb(clear_btn, clear_button_event_cb, LV_EVENT_CLICKED, NULL);

    // Create a timer to refresh the logs
    refresh_timer = lv_timer_create(refresh_logs_cb, LOG_REFRESH_INTERVAL_MS, NULL);

    // Set initial state - INFO is default in logger.c
    update_button_states(LOG_LEVEL_INFO);

    // Initial population of logs
    logger_update_ui(logs_container);
}