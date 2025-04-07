/*******************************************************************
 *
 * ui.h - User interface definitions for the Camper GUI
 *
 ******************************************************************/
#ifndef STATUS_TAB_H
#define STATUS_TAB_H

#include "lvgl/lvgl.h"
#include "../data/sensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates the main application UI
 * @description Creates a tabview with Status, Analytics, and Logs tabs
 */
void create_status_tab(lv_obj_t *left_column);

void update_status_ui(camper_sensor_t *camper_data);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */