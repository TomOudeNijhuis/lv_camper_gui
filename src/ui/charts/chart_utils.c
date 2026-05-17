/*******************************************************************
 *
 * chart_utils.c - Shared helpers for the energy / solar / temperature charts
 *
 ******************************************************************/
#include "chart_utils.h"

#include <stdio.h>
#include <string.h>

#include "../../lib/logger.h"

void chart_format_timestamp(const char* iso_timestamp, char* output, size_t output_size)
{
    if(iso_timestamp == NULL || output == NULL || output_size < 12)
    {
        if(output && output_size > 0) output[0] = '\0';
        return;
    }

    // ISO format: YYYY-MM-DDThh:mm:ss → "DD-MM HH:MM"
    if(strlen(iso_timestamp) >= 16 && iso_timestamp[4] == '-' && iso_timestamp[7] == '-' &&
       iso_timestamp[10] == 'T')
    {
        char day[3]   = {iso_timestamp[8], iso_timestamp[9], '\0'};
        char month[3] = {iso_timestamp[5], iso_timestamp[6], '\0'};
        char time[6]  = {iso_timestamp[11], iso_timestamp[12], ':',
                         iso_timestamp[14], iso_timestamp[15], '\0'};

        snprintf(output, output_size, "%s-%s %s", day, month, time);
    }
    else
    {
        strncpy(output, iso_timestamp, output_size - 1);
        output[output_size - 1] = '\0';
    }
}

lv_obj_t* chart_create_line(lv_obj_t* parent, const char* line_name, lv_color_t color,
                            lv_point_precise_t* points)
{
    if(parent == NULL || points == NULL)
    {
        log_error("Cannot create %s: Invalid parent or points", line_name);
        return NULL;
    }

    lv_obj_t* line = lv_line_create(parent);
    if(line == NULL)
    {
        log_error("Failed to create %s", line_name);
        return NULL;
    }

    lv_line_set_points(line, points, 2);
    lv_obj_set_style_line_width(line, 1, 0);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_dash_width(line, 3, 0);
    lv_obj_set_style_line_dash_gap(line, 3, 0);

    return line;
}

lv_obj_t* chart_create_value_label(lv_obj_t* parent, const char* label_name, lv_color_t color,
                                   float value, const char* unit_suffix, lv_align_t alignment,
                                   lv_coord_t x_offset, lv_coord_t y_offset)
{
    if(parent == NULL)
    {
        log_error("Cannot create %s: Invalid parent", label_name);
        return NULL;
    }

    lv_obj_t* label = lv_label_create(parent);
    if(label == NULL)
    {
        log_error("Failed to create %s", label_name);
        return NULL;
    }

    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text_fmt(label, "%.1f %s", value, unit_suffix ? unit_suffix : "");
    lv_obj_align(label, alignment, x_offset, y_offset);

    return label;
}
