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
extern "C"
{
#endif

#define REQUEST_TIMEOUT_SECONDS 30         // Discard requests older than 30 seconds
#define MAX_URL_LENGTH 256                 /* Maximum URL length */
#define MAX_JSON_ACTION_PAYLOAD_LENGTH 128 /* Maximum JSON payload length */

    typedef enum
    {
        FETCH_CAMPER_DATA = 0,
        FETCH_SYSTEM_DATA,
        FETCH_SMART_SOLAR,
        FETCH_SMART_SHUNT,
        FETCH_CLIMATE_INSIDE,
        FETCH_CLIMATE_OUTSIDE,
        FETCH_ENTITY_HISTORY,
        FETCH_TYPE_COUNT // Keep this last - used to validate request types
    } fetch_request_type_t;

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
    bool request_data_fetch(fetch_request_type_t request_type);

    /**
     * Request a camper action in the background
     */
    void request_camper_action(const char* entity_name, const char* status);

    /**
     * Get a copy of the current camper data
     */
    camper_sensor_t* get_camper_data(void);

    bool update_camper_entity(const char* entity_name, const char* status);

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

    void clear_entity_history(entity_history_t* history);

    /**
     * Request entity history data
     */
    bool request_entity_history(const char* sensor_name, const char* entity_name,
                                const char* interval, int samples);

    /**
     * Get a copy of the current entity history data
     */
    entity_history_t* get_entity_history_data(void);

    /**
     * Free the memory allocated for entity history data
     */
    void free_entity_history_data(entity_history_t* history);

    /**
     * History request parameters
     */
    typedef struct
    {
        char sensor_name[32];
        char entity_name[32];
        char interval[16];
        int  samples;
    } history_request_t;

#ifdef __cplusplus
}
#endif

#endif /* DATA_MANAGER_H */
