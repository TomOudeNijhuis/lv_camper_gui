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
extern "C" {
#endif

/**
 * Parse SmartSolar sensor data
 */
bool parse_smart_solar(struct json_object *sensor_obj, smart_solar_t *solar_data);

/**
 * Parse SmartShunt sensor data
 */
bool parse_smart_shunt(struct json_object *sensor_obj, smart_shunt_t *shunt_data);

/**
 * Parse climate sensor data
 */
bool parse_climate_sensor(struct json_object *sensor_obj, climate_sensor_t *climate);

/**
 * Parse camper sensor data from JSON response
 */
bool parse_camper_states(const char *json_str, camper_sensor_t *camper_data);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_PARSERS_H */
