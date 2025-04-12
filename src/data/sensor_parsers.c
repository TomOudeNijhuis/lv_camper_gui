/*******************************************************************
 *
 * sensor_parsers.c - Functions to parse camper sensor data
 *
 ******************************************************************/
#include <stdlib.h>
#include <string.h>
#include "sensor_parsers.h"
#include "../lib/logger.h"

/**
 * Parse SmartSolar sensor data
 */
bool parse_smart_solar(const char *json_str, smart_solar_t *solar_data) {
    if (!json_str || !solar_data) {
        return false;
    }
    
    struct json_object *parsed_json = json_tokener_parse(json_str);
    if (!parsed_json) {
        log_error("Failed to parse smart solar JSON");
        return false;
    }
    
    if (!json_object_is_type(parsed_json, json_type_array)) {
        log_error("Expected JSON array for smart solar data");
        json_object_put(parsed_json);
        return false;
    }
    
    // Create a temporary solar data structure
    smart_solar_t temp_solar = {0};
    
    // Iterate through entities in the array
    int array_len = json_object_array_length(parsed_json);
    for (int i = 0; i < array_len; i++) {
        struct json_object *entity_obj = json_object_array_get_idx(parsed_json, i);
        struct json_object *entity_name_obj, *state_value_obj;
        
        if (!json_object_object_get_ex(entity_obj, "entity_name", &entity_name_obj) ||
            !json_object_object_get_ex(entity_obj, "state", &state_value_obj)) {
            continue;  // Skip if missing required fields
        }
        
        const char *entity_name = json_object_get_string(entity_name_obj);
        const char *state_str = json_object_get_string(state_value_obj);
        
        // Map entity values to our struct
        if (strcmp(entity_name, "battery_charging_current") == 0) {
            temp_solar.battery_charging_current = atof(state_str);
        } 
        else if (strcmp(entity_name, "battery_voltage") == 0) {
            temp_solar.battery_voltage = atof(state_str);
        }
        else if (strcmp(entity_name, "charge_state") == 0) {
            strncpy(temp_solar.charge_state, state_str, sizeof(temp_solar.charge_state) - 1);
            temp_solar.charge_state[sizeof(temp_solar.charge_state) - 1] = '\0';
        }
        else if (strcmp(entity_name, "solar_power") == 0) {
            temp_solar.solar_power = atof(state_str);
        }
        else if (strcmp(entity_name, "yield_today") == 0) {
            temp_solar.yield_today = atof(state_str);
        }
    }
    
    // Copy the temp structure to the output parameter
    memcpy(solar_data, &temp_solar, sizeof(smart_solar_t));
    
    // Clean up
    json_object_put(parsed_json);
    
    return true;
}

/**
 * Parse SmartShunt sensor data
 */
bool parse_smart_shunt(const char *json_str, smart_shunt_t *shunt_data) {
    if (!json_str || !shunt_data) {
        return false;
    }
    
    struct json_object *parsed_json = json_tokener_parse(json_str);
    if (!parsed_json) {
        log_error("Failed to parse smart shunt JSON");
        return false;
    }
    
    if (!json_object_is_type(parsed_json, json_type_array)) {
        log_error("Expected JSON array for smart shunt data");
        json_object_put(parsed_json);
        return false;
    }
    
    // Create a temporary shunt data structure
    smart_shunt_t temp_shunt = {0};
    
    // Iterate through entities in the array
    int array_len = json_object_array_length(parsed_json);
    for (int i = 0; i < array_len; i++) {
        struct json_object *entity_obj = json_object_array_get_idx(parsed_json, i);
        struct json_object *entity_name_obj, *state_value_obj;
        
        if (!json_object_object_get_ex(entity_obj, "entity_name", &entity_name_obj) ||
            !json_object_object_get_ex(entity_obj, "state", &state_value_obj)) {
            continue;  // Skip if missing required fields
        }
        
        const char *entity_name = json_object_get_string(entity_name_obj);
        const char *state_str = json_object_get_string(state_value_obj);
        
        // Map entity values to our struct
        if (strcmp(entity_name, "voltage") == 0) {
            temp_shunt.voltage = atof(state_str);
        } 
        else if (strcmp(entity_name, "current") == 0) {
            temp_shunt.current = atof(state_str);
        }
        else if (strcmp(entity_name, "remaining_mins") == 0) {
            temp_shunt.remaining_mins = atoi(state_str);
        }
        else if (strcmp(entity_name, "soc") == 0) {
            temp_shunt.soc = atof(state_str);
        }
        else if (strcmp(entity_name, "consumed_ah") == 0) {
            temp_shunt.consumed_ah = atof(state_str);
        }
    }
    
    // Copy the temp structure to the output parameter
    memcpy(shunt_data, &temp_shunt, sizeof(smart_shunt_t));
    
    // Clean up
    json_object_put(parsed_json);
    
    return true;
}

/**
 * Parse climate sensor data (used for both inside and outside)
 */
bool parse_climate_sensor(const char *json_str, climate_sensor_t *climate) {
    if (!json_str || !climate) {
        return false;
    }
    
    struct json_object *parsed_json = json_tokener_parse(json_str);
    if (!parsed_json) {
        log_error("Failed to parse climate sensor JSON");
        return false;
    }
    
    if (!json_object_is_type(parsed_json, json_type_array)) {
        log_error("Expected JSON array for climate sensor data");
        json_object_put(parsed_json);
        return false;
    }
    
    // Create a temporary climate data structure 
    climate_sensor_t temp_climate = {0};
    
    // Iterate through entities in the array
    int array_len = json_object_array_length(parsed_json);
    for (int i = 0; i < array_len; i++) {
        struct json_object *entity_obj = json_object_array_get_idx(parsed_json, i);
        struct json_object *entity_name_obj, *state_value_obj;
        
        if (!json_object_object_get_ex(entity_obj, "entity_name", &entity_name_obj) ||
            !json_object_object_get_ex(entity_obj, "state", &state_value_obj)) {
            continue;  // Skip if missing required fields
        }
        
        const char *entity_name = json_object_get_string(entity_name_obj);
        const char *state_str = json_object_get_string(state_value_obj);
        
        // Map entity_name to the appropriate field in the climate structure
        if (strcmp(entity_name, "battery") == 0) {
            temp_climate.battery = atof(state_str);
        } 
        else if (strcmp(entity_name, "temperature") == 0) {
            temp_climate.temperature = atof(state_str);
        }
        else if (strcmp(entity_name, "humidity") == 0) {
            temp_climate.humidity = atof(state_str);
        }
    }
    
    // Copy the temp structure to the output parameter
    memcpy(climate, &temp_climate, sizeof(climate_sensor_t));
    
    // Clean up
    json_object_put(parsed_json);
    
    return true;
}

/**
 * Parse camper sensor states from JSON response
 */
bool parse_camper_states(const char *json_str, camper_sensor_t *camper_data) {
    if (!json_str || !camper_data) {
        return false;
    }
    
    struct json_object *parsed_json = json_tokener_parse(json_str);
    if (!parsed_json) {
        log_error("Failed to parse camper states JSON");
        return false;
    }
    
    if (!json_object_is_type(parsed_json, json_type_array)) {
        log_error("Expected JSON array for camper states");
        json_object_put(parsed_json);
        return false;
    }
    
    // Create a temporary camper data structure 
    camper_sensor_t temp_camper = {0};
    
    // Iterate through states in the array
    int array_len = json_object_array_length(parsed_json);
    for (int i = 0; i < array_len; i++) {
        struct json_object *state_obj = json_object_array_get_idx(parsed_json, i);
        struct json_object *entity_name_obj, *state_value_obj;
        
        if (!json_object_object_get_ex(state_obj, "entity_name", &entity_name_obj) ||
            !json_object_object_get_ex(state_obj, "state", &state_value_obj)) {
            continue;  // Skip if missing required fields
        }
        
        const char *entity_name = json_object_get_string(entity_name_obj);
        const char *state_str = json_object_get_string(state_value_obj);
        
        // Map entity_name to the appropriate field in the camper structure
        if (strcmp(entity_name, "household_voltage") == 0) {
            temp_camper.household_voltage = atof(state_str) / 1000.0f;  // Assuming millivolts
        }
        else if (strcmp(entity_name, "starter_voltage") == 0) {
            temp_camper.starter_voltage = atof(state_str) / 1000.0f;  // Assuming millivolts
        }
        else if (strcmp(entity_name, "mains_voltage") == 0) {
            temp_camper.mains_voltage = atof(state_str);
        }
        else if (strcmp(entity_name, "household_state") == 0) {
            temp_camper.household_state = (strcmp(state_str, "ON") == 0);
        }
        else if (strcmp(entity_name, "water_level") == 0) {
            temp_camper.water_state = atoi(state_str);
        }
        else if (strcmp(entity_name, "waste_level") == 0) {
            temp_camper.waste_state = atoi(state_str);
        }
        else if (strcmp(entity_name, "pump_state") == 0) {
            temp_camper.pump_state = (strcmp(state_str, "ON") == 0);
        }
    }
    
    // Copy the temp structure to the output parameter
    memcpy(camper_data, &temp_camper, sizeof(camper_sensor_t));
    
    // Clean up
    json_object_put(parsed_json);
    
    return true;
}
