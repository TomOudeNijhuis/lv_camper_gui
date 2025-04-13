/*******************************************************************
 *
 * ui.h - User interface definitions for the Camper GUI
 *
 ******************************************************************/
#ifndef UI_H
#define UI_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Creates the main application UI
     * @description Creates a tabview with Status, Analytics, and Logs tabs
     */
    void create_ui(void);

    /**
     * Enter sleep mode - turn off display
     */
    void ui_enter_sleep_mode(void);

    /**
     * Exit sleep mode - turn on display
     */
    void ui_exit_sleep_mode(void);

    /**
     * Check if display is in sleep mode
     */
    bool ui_is_sleeping(void);

    /**
     * Reset the inactivity timer
     */
    void ui_reset_inactivity_timer(void);

    /**
     * Clean up UI resources before exiting
     */
    void ui_cleanup(void);

    /**
     * Print memory usage statistics for debugging
     */
    void ui_print_memory_usage(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */