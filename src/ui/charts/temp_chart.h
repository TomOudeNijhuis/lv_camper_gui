#ifndef TEMP_CHART_H
#define TEMP_CHART_H

#include <stdbool.h>
#include "lvgl/lvgl.h"
#include "../../data/data_manager.h"

// Initialize the temperature chart
void initialize_temperature_chart(lv_obj_t* chart_container);

// Update functions
void update_climate_chart_with_history(entity_history_t* history_data, bool is_internal);
void refresh_climate_chart(void);
void reset_climate_chart(void);

// Data fetching functions
bool fetch_internal_climate(void);
bool fetch_external_climate(void);

// Combined fetch and update functions
bool update_internal_climate_chart(void);
bool update_external_climate_chart(void);

// Cleanup function
void temp_chart_cleanup(void);

#endif /* TEMP_CHART_H */
