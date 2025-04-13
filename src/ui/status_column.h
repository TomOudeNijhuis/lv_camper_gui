/*******************************************************************
 *
 * ui.h - User interface definitions for the Camper GUI
 *
 ******************************************************************/
#ifndef STATUS_COLUMN_H
#define STATUS_COLUMN_H

#include "lvgl/lvgl.h"
#include "../data/sensor_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Creates the main application UI
     * @description Creates a tabview with Status, Analytics, and Logs tabs
     */
    void create_status_column(lv_obj_t* left_column);
    void status_column_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* STATUS_COLUMN_H */