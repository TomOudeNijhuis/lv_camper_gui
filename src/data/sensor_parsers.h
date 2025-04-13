/*******************************************************************
 *
 * sensor_parsers.h - Functions to parse camper sensor data
 *
 ******************************************************************/
#ifndef SENSOR_PARSERS_H
#define SENSOR_PARSERS_H

#include <stdbool.h>
#include <json-c/json.h>
#include "sensor_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Parse SmartSolar sensor data
     */
    bool parse_smart_solar(const char* json_str, smart_solar_t* solar_data);

    /**
     * Parse SmartShunt sensor data
     */
    bool parse_smart_shunt(const char* json_str, smart_shunt_t* shunt_data);

    /**
     * Parse climate sensor data
     */
    bool parse_climate_sensor(const char* json_str, climate_sensor_t* climate);

    /**
     * Parse camper sensor data from JSON response
     */
    bool parse_camper_states(const char* json_str, camper_sensor_t* camper_data);

    /**
     * Parse entity history data
     */
    bool parse_entity_history(const char* json_str, entity_history_t* history);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_PARSERS_H */
