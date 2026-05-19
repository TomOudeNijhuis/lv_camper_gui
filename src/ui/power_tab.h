#ifndef POWER_TAB_H
#define POWER_TAB_H

#include "lvgl/lvgl.h"

/**
 * Build the Power tab content: battery chart and solar chart stacked on
 * the left half. Right half is reserved for future content. The two
 * charts are fed by timers owned by energy_temp_panel.c — no separate
 * lifecycle is needed here.
 */
void create_power_tab_content(lv_obj_t* power_tab);

#endif // POWER_TAB_H
