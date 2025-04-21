#ifndef ENERGY_TEMP_CHARTS_H
#define ENERGY_TEMP_CHARTS_H

#include <stdbool.h>
#include "lvgl/lvgl.h"
#include "../data/data_manager.h"

// Chart initialization functions
void initialize_temperature_chart(lv_obj_t* chart_container);
void initialize_energy_chart(lv_obj_t* chart_container);
void initialize_solar_chart(lv_obj_t* chart_container);

// Timer callback function
void update_long_timer_cb(lv_timer_t* timer);

// Cleanup function
void chart_cleanup(void);

#endif // ENERGY_TEMP_CHARTS_H