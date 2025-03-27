/*******************************************************************
 *
 * ui.h - User interface definitions for the Camper GUI
 *
 ******************************************************************/
#ifndef UI_H
#define UI_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates the main application UI
 * @description Creates a tabview with Status, Analytics, and Logs tabs
 */
void create_ui(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */