#include "lvgl/lvgl.h"
#include "../lib/logger.h"
#include "ui.h"

#include "charts/temp_chart.h"
#include "charts/battery_chart.h"
#include "charts/solar_chart.h"

void update_long_timer_cb(lv_timer_t* timer)
{
    static int fetch_state    = 0;
    bool       result         = false;
    static int fetch_interval = 0;
    static int fetch_valid    = 0;
    static int fetch_attempts = 0; // Track consecutive fetch attempts

    if(ui_is_sleeping())
    {
        fetch_state    = 0;
        fetch_interval = 0;
        fetch_attempts = 0;
        return;
    }

    if(fetch_interval > 0)
    {
        fetch_interval -= 1;
        return;
    }

    if(fetch_state == 0)
    {
        result = fetch_internal_climate();
        if(result)
            fetch_valid = 1;
        else
            fetch_attempts++;

        fetch_state = 1;
    }
    else if(fetch_state == 1)
    {
        result = update_internal_climate_chart();
        if(result)
            fetch_valid += 1;
        else
            fetch_attempts++;

        fetch_state = 2;
    }
    else if(fetch_state == 2)
    {
        result = fetch_external_climate();
        if(result)
            fetch_valid += 1;
        else
            fetch_attempts++;

        fetch_state = 3;
    }
    else if(fetch_state == 3)
    {
        result = update_external_climate_chart();
        if(result)
            fetch_valid += 1;
        else
            fetch_attempts++;

        fetch_state = 4;
    }
    else if(fetch_state == 4)
    {
        result = fetch_solar();
        if(result)
            fetch_valid += 1;
        else
            fetch_attempts++;

        fetch_state = 5;
    }
    else if(fetch_state == 5)
    {
        result = update_solar_chart();
        if(result)
            fetch_valid += 1;
        else
            fetch_attempts++;

        fetch_state = 6;
    }
    else if(fetch_state == 6)
    {
        result = fetch_battery_consumption();
        if(result)
            fetch_valid += 1;
        else
            fetch_attempts++;

        fetch_state = 7;
    }
    else if(fetch_state == 7)
    {
        result = update_energy_chart();
        if(result)
            fetch_valid += 1;
        else
            fetch_attempts++;

        fetch_state = 0;

        // If we've had too many failed fetch attempts, reset the charts
        if(fetch_attempts >= 3)
            fetch_attempts = 0;

        fetch_valid = 0;
        if(fetch_valid >= 5)
            fetch_interval = 5;
        else
            fetch_interval = 0;
    }
}

void chart_cleanup()
{
    temp_chart_cleanup();
    battery_chart_cleanup();
    solar_chart_cleanup();
}