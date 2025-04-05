#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "../lib/http_client.h"
#include "../lib/logger.h"
#include "camper_data.h"

// Global variables to hold the current sensor data
static smart_solar_t smart_solar = {0};
static smart_shunt_t smart_shunt = {0};
static climate_sensor_t inside_climate = {0};
static climate_sensor_t outside_climate = {0};
static camper_sensor_t camper = {0};

// Private function prototypes
static bool parse_smart_solar(struct json_object *sensor_obj);
static bool parse_smart_shunt(struct json_object *sensor_obj);
static bool parse_climate_sensor(struct json_object *sensor_obj, climate_sensor_t *sensor);
static bool parse_camper_sensor(struct json_object *sensor_obj);

/**
 * Fetch camper data from the server and update local state
 * @return 0 on success, non-zero on failure
 */
int fetch_camper_data(void) {
    struct json_object *parsed_json;
    // Updated URL to fetch only camper states
    http_response_t response = http_get("http://camperpi.local:8000/sensors/5/states/", 5);
    
    if (!response.success) {
        log_error("Failed to fetch camper data: %s", response.error);
        if (response.body && *response.body) {
            log_error("Response body: %s", response.body);
        }
        http_response_free(&response);
        return -1;
    }
    
    // Parse JSON response (array of states)
    parsed_json = json_tokener_parse(response.body);
    if (parsed_json == NULL) {
        log_error("Failed to parse JSON response");
        http_response_free(&response);
        return -1;
    }
    
    if (!json_object_is_type(parsed_json, json_type_array)) {
        log_error("Expected JSON array, got something else");
        json_object_put(parsed_json);
        http_response_free(&response);
        return -1;
    }
    
    // Iterate through states in the array
    int array_len = json_object_array_length(parsed_json);
    for (int i = 0; i < array_len; i++) {
        struct json_object *state_obj = json_object_array_get_idx(parsed_json, i);
        struct json_object *entity_id_obj, *state_value_obj;
        
        if (!json_object_object_get_ex(state_obj, "entity_id", &entity_id_obj) ||
            !json_object_object_get_ex(state_obj, "state", &state_value_obj)) {
            continue;  // Skip if missing required fields
        }
        
        int entity_id = json_object_get_int(entity_id_obj);
        const char *state_str = json_object_get_string(state_value_obj);
        
        // Map entity_id to the appropriate field in the camper structure
        switch (entity_id) {
            case 17:  // household_voltage
                camper.household_voltage = atof(state_str) / 1000.0f;  // Assuming millivolts
                break;
                
            case 18:  // starter_voltage
                camper.starter_voltage = atof(state_str) / 1000.0f;  // Assuming millivolts
                break;
                
            case 19:  // mains_voltage
                camper.mains_voltage = atof(state_str);
                break;
                
            case 20:  // household_state
                camper.household_state = (strcmp(state_str, "ON") == 0);
                break;
                
            case 21:  // water_state (as percentage 0-100)
                camper.water_state = atoi(state_str);
                break;
                
            case 22:  // waste_state (as percentage 0-100)
                camper.waste_state = atoi(state_str);
                break;
                
            case 23:  // pump_state
                camper.pump_state = (strcmp(state_str, "ON") == 0);
                break;
                
            default:
                // Unknown entity_id, just ignore
                break;
        }
    }
    
    // Clean up
    json_object_put(parsed_json);
    http_response_free(&response);
    
    // Debug output
    log_debug("Camper data updated: household_v=%.2f, starter_v=%.2f, mains_v=%.2f", 
             camper.household_voltage, camper.starter_voltage, camper.mains_voltage);
    log_debug("States: household=%s, pump=%s, water=%d%%, waste=%d%%",
             camper.household_state ? "ON" : "OFF",
             camper.pump_state ? "ON" : "OFF",
             camper.water_state, camper.waste_state);
    
    return 0;
}

int set_camper_action(const int entity_id, const char *status) {
    // Prepare the JSON payload
    char json_payload[64];
    snprintf(json_payload, sizeof(json_payload), "{\"state\": \"%s\"}", status);
    
    // Prepare the URL
    char api_url[64];
    snprintf(api_url, sizeof(api_url), "http://camperpi.local:8000/action/%d", entity_id);

    // Make the POST request
    http_response_t response = http_post_json(api_url, json_payload, 5);
    
    // Log the result
    if (!response.success) {
        log_error("Failed to update switch status: %s", response.error);
        if (response.body && *response.body) {
            log_error("Response body: %s", response.body);
        }
    } else {
        log_info("Switch status updated successfully");
        log_debug("Response: %s", response.body);
    }
    
    // Free the response
    http_response_free(&response);
    
    return response.success ? 0 : 1;
}

camper_sensor_t* get_camper_data(void) {
    return &camper;
}


int fetch_system_data(void) {
    struct json_object *parsed_json;
    http_response_t response = http_get("http://camperpi.local:8000/sensors", 5);
    
    if (!response.success) {
        log_error("Failed to fetch sensor data: %s", response.error);
        if (response.body && *response.body) {
            log_error("Response body: %s", response.body);
        }
        http_response_free(&response);
        return -1;
    }
    
    // Parse JSON response (array of sensors)
    parsed_json = json_tokener_parse(response.body);
    if (parsed_json == NULL) {
        log_error("Failed to parse JSON response");
        http_response_free(&response);
        return -1;
    }
    
    if (!json_object_is_type(parsed_json, json_type_array)) {
        log_error("Expected JSON array, got something else");
        json_object_put(parsed_json);
        http_response_free(&response);
        return -1;
    }
    
    // Iterate through sensors in the array
    int array_len = json_object_array_length(parsed_json);
    for (int i = 0; i < array_len; i++) {
        struct json_object *sensor_obj = json_object_array_get_idx(parsed_json, i);
        struct json_object *name_obj;
        
        if (!json_object_object_get_ex(sensor_obj, "name", &name_obj)) {
            continue;  // Skip if no name
        }
        
        const char *sensor_name = json_object_get_string(name_obj);
        
        // Process each sensor based on its name
        if (strcmp(sensor_name, "SmartSolar") == 0) {
            parse_smart_solar(sensor_obj);
        } 
        else if (strcmp(sensor_name, "SmartShunt") == 0) {
            parse_smart_shunt(sensor_obj);
        }
        else if (strcmp(sensor_name, "inside") == 0) {
            parse_climate_sensor(sensor_obj, &inside_climate);
        }
        else if (strcmp(sensor_name, "outside") == 0) {
            parse_climate_sensor(sensor_obj, &outside_climate);
        }
    }
    
    // Clean up
    json_object_put(parsed_json);
    http_response_free(&response);
    return 0;
}

/**
 * Parse SmartSolar sensor data
 */
static bool parse_smart_solar(struct json_object *sensor_obj) {
    struct json_object *entities_obj, *entity_obj, *name_obj, *value_obj;
    
    if (!json_object_object_get_ex(sensor_obj, "entities", &entities_obj) || 
        !json_object_is_type(entities_obj, json_type_array)) {
        return false;
    }
    
    int entities_len = json_object_array_length(entities_obj);
    
    // Loop through all entities in the SmartSolar sensor
    for (int i = 0; i < entities_len; i++) {
        entity_obj = json_object_array_get_idx(entities_obj, i);
        
        if (!json_object_object_get_ex(entity_obj, "name", &name_obj) ||
            !json_object_object_get_ex(entity_obj, "value", &value_obj)) {
            continue;
        }
        
        const char *entity_name = json_object_get_string(name_obj);
        
        // Map entity values to our struct
        if (strcmp(entity_name, "battery_charging_current") == 0) {
            smart_solar.battery_charging_current = json_object_get_double(value_obj);
        } 
        else if (strcmp(entity_name, "battery_voltage") == 0) {
            smart_solar.battery_voltage = json_object_get_double(value_obj);
        }
        else if (strcmp(entity_name, "charge_state") == 0) {
            smart_solar.charge_state = json_object_get_int(value_obj);
        }
        else if (strcmp(entity_name, "solar_power") == 0) {
            smart_solar.solar_power = json_object_get_double(value_obj);
        }
        else if (strcmp(entity_name, "yield_today") == 0) {
            smart_solar.yield_today = json_object_get_double(value_obj);
        }
    }
    
    return true;
}

/**
 * Parse SmartShunt sensor data
 */
static bool parse_smart_shunt(struct json_object *sensor_obj) {
    struct json_object *entities_obj, *entity_obj, *name_obj, *value_obj;
    
    if (!json_object_object_get_ex(sensor_obj, "entities", &entities_obj) || 
        !json_object_is_type(entities_obj, json_type_array)) {
        return false;
    }
    
    int entities_len = json_object_array_length(entities_obj);
    
    // Loop through all entities in the SmartShunt sensor
    for (int i = 0; i < entities_len; i++) {
        entity_obj = json_object_array_get_idx(entities_obj, i);
        
        if (!json_object_object_get_ex(entity_obj, "name", &name_obj) ||
            !json_object_object_get_ex(entity_obj, "value", &value_obj)) {
            continue;
        }
        
        const char *entity_name = json_object_get_string(name_obj);
        
        // Map entity values to our struct
        if (strcmp(entity_name, "voltage") == 0) {
            smart_shunt.voltage = json_object_get_double(value_obj);
        } 
        else if (strcmp(entity_name, "current") == 0) {
            smart_shunt.current = json_object_get_double(value_obj);
        }
        else if (strcmp(entity_name, "remaining_mins") == 0) {
            smart_shunt.remaining_mins = json_object_get_int(value_obj);
        }
        else if (strcmp(entity_name, "soc") == 0) {
            smart_shunt.soc = json_object_get_double(value_obj);
        }
        else if (strcmp(entity_name, "consumed_ah") == 0) {
            smart_shunt.consumed_ah = json_object_get_double(value_obj);
        }
    }
    
    return true;
}

/**
 * Parse climate sensor data (used for both inside and outside)
 */
static bool parse_climate_sensor(struct json_object *sensor_obj, climate_sensor_t *climate) {
    struct json_object *entities_obj, *entity_obj, *name_obj, *value_obj;
    
    if (!json_object_object_get_ex(sensor_obj, "entities", &entities_obj) || 
        !json_object_is_type(entities_obj, json_type_array)) {
        return false;
    }
    
    int entities_len = json_object_array_length(entities_obj);
    
    // Loop through all entities in the climate sensor
    for (int i = 0; i < entities_len; i++) {
        entity_obj = json_object_array_get_idx(entities_obj, i);
        
        if (!json_object_object_get_ex(entity_obj, "name", &name_obj) ||
            !json_object_object_get_ex(entity_obj, "value", &value_obj)) {
            continue;
        }
        
        const char *entity_name = json_object_get_string(name_obj);
        
        // Map entity values to our struct
        if (strcmp(entity_name, "battery") == 0) {
            climate->battery = json_object_get_double(value_obj);
        } 
        else if (strcmp(entity_name, "temperature") == 0) {
            climate->temperature = json_object_get_double(value_obj);
        }
        else if (strcmp(entity_name, "humidity") == 0) {
            climate->humidity = json_object_get_double(value_obj);
        }
    }
    
    return true;
}

// Getter functions to access the sensor data from outside this file

smart_solar_t* get_smart_solar_data(void) {
    return &smart_solar;
}

smart_shunt_t* get_smart_shunt_data(void) {
    return &smart_shunt;
}

climate_sensor_t* get_inside_climate_data(void) {
    return &inside_climate;
}

climate_sensor_t* get_outside_climate_data(void) {
    return &outside_climate;
}

