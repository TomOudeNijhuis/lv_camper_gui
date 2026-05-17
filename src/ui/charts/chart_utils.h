/*******************************************************************
 *
 * chart_utils.h - Shared helpers for the energy / solar / temperature charts
 *
 ******************************************************************/
#ifndef CHART_UTILS_H
#define CHART_UTILS_H

#include <stddef.h>
#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Format an ISO-8601 timestamp (YYYY-MM-DDThh:mm:ss) as "DD-MM HH:MM".
     * On invalid input, copies up to output_size-1 bytes from iso_timestamp as a
     * fallback. output_size must be at least 12.
     */
    void chart_format_timestamp(const char* iso_timestamp, char* output, size_t output_size);

    /**
     * Create a dashed horizontal line on a chart (used for min/max marker lines).
     * Logs and returns NULL on invalid input.
     */
    lv_obj_t* chart_create_line(lv_obj_t* parent, const char* line_name, lv_color_t color,
                                lv_point_precise_t* points);

    /**
     * Create a small value label aligned to a chart (used for min/max markers).
     * `unit_suffix` is appended via "%.1f <suffix>" format, e.g. "Wh" or "Ah".
     * Logs and returns NULL on invalid input.
     */
    lv_obj_t* chart_create_value_label(lv_obj_t* parent, const char* label_name, lv_color_t color,
                                       float value, const char* unit_suffix, lv_align_t alignment,
                                       lv_coord_t x_offset, lv_coord_t y_offset);

#ifdef __cplusplus
}
#endif

#endif /* CHART_UTILS_H */
