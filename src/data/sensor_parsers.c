/*******************************************************************
 *
 * sensor_parsers.c - Functions to parse camper sensor data
 *
 ******************************************************************/
#include <stdlib.h>
#include <string.h>
#include "sensor_parsers.h"
#include "../lib/logger.h"
#include "data_manager.h"

/**
 * Helper function to get numeric value from JSON object, handling both integer and double types
 */
static float get_json_number(struct json_object* obj)
{
    if(json_object_is_type(obj, json_type_double))
    {
        return (float)json_object_get_double(obj);
    }
    else if(json_object_is_type(obj, json_type_int))
    {
        return (float)json_object_get_int64(obj);
    }
    return 0.0f; // Default value for non-numeric types
}

/**
 * Parse SmartSolar sensor data
 */
bool parse_smart_solar(const char* json_str, smart_solar_t* solar_data)
{
    if(!json_str || !solar_data)
    {
        return false;
    }

    struct json_object* parsed_json = json_tokener_parse(json_str);
    if(!parsed_json)
    {
        log_error("Failed to parse smart solar JSON");
        return false;
    }

    if(!json_object_is_type(parsed_json, json_type_array))
    {
        log_error("Expected JSON array for smart solar data");
        json_object_put(parsed_json);
        return false;
    }

    // Create a temporary solar data structure
    smart_solar_t temp_solar = {0};

    // Counter for required fields
    int field_count = 0;

    // Iterate through entities in the array
    int array_len = json_object_array_length(parsed_json);
    for(int i = 0; i < array_len; i++)
    {
        struct json_object* entity_obj = json_object_array_get_idx(parsed_json, i);
        struct json_object *entity_name_obj, *state_value_obj;

        if(!json_object_object_get_ex(entity_obj, "entity_name", &entity_name_obj) ||
           !json_object_object_get_ex(entity_obj, "state", &state_value_obj))
        {
            continue; // Skip if missing required fields
        }

        const char* entity_name = json_object_get_string(entity_name_obj);
        const char* state_str   = json_object_get_string(state_value_obj);

        // Map entity values to our struct
        if(strcmp(entity_name, "battery_charging_current") == 0)
        {
            temp_solar.battery_charging_current = atof(state_str);
            field_count++;
        }
        else if(strcmp(entity_name, "battery_voltage") == 0)
        {
            temp_solar.battery_voltage = atof(state_str);
            field_count++;
        }
        else if(strcmp(entity_name, "charge_state") == 0)
        {
            strncpy(temp_solar.charge_state, state_str, sizeof(temp_solar.charge_state) - 1);
            temp_solar.charge_state[sizeof(temp_solar.charge_state) - 1] = '\0';
            field_count++;
        }
        else if(strcmp(entity_name, "solar_power") == 0)
        {
            temp_solar.solar_power = atof(state_str);
            field_count++;
        }
        else if(strcmp(entity_name, "yield_today") == 0)
        {
            temp_solar.yield_today = atof(state_str);
            field_count++;
        }
    }

    // Check if we found at least 5 required fields
    if(field_count < 5)
    {
        log_error("Smart solar data must contain at least 5 fields");
        json_object_put(parsed_json);
        return false;
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
bool parse_smart_shunt(const char* json_str, smart_shunt_t* shunt_data)
{
    if(!json_str || !shunt_data)
    {
        return false;
    }

    struct json_object* parsed_json = json_tokener_parse(json_str);
    if(!parsed_json)
    {
        log_error("Failed to parse smart shunt JSON");
        return false;
    }

    if(!json_object_is_type(parsed_json, json_type_array))
    {
        log_error("Expected JSON array for smart shunt data");
        json_object_put(parsed_json);
        return false;
    }

    // Create a temporary shunt data structure
    smart_shunt_t temp_shunt = {0};

    // Counter for processed fields and array to track processed entities
    int  field_count             = 0;
    char processed_entities[256] = {0}; // Buffer to store processed entity names

    // Iterate through entities in the array
    int array_len = json_object_array_length(parsed_json);
    for(int i = 0; i < array_len; i++)
    {
        struct json_object* entity_obj = json_object_array_get_idx(parsed_json, i);
        struct json_object *entity_name_obj, *state_value_obj;

        if(!json_object_object_get_ex(entity_obj, "entity_name", &entity_name_obj) ||
           !json_object_object_get_ex(entity_obj, "state", &state_value_obj))
        {
            continue; // Skip if missing required fields
        }

        const char* entity_name = json_object_get_string(entity_name_obj);
        const char* state_str   = json_object_get_string(state_value_obj);

        // Map entity values to our struct
        if(strcmp(entity_name, "voltage") == 0)
        {
            temp_shunt.voltage = atof(state_str);
            field_count++;
            if(strlen(processed_entities) > 0)
                strcat(processed_entities, ", ");
            strcat(processed_entities, entity_name);
        }
        else if(strcmp(entity_name, "current") == 0)
        {
            temp_shunt.current = atof(state_str);
            field_count++;
            if(strlen(processed_entities) > 0)
                strcat(processed_entities, ", ");
            strcat(processed_entities, entity_name);
        }
        else if(strcmp(entity_name, "remaining_mins") == 0)
        {
            temp_shunt.remaining_mins = atoi(state_str);
            field_count++;
            if(strlen(processed_entities) > 0)
                strcat(processed_entities, ", ");
            strcat(processed_entities, entity_name);
        }
        else if(strcmp(entity_name, "soc") == 0)
        {
            temp_shunt.soc = atof(state_str);
            field_count++;
            if(strlen(processed_entities) > 0)
                strcat(processed_entities, ", ");
            strcat(processed_entities, entity_name);
        }
        else if(strcmp(entity_name, "consumed_ah") == 0)
        {
            temp_shunt.consumed_ah = atof(state_str);
            field_count++;
            if(strlen(processed_entities) > 0)
                strcat(processed_entities, ", ");
            strcat(processed_entities, entity_name);
        }
        else
        {
            log_warning("Unknown entity name in smart shunt data: %s", entity_name);
        }
    }

    // Check if we found fewer than 5 required fields and warn
    if(field_count < 5)
    {
        log_warning("Smart shunt data contains only %d fields (expected 5). Processed entities: %s",
                    field_count, processed_entities);
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
bool parse_climate_sensor(const char* json_str, climate_sensor_t* climate)
{
    if(!json_str || !climate)
    {
        return false;
    }

    struct json_object* parsed_json = json_tokener_parse(json_str);
    if(!parsed_json)
    {
        log_error("Failed to parse climate sensor JSON");
        return false;
    }

    if(!json_object_is_type(parsed_json, json_type_array))
    {
        log_error("Expected JSON array for climate sensor data");
        json_object_put(parsed_json);
        return false;
    }

    // Create a temporary climate data structure
    climate_sensor_t temp_climate = {0};

    // Counter for required fields
    int field_count = 0;

    // Iterate through entities in the array
    int array_len = json_object_array_length(parsed_json);
    for(int i = 0; i < array_len; i++)
    {
        struct json_object* entity_obj = json_object_array_get_idx(parsed_json, i);
        struct json_object *entity_name_obj, *state_value_obj;

        if(!json_object_object_get_ex(entity_obj, "entity_name", &entity_name_obj) ||
           !json_object_object_get_ex(entity_obj, "state", &state_value_obj))
        {
            continue; // Skip if missing required fields
        }

        const char* entity_name = json_object_get_string(entity_name_obj);
        const char* state_str   = json_object_get_string(state_value_obj);

        // Map entity_name to the appropriate field in the climate structure
        if(strcmp(entity_name, "battery") == 0)
        {
            temp_climate.battery = atof(state_str);
            field_count++;
        }
        else if(strcmp(entity_name, "temperature") == 0)
        {
            temp_climate.temperature = atof(state_str);
            field_count++;
        }
        else if(strcmp(entity_name, "humidity") == 0)
        {
            temp_climate.humidity = atof(state_str);
            field_count++;
        }
    }

    // Check if we found at least 3 required fields
    if(field_count < 3)
    {
        log_error("Climate sensor data must contain at least 3 fields");
        json_object_put(parsed_json);

        return false;
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
bool parse_camper_states(const char* json_str, camper_sensor_t* camper_data)
{
    if(!json_str || !camper_data)
    {
        return false;
    }

    struct json_object* parsed_json = json_tokener_parse(json_str);
    if(!parsed_json)
    {
        log_error("Failed to parse camper states JSON");
        return false;
    }

    if(!json_object_is_type(parsed_json, json_type_array))
    {
        log_error("Expected JSON array for camper states");
        json_object_put(parsed_json);
        return false;
    }

    // Create a temporary camper data structure
    camper_sensor_t temp_camper = {0};

    // Counter for required fields
    int field_count = 0;

    // Iterate through states in the array
    int array_len = json_object_array_length(parsed_json);
    for(int i = 0; i < array_len; i++)
    {
        struct json_object* state_obj = json_object_array_get_idx(parsed_json, i);
        struct json_object *entity_name_obj, *state_value_obj;

        if(!json_object_object_get_ex(state_obj, "entity_name", &entity_name_obj) ||
           !json_object_object_get_ex(state_obj, "state", &state_value_obj))
        {
            continue; // Skip if missing required fields
        }

        const char* entity_name = json_object_get_string(entity_name_obj);
        const char* state_str   = json_object_get_string(state_value_obj);

        // Map entity_name to the appropriate field in the camper structure
        if(strcmp(entity_name, "household_voltage") == 0)
        {
            temp_camper.household_voltage = atof(state_str) / 1000.0f; // millivolts
            field_count++;
        }
        else if(strcmp(entity_name, "starter_voltage") == 0)
        {
            temp_camper.starter_voltage = atof(state_str) / 1000.0f; // millivolts
            field_count++;
        }
        else if(strcmp(entity_name, "mains_voltage") == 0)
        {
            temp_camper.mains_voltage = atof(state_str) / 1000.0f; // millivolts
            field_count++;
        }
        else if(strcmp(entity_name, "household_state") == 0)
        {
            temp_camper.household_state =
                (strcmp(state_str, "ON") == 0 || strcmp(state_str, "PENDING") == 0);
            field_count++;
        }
        else if(strcmp(entity_name, "water_state") == 0)
        {
            temp_camper.water_state = atoi(state_str);
            field_count++;
        }
        else if(strcmp(entity_name, "waste_state") == 0)
        {
            temp_camper.waste_state = atoi(state_str);
            field_count++;
        }
        else if(strcmp(entity_name, "pump_state") == 0)
        {
            temp_camper.pump_state = (strcmp(state_str, "ON") == 0);
            field_count++;
        }
    }

    // Check if we found at least 7 required fields
    if(field_count < 7)
    {
        log_error("Camper states data must contain at least 7 fields found %d", field_count);
        json_object_put(parsed_json);
        return false;
    }

    // Copy the temp structure to the output parameter
    memcpy(camper_data, &temp_camper, sizeof(camper_sensor_t));

    // Clean up
    json_object_put(parsed_json);

    return true;
}

/**
 * Parse entity history JSON response
 * @param json_str The JSON string to parse
 * @param history Pointer to entity_history_t structure to fill
 * @return true if parsing succeeded, false otherwise
 */
bool parse_entity_history(const char* json_str, entity_history_t* history)
{
    if(!json_str || !history)
    {
        return false;
    }

    // Clear any existing data
    clear_entity_history(history);

    struct json_object* parsed_json = json_tokener_parse(json_str);
    if(!parsed_json)
    {
        log_error("Failed to parse history JSON");
        return false;
    }

    bool success = false;

    do
    {
        // Parse basic fields
        struct json_object* is_numeric_json;
        if(!json_object_object_get_ex(parsed_json, "is_numeric", &is_numeric_json))
        {
            log_error("Missing is_numeric field in history JSON");
            break;
        }
        history->is_numeric = json_object_get_boolean(is_numeric_json);

        struct json_object* entity_name_json;
        if(json_object_object_get_ex(parsed_json, "entity_name", &entity_name_json) &&
           json_object_is_type(entity_name_json, json_type_string))
        {
            const char* entity_name = json_object_get_string(entity_name_json);
            strncpy(history->entity_name, entity_name, sizeof(history->entity_name) - 1);
            history->entity_name[sizeof(history->entity_name) - 1] = '\0';
        }

        struct json_object* unit_json;
        if(json_object_object_get_ex(parsed_json, "unit", &unit_json) &&
           json_object_is_type(unit_json, json_type_string))
        {
            const char* unit = json_object_get_string(unit_json);
            strncpy(history->unit, unit, sizeof(history->unit) - 1);
            history->unit[sizeof(history->unit) - 1] = '\0';
        }
        else
        {
            history->unit[0] = '\0'; // No unit or null
        }

        // Get the data object
        struct json_object* data_json;
        if(!json_object_object_get_ex(parsed_json, "data", &data_json) ||
           !json_object_is_type(data_json, json_type_object))
        {
            log_error("Missing or invalid data object in history JSON");
            break;
        }

        // Parse timestamps array
        struct json_object* timestamps_json;
        if(!json_object_object_get_ex(data_json, "timestamps", &timestamps_json) ||
           !json_object_is_type(timestamps_json, json_type_array))
        {
            log_error("Missing or invalid timestamps array in history JSON");
            break;
        }

        int count = json_object_array_length(timestamps_json);
        if(count <= 0)
        {
            log_error("Empty timestamps array in history JSON");
            break;
        }

        history->count      = count;
        history->timestamps = (char**)malloc(count * sizeof(char*));
        if(!history->timestamps)
        {
            log_error("Failed to allocate memory for timestamps");
            break;
        }
        memset(history->timestamps, 0, count * sizeof(char*));

        // Allocate memory for data arrays
        history->min  = (float*)malloc(count * sizeof(float));
        history->max  = (float*)malloc(count * sizeof(float));
        history->mean = (float*)malloc(count * sizeof(float));

        if(!history->min || !history->max || !history->mean)
        {
            log_error("Failed to allocate memory for data arrays");
            break;
        }

        // Parse timestamps
        for(int i = 0; i < count; i++)
        {
            struct json_object* timestamp_item = json_object_array_get_idx(timestamps_json, i);
            if(json_object_is_type(timestamp_item, json_type_string))
            {
                const char* timestamp_str = json_object_get_string(timestamp_item);
                history->timestamps[i]    = strdup(timestamp_str);
                if(!history->timestamps[i])
                {
                    log_error("Failed to duplicate timestamp string");
                    break;
                }
            }
            else
            {
                log_error("Invalid timestamp at index %d", i);
                break;
            }
        }

        // Parse min values
        struct json_object* min_json;
        if(json_object_object_get_ex(data_json, "min", &min_json) &&
           json_object_is_type(min_json, json_type_array) &&
           json_object_array_length(min_json) == count)
        {
            for(int i = 0; i < count; i++)
            {
                struct json_object* item = json_object_array_get_idx(min_json, i);
                history->min[i]          = get_json_number(item);
            }
        }
        else
        {
            log_error("Invalid min array in history JSON");
            break;
        }

        // Parse max values
        struct json_object* max_json;
        if(json_object_object_get_ex(data_json, "max", &max_json) &&
           json_object_is_type(max_json, json_type_array) &&
           json_object_array_length(max_json) == count)
        {
            for(int i = 0; i < count; i++)
            {
                struct json_object* item = json_object_array_get_idx(max_json, i);
                history->max[i]          = get_json_number(item);
            }
        }
        else
        {
            log_error("Invalid max array in history JSON");
            break;
        }

        // Parse mean values
        struct json_object* mean_json;
        if(json_object_object_get_ex(data_json, "mean", &mean_json) &&
           json_object_is_type(mean_json, json_type_array) &&
           json_object_array_length(mean_json) == count)
        {
            for(int i = 0; i < count; i++)
            {
                struct json_object* item = json_object_array_get_idx(mean_json, i);
                history->mean[i]         = get_json_number(item);
            }
        }
        else
        {
            log_error("Invalid mean array in history JSON");
            break;
        }

        success = true;
    } while(0);

    json_object_put(parsed_json);

    if(!success)
    {
        clear_entity_history(history);
    }

    return success;
}