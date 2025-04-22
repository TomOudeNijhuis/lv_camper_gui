#ifndef SOLAR_CHART_H
#define SOLAR_CHART_H

#include <stdbool.h>
#include "lvgl/lvgl.h"
#include "../../data/data_manager.h"

// Initialize the solar chart
void initialize_solar_chart(lv_obj_t* chart_container);

// Chart update functions
bool update_solar_chart_with_history(entity_history_t* history_data);

// Data fetching functions
bool fetch_solar(void);

// Combined fetch and update functions
bool update_solar_chart(void);

// Cleanup function
void solar_chart_cleanup(void);

#endif /* SOLAR_CHART_H */