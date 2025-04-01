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
void ui_enter_sleep_mode(void);
void ui_exit_sleep_mode(void);
bool ui_is_sleeping(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */