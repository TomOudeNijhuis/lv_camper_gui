/*******************************************************************
 *
 * lv_awesome_16.h - Custom FontAwesome symbols for the Camper GUI
 *
 ******************************************************************/
#ifndef LV_AWESOME_16_H
#define LV_AWESOME_16_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl/lvgl.h"

/* Declare the custom font */
#if LVGL_VERSION_MAJOR >= 8
    extern const lv_font_t lv_awesome_16;
#else
extern lv_font_t lv_awesome_16;
#endif

/* FontAwesome sun icon (Unicode: f185) */
#define LV_SYMBOL_SUN "\xEF\x86\x85"

/* FontAwesome moon icon (Unicode: f186) */
#define LV_SYMBOL_MOON "\xEF\x86\x86"

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_AWESOME_16_H */
