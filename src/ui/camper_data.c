#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "../lib/http_client.h"
#include "../lib/logger.h"
#include "camper_data.h"

// Data structures for each sensor type
typedef struct {
    float battery_charging_current;
    float battery_voltage;
    int charge_state;
    float solar_power;
    float yield_today;
} smart_solar_t;

typedef struct {
    float voltage;
    float current;
    int remaining_mins;
    float soc;  // State of Charge (%)
    float consumed_ah;
} smart_shunt_t;

typedef struct {
    float battery;
    float temperature;
    float humidity;
} climate_sensor_t;

typedef struct {
    float household_voltage;
    float starter_voltage;
    float mains_voltage;
    bool household_state;
    bool water_state;
    bool waste_state;
    bool pump_state;
} camper_sensor_t;

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
 * Fetch system data from the server and update local state
 * @return 0 on success, non-zero on failure
 */
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
        else if (strcmp(sensor_name, "camper") == 0) {
            parse_camper_sensor(sensor_obj);
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

/**
 * Parse camper sensor data
 */
static bool parse_camper_sensor(struct json_object *sensor_obj) {
    struct json_object *entities_obj, *entity_obj, *name_obj, *value_obj;
    
    if (!json_object_object_get_ex(sensor_obj, "entities", &entities_obj) || 
        !json_object_is_type(entities_obj, json_type_array)) {
        return false;
    }
    
    int entities_len = json_object_array_length(entities_obj);
    
    // Loop through all entities in the camper sensor
    for (int i = 0; i < entities_len; i++) {
        entity_obj = json_object_array_get_idx(entities_obj, i);
        
        if (!json_object_object_get_ex(entity_obj, "name", &name_obj) ||
            !json_object_object_get_ex(entity_obj, "value", &value_obj)) {
            continue;
        }
        
        const char *entity_name = json_object_get_string(name_obj);
        
        // Map entity values to our struct
        if (strcmp(entity_name, "household_voltage") == 0) {
            camper.household_voltage = json_object_get_double(value_obj);
        } 
        else if (strcmp(entity_name, "starter_voltage") == 0) {
            camper.starter_voltage = json_object_get_double(value_obj);
        }
        else if (strcmp(entity_name, "mains_voltage") == 0) {
            camper.mains_voltage = json_object_get_double(value_obj);
        }
        else if (strcmp(entity_name, "household_state") == 0) {
            camper.household_state = json_object_get_boolean(value_obj);
        }
        else if (strcmp(entity_name, "water_state") == 0) {
            camper.water_state = json_object_get_boolean(value_obj);
        }
        else if (strcmp(entity_name, "waste_state") == 0) {
            camper.waste_state = json_object_get_boolean(value_obj);
        }
        else if (strcmp(entity_name, "pump_state") == 0) {
            camper.pump_state = json_object_get_boolean(value_obj);
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

camper_sensor_t* get_camper_data(void) {
    return &camper;
}