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

/* FontAwesome power icon (Unicode: f011) */
#define LV_SYMBOL_POWER "\xEF\x80\x91"

/* FontAwesome arrow up icon (Unicode: f062) */
#define LV_SYMBOL_ARROW_UP "\xEF\x81\xA2"

/* FontAwesome arrow up square icon (Unicode: f0d8) */
#define LV_SYMBOL_ARROW_UP_SQUARE "\xEF\x83\x98"

/* FontAwesome lightning icon (Unicode: f0e7) */
#define LV_SYMBOL_LIGHTNING "\xEF\x83\xA7"

/* FontAwesome arrow up thin icon (Unicode: f106) */
#define LV_SYMBOL_ARROW_UP_THIN "\xEF\x84\x86"

/* FontAwesome sun icon (Unicode: f185) */
#define LV_SYMBOL_SUN "\xEF\x86\x85"

/* FontAwesome moon icon (Unicode: f186) */
#define LV_SYMBOL_MOON "\xEF\x86\x86"

/* FontAwesome battery icons (Unicode: f240-f244) */
#define LV_SYMBOL_BATTERY_FULL "\xEF\x89\x80"
#define LV_SYMBOL_BATTERY_THREE_QUARTERS "\xEF\x89\x81"
#define LV_SYMBOL_BATTERY_HALF "\xEF\x89\x82"
#define LV_SYMBOL_BATTERY_QUARTER "\xEF\x89\x83"
#define LV_SYMBOL_BATTERY_EMPTY "\xEF\x89\x84"

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_AWESOME_16_H */
