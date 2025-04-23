#ifndef BATTERY_CHART_H
#define BATTERY_CHART_H

#include <stdbool.h>
#include "lvgl/lvgl.h"
#include "../../data/data_manager.h"

// Initialize the battery consumption chart
void initialize_energy_chart(lv_obj_t* chart_container);

// Update functions
bool update_energy_chart_with_history(entity_history_t* history_data);

// Cleanup function
void battery_chart_cleanup(void);

#endif /* BATTERY_CHART_H */