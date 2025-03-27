/*******************************************************************
 *
 * status_tab.c - Status tab UI implementation for the Camper GUI
 *
 ******************************************************************/
#include "status_tab.h"
#include <string.h>
#include <stdio.h> 
/**
 * @brief Event handler for household button
 */
static void household_event_handler(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    
    const char *current_text = lv_label_get_text(label);
    
    if (strstr(current_text, "[OFF]")) {
        // Turn ON
        lv_label_set_text(label, "Household [ON]");
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x008800), 0); // green
    } else {
        // Turn OFF
        lv_label_set_text(label, "Household [OFF]");
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x880000), 0); // darkred
    }
    
    // Here you would add the actual API call or functionality
}

/**
 * @brief Event handler for pump button
 */
static void pump_event_handler(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    
    const char *current_text = lv_label_get_text(label);
    
    if (strstr(current_text, "[OFF]")) {
        // Turn ON
        lv_label_set_text(label, "Pump [ON]");
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x008800), 0); // green
    } else {
        // Turn OFF
        lv_label_set_text(label, "Pump [OFF]");
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x880000), 0); // darkred
    }
    
    // Here you would add the actual API call or functionality
}

void create_status_tab(lv_obj_t *tab_status)
{
    printf("Starting create_status_tab()\n");
    
    // Create title - make it smaller and more compact
    lv_obj_t *title_label = lv_label_create(tab_status);
    lv_label_set_text(title_label, "Camper");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0); // Smaller font
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 3); // Reduced top margin
    
    printf("Created title\n");
 
    // Create a container for the control elements - smaller padding
    lv_obj_t *controls_container = lv_obj_create(tab_status);
    lv_obj_set_size(controls_container, lv_pct(100), lv_pct(95)); // Larger to use more space
    lv_obj_align(controls_container, LV_ALIGN_BOTTOM_MID, 0, 5); // Reduced top margin
    lv_obj_set_flex_flow(controls_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(controls_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(controls_container, 8, 0); // Reduced row padding
    lv_obj_set_style_pad_all(controls_container, 10, 0); // Reduced overall padding
    
    // Remove border from main container for cleaner look
    lv_obj_set_style_border_width(controls_container, 0, 0);
    lv_obj_set_style_radius(controls_container, 0, 0);
    
    printf("Created control container\n");

    // Household button
    lv_obj_t *household_btn = lv_btn_create(controls_container);
    lv_obj_set_size(household_btn, lv_pct(100), 50);
    lv_obj_set_style_bg_color(household_btn, lv_color_hex(0x880000), 0); // darkred
    lv_obj_t *household_label = lv_label_create(household_btn);
    lv_label_set_text(household_label, "Household [OFF]");
    lv_obj_center(household_label);

    printf("Created household button\n");

    // Pump button
    lv_obj_t *pump_btn = lv_btn_create(controls_container);
    lv_obj_set_size(pump_btn, lv_pct(100), 50);
    lv_obj_set_style_bg_color(pump_btn, lv_color_hex(0x880000), 0); // darkred
    lv_obj_t *pump_label = lv_label_create(pump_btn);
    lv_label_set_text(pump_label, "Pump [OFF]");
    lv_obj_center(pump_label);

    printf("Created pump button\n");

    // Water level bar
    lv_obj_t *water_label = lv_label_create(controls_container);
    lv_label_set_text(water_label, "Water");
    
    lv_obj_t *water_bar = lv_bar_create(controls_container);
    lv_obj_set_size(water_bar, lv_pct(100), 30);
    lv_bar_set_range(water_bar, 0, 100);
    lv_bar_set_value(water_bar, 60, LV_ANIM_OFF); // Example value
    lv_obj_set_style_bg_color(water_bar, lv_color_hex(0x383838), 0);
    lv_obj_set_style_bg_color(water_bar, lv_color_hex(0x2196f3), LV_PART_INDICATOR); // blue

    printf("Created water bar\n");

    // Waste level bar
    lv_obj_t *waste_label = lv_label_create(controls_container);
    lv_label_set_text(waste_label, "Waste");
    
    lv_obj_t *waste_bar = lv_bar_create(controls_container);
    lv_obj_set_size(waste_bar, lv_pct(100), 30);
    lv_bar_set_range(waste_bar, 0, 100);
    lv_bar_set_value(waste_bar, 40, LV_ANIM_OFF); // Example value
    lv_obj_set_style_bg_color(waste_bar, lv_color_hex(0x383838), 0);
    lv_obj_set_style_bg_color(waste_bar, lv_color_hex(0xFF5722), LV_PART_INDICATOR); // orange

    printf("Created waste bar\n");

    // Mains status button (as indicator)
    lv_obj_t *mains_btn = lv_btn_create(controls_container);
    lv_obj_set_size(mains_btn, lv_pct(100), 50);
    lv_obj_set_style_bg_color(mains_btn, lv_color_hex(0x808080), 0); // gray
    lv_obj_clear_flag(mains_btn, LV_OBJ_FLAG_CLICKABLE); // Remove clickable flag (using clear_flag instead of add_flag)
    lv_obj_t *mains_label = lv_label_create(mains_btn);
    lv_label_set_text(mains_label, "Mains [Unknown]");
    lv_obj_center(mains_label);
    
    // Create a container for voltage info with row layout
    lv_obj_t *voltage_container = lv_obj_create(controls_container);
    lv_obj_set_size(voltage_container, lv_pct(100), 70); // Slightly smaller height
    lv_obj_set_style_pad_all(voltage_container, 3, 0); // Reduced padding
    lv_obj_set_style_border_width(voltage_container, 0, 0); // No border on outer container
    
    // Use a row layout for voltages - this places containers side by side
    lv_obj_set_flex_flow(voltage_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(voltage_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    printf("Created voltage container\n");

    // Create starter container (left side) - thinner border
    lv_obj_t *starter_container = lv_obj_create(voltage_container);
    lv_obj_set_size(starter_container, lv_pct(48), lv_pct(95)); // Slightly larger width
    lv_obj_set_style_pad_all(starter_container, 3, 0); // Reduced padding
    lv_obj_set_flex_flow(starter_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(starter_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Thinner border
    lv_obj_set_style_border_width(starter_container, 1, 0); // 1px border
    lv_obj_set_style_border_color(starter_container, lv_color_hex(0xCCCCCC), 0); // Lighter color
    lv_obj_set_style_radius(starter_container, 3, 0); // Smaller radius

    // Create household container (right side) - thinner border
    lv_obj_t *household_container = lv_obj_create(voltage_container);
    lv_obj_set_size(household_container, lv_pct(48), lv_pct(95)); // Slightly larger width
    lv_obj_set_style_pad_all(household_container, 3, 0); // Reduced padding
    lv_obj_set_flex_flow(household_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(household_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Thinner border
    lv_obj_set_style_border_width(household_container, 1, 0); // 1px border
    lv_obj_set_style_border_color(household_container, lv_color_hex(0xCCCCCC), 0); // Lighter color
    lv_obj_set_style_radius(household_container, 3, 0); // Smaller radius
    
    printf("Created starter and household containers\n");

    // Starter voltage
    lv_obj_t *starter_label = lv_label_create(starter_container);
    lv_label_set_text(starter_label, "Starter [V]");
    
    lv_obj_t *starter_value = lv_label_create(starter_container);
    lv_label_set_text(starter_value, "12.6");
    lv_obj_set_style_text_color(starter_value, lv_color_hex(0x00C853), 0); // green
    
    printf("Created starter labels\n");

    // Household voltage
    lv_obj_t *household_v_label = lv_label_create(household_container);
    lv_label_set_text(household_v_label, "Household [V]");
    
    lv_obj_t *household_v_value = lv_label_create(household_container);
    lv_label_set_text(household_v_value, "12.4");
    lv_obj_set_style_text_color(household_v_value, lv_color_hex(0x00C853), 0); // green
    
    printf("Created household labels\n");
    
    // Adding event handlers for buttons
    lv_obj_add_event_cb(household_btn, household_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(pump_btn, pump_event_handler, LV_EVENT_CLICKED, NULL);

    printf("Added event handlers\n");
    printf("Status tab creation complete\n");
}