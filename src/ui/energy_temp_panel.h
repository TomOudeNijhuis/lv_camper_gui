#ifndef ENERGY_TEMP_PANEL_H
#define ENERGY_TEMP_PANEL_H

#include "lvgl/lvgl.h"

/**
 * Creates the energy and temperature panel
 * @param parent Parent container where the panel should be created
 */
void create_energy_temp_panel(lv_obj_t* parent);

/**
 * Updates the energy and temperature data displayed in the panel
 */
void update_energy_temp_data(void);

/**
 * Cleanup resources used by the energy and temperature panel
 */
void energy_temp_panel_cleanup(void);

#endif // ENERGY_TEMP_PANEL_H
