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
bool parse_smart_solar(struct json_object *sensor_obj, smart_solar_t *solar_data) {
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
            solar_data->battery_charging_current = json_object_get_double(value_obj);
        } 
        else if (strcmp(entity_name, "battery_voltage") == 0) {
            solar_data->battery_voltage = json_object_get_double(value_obj);
        }
        else if (strcmp(entity_name, "charge_state") == 0) {
            solar_data->charge_state = json_object_get_int(value_obj);
        }
        else if (strcmp(entity_name, "solar_power") == 0) {
            solar_data->solar_power = json_object_get_double(value_obj);
        }
        else if (strcmp(entity_name, "yield_today") == 0) {
            solar_data->yield_today = json_object_get_double(value_obj);
        }
    }
    
    return true;
}

/**
 * Parse SmartShunt sensor data
 */
bool parse_smart_shunt(struct json_object *sensor_obj, smart_shunt_t *shunt_data) {
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
            shunt_data->voltage = json_object_get_double(value_obj);
        } 
        else if (strcmp(entity_name, "current") == 0) {
            shunt_data->current = json_object_get_double(value_obj);
        }
        else if (strcmp(entity_name, "remaining_mins") == 0) {
            shunt_data->remaining_mins = json_object_get_int(value_obj);
        }
        else if (strcmp(entity_name, "soc") == 0) {
            shunt_data->soc = json_object_get_double(value_obj);
        }
        else if (strcmp(entity_name, "consumed_ah") == 0) {
            shunt_data->consumed_ah = json_object_get_double(value_obj);
        }
    }
    
    return true;
}

/**
 * Parse climate sensor data (used for both inside and outside)
 */
bool parse_climate_sensor(struct json_object *sensor_obj, climate_sensor_t *climate) {
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
                temp_camper.household_voltage = atof(state_str) / 1000.0f;  // Assuming millivolts
                break;
                
            case 18:  // starter_voltage
                temp_camper.starter_voltage = atof(state_str) / 1000.0f;  // Assuming millivolts
                break;
                
            case 19:  // mains_voltage
                temp_camper.mains_voltage = atof(state_str);
                break;
                
            case 20:  // household_state
                temp_camper.household_state = (strcmp(state_str, "ON") == 0);
                break;
                
            case 21:  // water_state (as percentage 0-100)
                temp_camper.water_state = atoi(state_str);
                break;
                
            case 22:  // waste_state (as percentage 0-100)
                temp_camper.waste_state = atoi(state_str);
                break;
                
            case 23:  // pump_state
                temp_camper.pump_state = (strcmp(state_str, "ON") == 0);
                break;
                
            default:
                // Unknown entity_id, just ignore
                break;
        }
    }
    
    // Copy the temp structure to the output parameter
    memcpy(camper_data, &temp_camper, sizeof(camper_sensor_t));
    
    // Clean up
    json_object_put(parsed_json);
    
    return true;
}
