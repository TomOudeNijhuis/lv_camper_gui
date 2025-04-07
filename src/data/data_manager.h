/*******************************************************************
 *
 * data_manager.h - Central data manager for camper sensor data
 *
 ******************************************************************/
#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <stdbool.h>
#include "sensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the background worker system
 * @return 0 on success, non-zero on failure
 */
int init_background_fetcher(void);

/**
 * Shutdown the background worker
 */
void shutdown_background_fetcher(void);

/**
 * Check if any background operation is in progress
 */
bool is_background_busy(void);

/**
 * Request a new fetch operation
 */
void request_camper_data_fetch(void);

/**
 * Request a camper action in the background
 */
void request_camper_action(const int entity_id, const char *status);

/**
 * Get a copy of the current camper data
 */
camper_sensor_t* get_camper_data(void);

/**
 * Get a copy of the current SmartSolar data
 */
smart_solar_t* get_smart_solar_data(void);

/**
 * Get a copy of the current SmartShunt data
 */
smart_shunt_t* get_smart_shunt_data(void);

/**
 * Get a copy of the current inside climate data
 */
climate_sensor_t* get_inside_climate_data(void);

/**
 * Get a copy of the current outside climate data
 */
climate_sensor_t* get_outside_climate_data(void);

#ifdef __cplusplus
}
#endif

#endif /* DATA_MANAGER_H */
