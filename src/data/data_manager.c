/*******************************************************************
 *
 * data_manager.c - Central data manager implementation
 *
 ******************************************************************/
#include <stdbool.h>
#include <string.h>
#include <stdio.h>    // Add this for snprintf
#include <pthread.h>
#include <unistd.h>
#include "data_manager.h"
#include "sensor_parsers.h"
#include "data_actions.h"
#include "../lib/http_client.h"
#include "../lib/logger.h"
#include "../lib/mem_debug.h"
#include "../main.h"

// Global variables to hold the current sensor data
static smart_solar_t smart_solar = {0};
static smart_shunt_t smart_shunt = {0};
static climate_sensor_t inside_climate = {0};
static climate_sensor_t outside_climate = {0};
static camper_sensor_t camper = {0};

// Mutex for thread safety
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

// Background worker thread
static pthread_t worker_thread = 0;
static volatile bool fetch_requested = false;
static volatile bool worker_running = false;

// Action queue
#define MAX_ACTION_QUEUE 10
typedef struct {
    int entity_id;
    char status[8]; // ON/OFF or other short status strings
} camper_action_t;

static camper_action_t action_queue[MAX_ACTION_QUEUE];
static volatile int action_queue_head = 0;
static volatile int action_queue_tail = 0;
static pthread_mutex_t action_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations for internal functions
static void* background_worker_thread(void* arg);
static bool is_action_queue_empty(void);
static bool is_action_queue_full(void);
static bool enqueue_action(const camper_action_t *action);
static bool dequeue_action(camper_action_t *action);
static int fetch_camper_data_internal(void);
static int fetch_system_data_internal(void);

/**
 * Initialize the background worker system
 */
int init_background_fetcher(void) {
    if (worker_running) {
        log_warning("Background worker already running");
        return 0;
    }
    
    worker_running = true;
    fetch_requested = false;
    action_queue_head = action_queue_tail = 0;
    
    if (pthread_create(&worker_thread, NULL, background_worker_thread, NULL) != 0) {
        log_error("Failed to create background worker thread");
        worker_running = false;
        return -1;
    }
    
    log_info("Background worker initialized");
    return 0;
}

/**
 * Shutdown the background worker
 */
void shutdown_background_fetcher(void) {
    if (!worker_running) {
        return;
    }
    
    log_info("Shutting down background worker");
    worker_running = false;
    
    // Wait for thread to exit
    if (worker_thread) {
        pthread_join(worker_thread, NULL);
        worker_thread = 0;
    }
}

/**
 * Check if any background operation is in progress
 */
bool is_background_busy(void) {
    return fetch_requested || !is_action_queue_empty();
}

/**
 * Request a new fetch operation
 */
void request_camper_data_fetch(void) {
    if (!worker_running) {
        log_error("Background worker not running, initialize it first");
        return;
    }
    
    fetch_requested = true;
}

/**
 * Request a camper action in the background
 */
void request_camper_action(const int entity_id, const char *status) {
    if (!worker_running) {
        log_error("Background worker not running, initialize it first");
        return;
    }
    
    camper_action_t action;
    action.entity_id = entity_id;
    strncpy(action.status, status, sizeof(action.status) - 1);
    action.status[sizeof(action.status) - 1] = '\0';  // Ensure null termination
    
    if (!enqueue_action(&action)) {
        log_error("Failed to queue action - queue full");
    }
}

/**
 * Check if action queue is empty
 */
static bool is_action_queue_empty(void) {
    return action_queue_head == action_queue_tail;
}

/**
 * Check if action queue is full
 */
static bool is_action_queue_full(void) {
    return ((action_queue_tail + 1) % MAX_ACTION_QUEUE) == action_queue_head;
}

/**
 * Add an action to the queue
 */
static bool enqueue_action(const camper_action_t *action) {
    bool result = false;
    
    pthread_mutex_lock(&action_mutex);
    
    if (!is_action_queue_full()) {
        action_queue[action_queue_tail] = *action;
        action_queue_tail = (action_queue_tail + 1) % MAX_ACTION_QUEUE;
        result = true;
    }
    
    pthread_mutex_unlock(&action_mutex);
    
    return result;
}

/**
 * Remove an action from the queue
 */
static bool dequeue_action(camper_action_t *action) {
    bool result = false;
    
    pthread_mutex_lock(&action_mutex);
    
    if (!is_action_queue_empty()) {
        *action = action_queue[action_queue_head];
        action_queue_head = (action_queue_head + 1) % MAX_ACTION_QUEUE;
        result = true;
    }
    
    pthread_mutex_unlock(&action_mutex);
    
    return result;
}

/**
 * Background thread function that handles work requests
 */
static void* background_worker_thread(void* arg) {
    log_info("Background worker thread started");
    
    camper_action_t action;
    
    while (worker_running) {
        bool did_work = false;
        
        // Handle fetch request
        if (fetch_requested) {
            fetch_requested = false;
            fetch_camper_data_internal();
            did_work = true;
        }
        
        // Handle one action from the queue
        if (dequeue_action(&action)) {
            set_camper_action_internal(action.entity_id, action.status);
            did_work = true;
        }
        
        // If no work was done this iteration, sleep
        if (!did_work) {
            usleep(BACKGROUND_FETCH_SLEEP_US);
        }
    }
    
    log_info("Background worker thread exiting");
    return NULL;
}

/**
 * Internal function to fetch camper data from the server and update local state
 */
static int fetch_camper_data_internal(void) {
    char api_url[MAX_URL_LENGTH];
    snprintf(api_url, sizeof(api_url), "%s/sensors/5/states/", API_BASE_URL);
    
    http_response_t response = http_get(api_url, HTTP_TIMEOUT_SECONDS);
    
    if (!response.success) {
        log_error("Failed to fetch camper data: %s", response.error);
        if (response.body && *response.body) {
            log_error("Response body: %s", response.body);
        }
        http_response_free(&response);
        return -1;
    }
    
    // Parse the response
    camper_sensor_t temp_camper = {0};
    if (parse_camper_states(response.body, &temp_camper)) {
        // Update the actual data structure in a thread-safe manner
        pthread_mutex_lock(&data_mutex);
        memcpy(&camper, &temp_camper, sizeof(camper_sensor_t));
        pthread_mutex_unlock(&data_mutex);
        
        // Debug output
        log_debug("Camper data updated: household_v=%.2f, starter_v=%.2f, mains_v=%.2f", 
                 camper.household_voltage, camper.starter_voltage, camper.mains_voltage);
        log_debug("States: household=%s, pump=%s, water=%d%%, waste=%d%%",
                 camper.household_state ? "ON" : "OFF",
                 camper.pump_state ? "ON" : "OFF",
                 camper.water_state, camper.waste_state);
    }
    
    http_response_free(&response);
    
    return 0;
}

/**
 * Fetch system data from the server
 */
static int fetch_system_data_internal(void) {
    char api_url[MAX_URL_LENGTH];
    snprintf(api_url, sizeof(api_url), "%s/sensors/5/states/", API_BASE_URL);

    http_response_t response = http_get(api_url, HTTP_TIMEOUT_SECONDS);
    
    if (!response.success) {
        log_error("Failed to fetch sensor data: %s", response.error);
        if (response.body && *response.body) {
            log_error("Response body: %s", response.body);
        }
        http_response_free(&response);
        return -1;
    }
    
    // Parse JSON response (array of sensors)
    struct json_object *parsed_json = json_tokener_parse(response.body);
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
    
    smart_solar_t temp_smart_solar = {0};
    smart_shunt_t temp_smart_shunt = {0};
    climate_sensor_t temp_inside_climate = {0};
    climate_sensor_t temp_outside_climate = {0};
    
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
            parse_smart_solar(sensor_obj, &temp_smart_solar);
        } 
        else if (strcmp(sensor_name, "SmartShunt") == 0) {
            parse_smart_shunt(sensor_obj, &temp_smart_shunt);
        }
        else if (strcmp(sensor_name, "inside") == 0) {
            parse_climate_sensor(sensor_obj, &temp_inside_climate);
        }
        else if (strcmp(sensor_name, "outside") == 0) {
            parse_climate_sensor(sensor_obj, &temp_outside_climate);
        }
    }
    
    // Update all sensors atomically
    pthread_mutex_lock(&data_mutex);
    memcpy(&smart_solar, &temp_smart_solar, sizeof(smart_solar_t));
    memcpy(&smart_shunt, &temp_smart_shunt, sizeof(smart_shunt_t));
    memcpy(&inside_climate, &temp_inside_climate, sizeof(climate_sensor_t));
    memcpy(&outside_climate, &temp_outside_climate, sizeof(climate_sensor_t));
    pthread_mutex_unlock(&data_mutex);
    
    // Clean up
    json_object_put(parsed_json);
    http_response_free(&response);
    return 0;
}

// Thread-safe getter functions to access the sensor data
smart_solar_t* get_smart_solar_data(void) {
    static smart_solar_t safe_copy;
    
    pthread_mutex_lock(&data_mutex);
    memcpy(&safe_copy, &smart_solar, sizeof(smart_solar_t));
    pthread_mutex_unlock(&data_mutex);
    
    return &safe_copy;
}

smart_shunt_t* get_smart_shunt_data(void) {
    static smart_shunt_t safe_copy;
    
    pthread_mutex_lock(&data_mutex);
    memcpy(&safe_copy, &smart_shunt, sizeof(smart_shunt_t));
    pthread_mutex_unlock(&data_mutex);
    
    return &safe_copy;
}

climate_sensor_t* get_inside_climate_data(void) {
    static climate_sensor_t safe_copy;
    
    pthread_mutex_lock(&data_mutex);
    memcpy(&safe_copy, &inside_climate, sizeof(climate_sensor_t));
    pthread_mutex_unlock(&data_mutex);
    
    return &safe_copy;
}

climate_sensor_t* get_outside_climate_data(void) {
    static climate_sensor_t safe_copy;
    
    pthread_mutex_lock(&data_mutex);
    memcpy(&safe_copy, &outside_climate, sizeof(climate_sensor_t));
    pthread_mutex_unlock(&data_mutex);
    
    return &safe_copy;
}

camper_sensor_t* get_camper_data(void) {
    static camper_sensor_t safe_copy;
    
    pthread_mutex_lock(&data_mutex);
    memcpy(&safe_copy, &camper, sizeof(camper_sensor_t));
    pthread_mutex_unlock(&data_mutex);
    
    return &safe_copy;
}
