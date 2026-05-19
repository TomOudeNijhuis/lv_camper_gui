#ifndef ENERGY_TEMP_PANEL_H
#define ENERGY_TEMP_PANEL_H

#include "lvgl/lvgl.h"

/**
 * Build the Status tab right column: enlarged temperature chart on top,
 * summary panel with all data labels on the bottom. Also creates the two
 * shared timers that drive label and chart updates for both this column
 * and the Power tab.
 */
void create_status_right_column(lv_obj_t* right_column);

/**
 * Cleanup the shared timers, label pointers, and chart statics owned by
 * this module. Called once from ui_cleanup().
 */
void energy_temp_panel_cleanup(void);

#endif // ENERGY_TEMP_PANEL_H
