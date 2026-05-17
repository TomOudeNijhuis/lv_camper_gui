/*******************************************************************
 *
 * errors_tab.h - Errors tab UI: list active firmware errors and clear them
 *
 ******************************************************************/
#ifndef ERRORS_TAB_H
#define ERRORS_TAB_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Build the Errors tab inside the given parent (a tab page returned by
     * lv_tabview_add_tab). The tabview pointer and tab index are needed so the
     * tab label can be updated with a badge when errors are active.
     */
    void create_errors_tab(lv_obj_t* parent, lv_obj_t* tabview, uint32_t tab_index);

#ifdef __cplusplus
}
#endif

#endif /* ERRORS_TAB_H */
