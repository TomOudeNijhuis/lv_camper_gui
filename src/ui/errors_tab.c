/*******************************************************************
 *
 * errors_tab.c - Errors tab UI implementation
 *
 ******************************************************************/
#include "errors_tab.h"
#include "../lib/logger.h"
#include "../data/data_manager.h"
#include "../data/error_bits.h"
#include "../data/sensor_types.h"
#include "../main.h"
#include "ui.h"

static lv_obj_t*   errors_container = NULL;
static lv_obj_t*   tabview_ref      = NULL;
static uint32_t    tab_index_ref    = 0;
static lv_timer_t* refresh_timer    = NULL;
static uint16_t    last_rendered_mask = 0;
static bool        first_render       = true;

static void clear_bit_event_cb(lv_event_t* e)
{
    uint16_t bit = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
    log_info("Clearing error bit 0x%04X", bit);
    request_camper_clear_errors(bit);
}

static void render_rows(uint16_t mask)
{
    lv_obj_clean(errors_container);

    if(mask == 0)
    {
        lv_obj_t* label = lv_label_create(errors_container);
        lv_label_set_text(label, "No active errors");
        lv_obj_set_style_text_color(label, lv_color_hex(0x808080), 0);
        return;
    }

    for(size_t i = 0; i < ERROR_BITS_COUNT; i++)
    {
        if(!(mask & ERROR_BITS[i].bit)) continue;

        lv_obj_t* btn = lv_btn_create(errors_container);
        lv_obj_set_size(btn, lv_pct(95), 50);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x8B0000), 0);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, ERROR_BITS[i].name);
        lv_obj_center(label);

        lv_obj_add_event_cb(btn, clear_bit_event_cb, LV_EVENT_CLICKED,
                            (void*)(uintptr_t)ERROR_BITS[i].bit);
    }
}

static void update_tab_badge(uint16_t mask)
{
    if(tabview_ref == NULL) return;
    lv_tabview_rename_tab(tabview_ref, tab_index_ref,
                          mask ? LV_SYMBOL_WARNING " Errors" : "Errors");
}

static void refresh_errors_cb(lv_timer_t* timer)
{
    if(ui_is_sleeping()) return;

    camper_sensor_t* data = get_camper_data();
    if(!data || !data->valid)
    {
        if(first_render)
        {
            render_rows(0);
            first_render = false;
        }
        return;
    }

    uint16_t mask = data->errors_state;
    if(!first_render && mask == last_rendered_mask) return;

    render_rows(mask);
    update_tab_badge(mask);
    last_rendered_mask = mask;
    first_render       = false;
}

void create_errors_tab(lv_obj_t* parent, lv_obj_t* tabview, uint32_t tab_index)
{
    tabview_ref   = tabview;
    tab_index_ref = tab_index;

    errors_container = lv_obj_create(parent);
    lv_obj_set_size(errors_container, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(errors_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(errors_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(errors_container, 10, 0);
    lv_obj_set_style_pad_row(errors_container, 8, 0);
    lv_obj_set_style_bg_opa(errors_container, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(errors_container, LV_SCROLLBAR_MODE_AUTO);

    render_rows(0);

    refresh_timer = lv_timer_create(refresh_errors_cb, DATA_CAMPER_UPDATE_INTERVAL_MS, NULL);
}
