/*******************************************************************
 *
 * sensor_types.h - Type definitions for camper sensors
 *
 ******************************************************************/
#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        float battery_voltage;
        float battery_charging_current;
        float solar_power;
        float yield_today;
        char  charge_state[32];
        bool  valid;
    } smart_solar_t;

    typedef struct
    {
        float voltage;
        float current;
        int   remaining_mins;
        float soc;
        float consumed_ah;
        bool  valid;
    } smart_shunt_t;

    typedef struct
    {
        float temperature;
        float humidity;
        float battery;
        bool  valid;
    } climate_sensor_t;

    typedef struct
    {
        float household_voltage;
        float starter_voltage;
        float mains_voltage;
        bool  household_state;
        bool  pump_state;
        int   water_state; // Percentage 0-100
        int   waste_state; // Percentage 0-100
        bool  valid;
    } camper_sensor_t;

    /**
     * Structure for entity history data
     */
    typedef struct
    {
        char   entity_name[64];
        char   unit[16];
        bool   is_numeric;
        int    count;
        char** timestamps;
        float* min;
        float* max;
        float* mean;
        bool   valid;
    } entity_history_t;

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_TYPES_H */
