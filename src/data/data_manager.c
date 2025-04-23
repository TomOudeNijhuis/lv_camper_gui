/*******************************************************************
 *
 * data_manager.c - Central data manager implementation
 *
 ******************************************************************/
#include <stdbool.h>
#include <string.h>
#include <stdio.h> // Add this for snprintf
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
static smart_solar_t    smart_solar     = {0};
static smart_shunt_t    smart_shunt     = {0};
static climate_sensor_t inside_climate  = {0};
static climate_sensor_t outside_climate = {0};
static camper_sensor_t  camper          = {0};

// Global variable to hold history data
static entity_history_t  entity_history          = {0};
static history_request_t current_history_request = {0};

// Mutex for thread safety
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

// Background worker thread
static pthread_t     worker_thread   = 0;
static volatile bool fetch_requested = false;
static volatile bool worker_running  = false;

#define MAX_FETCH_QUEUE 20
typedef struct
{
    fetch_request_type_t request_type;
    uint32_t             timestamp; // For prioritization if needed
} fetch_request_t;

static fetch_request_t fetch_queue[MAX_FETCH_QUEUE];
static volatile int    fetch_queue_head = 0;
static volatile int    fetch_queue_tail = 0;
static pthread_mutex_t fetch_mutex      = PTHREAD_MUTEX_INITIALIZER;

static bool is_fetch_queue_empty(void);
static bool is_fetch_queue_full(void);
static bool enqueue_fetch_request(fetch_request_type_t request_type);
static bool dequeue_fetch_request(fetch_request_t* action);

// Action queue
#define MAX_ACTION_QUEUE 10
typedef struct
{
    char entity_name[16];
    char status[8]; // ON/OFF or other short status strings
} camper_action_t;

static camper_action_t action_queue[MAX_ACTION_QUEUE];
static volatile int    action_queue_head = 0;
static volatile int    action_queue_tail = 0;
static pthread_mutex_t action_mutex      = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations for internal functions
static void* background_worker_thread(void* arg);
static bool  is_action_queue_empty(void);
static bool  is_action_queue_full(void);
static bool  enqueue_action(const camper_action_t* action);
static bool  dequeue_action(camper_action_t* action);

static int fetch_data_internal(fetch_request_type_t request_type);
static int fetch_camper_data_internal(void);
static int fetch_climate_data_internal(const char* location);
static int fetch_smart_solar_data_internal(void);
static int fetch_smart_shunt_data_internal(void);
static int fetch_entity_history_data_internal(void);

/**
 * Initialize the background worker system
 */
int init_background_fetcher(void)
{
    if(worker_running)
    {
        log_warning("Background worker already running");
        return 0;
    }

    smart_solar.valid     = false;
    smart_shunt.valid     = false;
    camper.valid          = false;
    inside_climate.valid  = false;
    outside_climate.valid = false;

    // Initialize entity_history completely to prevent segfaults
    memset(&entity_history, 0, sizeof(entity_history_t));
    entity_history.valid = false;

    worker_running    = true;
    fetch_requested   = false;
    action_queue_head = action_queue_tail = 0;

    if(pthread_create(&worker_thread, NULL, background_worker_thread, NULL) != 0)
    {
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
void shutdown_background_fetcher(void)
{
    if(!worker_running)
    {
        return;
    }

    log_info("Shutting down background worker");
    worker_running = false;

    // Wait for thread to exit
    if(worker_thread)
    {
        pthread_join(worker_thread, NULL);
        worker_thread = 0;
    }
}

/**
 * Check if any background operation is in progress
 */
bool is_background_busy(void)
{
    return !is_fetch_queue_empty() || !is_action_queue_empty();
}

/*
 * Request a camper action in the background
 */
void request_camper_action(const char* entity_name, const char* status)
{
    if(!worker_running)
    {
        log_error("Background worker not running, initialize it first");
        return;
    }

    camper_action_t action;
    strncpy(action.entity_name, entity_name, sizeof(action.entity_name) - 1);
    action.entity_name[sizeof(action.entity_name) - 1] = '\0'; // Ensure null termination
    strncpy(action.status, status, sizeof(action.status) - 1);
    action.status[sizeof(action.status) - 1] = '\0'; // Ensure null termination

    if(!enqueue_action(&action))
    {
        log_error("Failed to queue action - queue full");
    }
}

/**
 * Request a specific data fetch operation
 * @param request_type Type of data to fetch
 * @return true if request was queued successfully, false otherwise
 */
bool request_data_fetch(fetch_request_type_t request_type)
{
    if(!worker_running)
    {
        log_error("Background worker not running, initialize it first");
        return false;
    }

    if(request_type >= FETCH_TYPE_COUNT)
    {
        log_error("Invalid fetch request type: %d", request_type);
        return false;
    }

    bool queued = enqueue_fetch_request(request_type);
    if(!queued)
    {
        log_warning("Failed to queue fetch request: queue full or duplicate");
    }

    return queued;
}

/**
 * Request historical data for a specific entity
 * @param sensor_name Name of the sensor (e.g., "inside", "SmartSolar")
 * @param entity_name Name of the entity (e.g., "temperature")
 * @param interval Time interval for data points (e.g., "hour", "day")
 * @param samples Number of data points to retrieve
 * @return true if request was queued successfully, false otherwise
 */
bool request_entity_history(const char* sensor_name, const char* entity_name, const char* interval,
                            int samples)
{
    if(!worker_running)
    {
        log_error("Background worker not running, initialize it first");
        return false;
    }

    if(!sensor_name || !entity_name || !interval || samples <= 0)
    {
        log_error("Invalid parameters for history request");
        return false;
    }

    // Store request parameters for later use
    pthread_mutex_lock(&data_mutex);
    snprintf(current_history_request.sensor_name, sizeof(current_history_request.sensor_name), "%s",
             sensor_name);
    snprintf(current_history_request.entity_name, sizeof(current_history_request.entity_name), "%s",
             entity_name);
    snprintf(current_history_request.interval, sizeof(current_history_request.interval), "%s",
             interval);
    current_history_request.samples = samples;
    pthread_mutex_unlock(&data_mutex);

    // Queue the request
    return request_data_fetch(FETCH_ENTITY_HISTORY);
}

/**
 * Check if action queue is empty
 */
static bool is_action_queue_empty(void)
{
    return action_queue_head == action_queue_tail;
}

/**
 * Check if action queue is full
 */
static bool is_action_queue_full(void)
{
    return ((action_queue_tail + 1) % MAX_ACTION_QUEUE) == action_queue_head;
}

/**
 * Add an action to the queue
 */
static bool enqueue_action(const camper_action_t* action)
{
    bool result = false;

    pthread_mutex_lock(&action_mutex);

    if(!is_action_queue_full())
    {
        action_queue[action_queue_tail] = *action;
        action_queue_tail               = (action_queue_tail + 1) % MAX_ACTION_QUEUE;
        result                          = true;
    }

    pthread_mutex_unlock(&action_mutex);

    return result;
}

/**
 * Remove an action from the queue
 */
static bool dequeue_action(camper_action_t* action)
{
    bool result = false;

    pthread_mutex_lock(&action_mutex);

    if(!is_action_queue_empty())
    {
        *action           = action_queue[action_queue_head];
        action_queue_head = (action_queue_head + 1) % MAX_ACTION_QUEUE;
        result            = true;
    }

    pthread_mutex_unlock(&action_mutex);

    return result;
}

static bool is_fetch_queue_empty(void)
{
    return fetch_queue_head == fetch_queue_tail;
}

static bool is_fetch_queue_full(void)
{
    return ((fetch_queue_tail + 1) % MAX_FETCH_QUEUE) == fetch_queue_head;
}

static bool enqueue_fetch_request(fetch_request_type_t request_type)
{
    bool result = false;

    pthread_mutex_lock(&fetch_mutex);

    // Check if same request is already in queue (de-duplication)
    bool duplicate = false;
    int  i         = fetch_queue_head;
    while(i != fetch_queue_tail)
    {
        if(fetch_queue[i].request_type == request_type)
        {
            duplicate = true;
            break;
        }
        i = (i + 1) % MAX_FETCH_QUEUE;
    }

    if(!duplicate && !is_fetch_queue_full())
    {
        fetch_queue[fetch_queue_tail].request_type = request_type;
        fetch_queue[fetch_queue_tail].timestamp    = time(NULL);
        fetch_queue_tail                           = (fetch_queue_tail + 1) % MAX_FETCH_QUEUE;
        result                                     = true;
    }

    pthread_mutex_unlock(&fetch_mutex);

    return result;
}

static bool dequeue_fetch_request(fetch_request_t* request)
{
    bool   result       = false;
    time_t current_time = time(NULL);

    pthread_mutex_lock(&fetch_mutex);

    while(!is_fetch_queue_empty())
    {
        *request = fetch_queue[fetch_queue_head];

        // Check if the request is stale
        if(current_time - request->timestamp > REQUEST_TIMEOUT_SECONDS)
        {
            // Log that we're skipping a stale request
            log_warning("Skipping stale request type %d (age: %ld seconds)", request->request_type,
                        current_time - request->timestamp);

            // Move to the next request
            fetch_queue_head = (fetch_queue_head + 1) % MAX_FETCH_QUEUE;
            continue;
        }

        fetch_queue_head = (fetch_queue_head + 1) % MAX_FETCH_QUEUE;
        result           = true;
        break;
    }

    pthread_mutex_unlock(&fetch_mutex);

    return result;
}

/**
 * Background thread function that handles work requests
 */
static void* background_worker_thread(void* arg)
{
    log_info("Background worker thread started");

    camper_action_t action;
    fetch_request_t fetch_request;

    while(worker_running)
    {
        bool did_work = false;

        // Handle one fetch request from the queue
        if(dequeue_fetch_request(&fetch_request))
        {
            fetch_data_internal(fetch_request.request_type);
            did_work = true;
        }

        // Handle one action from the queue
        if(dequeue_action(&action))
        {
            set_camper_action_internal(action.entity_name, action.status);
            did_work = true;
        }

        // If no work was done this iteration, sleep
        if(!did_work)
        {
            usleep(BACKGROUND_FETCH_SLEEP_US);
        }
    }

    log_info("Background worker thread exiting");
    return NULL;
}

/**
 * Internal function to fetch specific sensor data based on request type
 */
static int fetch_data_internal(fetch_request_type_t request_type)
{
    switch(request_type)
    {
        case FETCH_CAMPER_DATA: return fetch_camper_data_internal();
        case FETCH_CLIMATE_INSIDE: return fetch_climate_data_internal("inside");
        case FETCH_CLIMATE_OUTSIDE: return fetch_climate_data_internal("outside");
        case FETCH_SMART_SOLAR: return fetch_smart_solar_data_internal();
        case FETCH_SMART_SHUNT: return fetch_smart_shunt_data_internal();
        case FETCH_ENTITY_HISTORY: return fetch_entity_history_data_internal();
        default: log_warning("Unimplemented fetch request type: %d", request_type); return -1;
    }
}

/**
 * Internal function to fetch camper data from the server and update local state
 */
void clear_entity_history(entity_history_t* history)
{
    if(!history)
        return;

    if(history->timestamps)
    {
        for(int i = 0; i < history->count; i++)
        {
            if(history->timestamps[i])
            {
                free(history->timestamps[i]);
                history->timestamps[i] = NULL;
            }
        }
        free(history->timestamps);
        history->timestamps = NULL;
    }

    if(history->min)
    {
        free(history->min);
        history->min = NULL;
    }

    if(history->max)
    {
        free(history->max);
        history->max = NULL;
    }

    if(history->mean)
    {
        free(history->mean);
        history->mean = NULL;
    }

    history->count = 0;
}

/**
 * Internal function to fetch camper data from the server and update local state
 */
static int fetch_camper_data_internal(void)
{
    char api_url[MAX_URL_LENGTH];
    snprintf(api_url, sizeof(api_url), "%s/sensors/camper/states/", API_BASE_URL);

    http_response_t response = http_get(api_url, HTTP_TIMEOUT_SECONDS);

    if(!response.success)
    {
        log_error("Failed to fetch camper data: %s", response.error);
        if(response.body && *response.body)
        {
            log_error("Response body: %s", response.body);
        }
        http_response_free(&response);

        // Mark data as invalid
        pthread_mutex_lock(&data_mutex);
        camper.valid = false;
        pthread_mutex_unlock(&data_mutex);

        return -1;
    }

    // Parse the response
    camper_sensor_t temp_camper = {0};
    if(parse_camper_states(response.body, &temp_camper))
    {
        temp_camper.valid = true;

        // Update the actual data structure in a thread-safe manner
        pthread_mutex_lock(&data_mutex);
        memcpy(&camper, &temp_camper, sizeof(camper_sensor_t));
        pthread_mutex_unlock(&data_mutex);

        // Debug output
        log_debug("Camper data updated: household_v=%.2f, starter_v=%.2f, mains_v=%.2f",
                  camper.household_voltage, camper.starter_voltage, camper.mains_voltage);
        log_debug("States: household=%s, pump=%s, water=%d%%, waste=%d%%",
                  camper.household_state ? "ON" : "OFF", camper.pump_state ? "ON" : "OFF",
                  camper.water_state, camper.waste_state);
    }
    else
    {
        // Mark data as invalid if parsing failed
        pthread_mutex_lock(&data_mutex);
        camper.valid = false;
        pthread_mutex_unlock(&data_mutex);
    }

    http_response_free(&response);

    return 0;
}

/**
 * Internal function to fetch climate data from the server and update local state
 * @param location "inside" or "outside" to specify which climate sensor to fetch
 */
static int fetch_climate_data_internal(const char* location)
{
    char api_url[MAX_URL_LENGTH];
    snprintf(api_url, sizeof(api_url), "%s/sensors/%s/states/", API_BASE_URL, location);

    http_response_t response = http_get(api_url, HTTP_TIMEOUT_SECONDS);

    if(!response.success)
    {
        log_error("Failed to fetch %s climate data: %s", location, response.error);
        if(response.body && *response.body)
        {
            log_error("Response body: %s", response.body);
        }
        http_response_free(&response);

        // Mark data as invalid
        pthread_mutex_lock(&data_mutex);
        if(strcmp(location, "inside") == 0)
        {
            inside_climate.valid = false;
        }
        else if(strcmp(location, "outside") == 0)
        {
            outside_climate.valid = false;
        }
        pthread_mutex_unlock(&data_mutex);

        return -1;
    }

    // Parse the response
    climate_sensor_t temp_climate = {0};
    if(parse_climate_sensor(response.body, &temp_climate))
    {
        // Set valid flag
        temp_climate.valid = true;

        // Update the appropriate data structure in a thread-safe manner
        pthread_mutex_lock(&data_mutex);
        if(strcmp(location, "inside") == 0)
        {
            memcpy(&inside_climate, &temp_climate, sizeof(climate_sensor_t));
            log_debug("Inside climate data updated: temperature=%.2f, humidity=%.2f, battery=%.2f",
                      inside_climate.temperature, inside_climate.humidity, inside_climate.battery);
        }
        else if(strcmp(location, "outside") == 0)
        {
            memcpy(&outside_climate, &temp_climate, sizeof(climate_sensor_t));
            log_debug("Outside climate data updated: temperature=%.2f, humidity=%.2f, battery=%.2f",
                      outside_climate.temperature, outside_climate.humidity,
                      outside_climate.battery);
        }
        pthread_mutex_unlock(&data_mutex);
    }
    else
    {
        // Mark data as invalid if parsing failed
        pthread_mutex_lock(&data_mutex);
        if(strcmp(location, "inside") == 0)
        {
            inside_climate.valid = false;
        }
        else if(strcmp(location, "outside") == 0)
        {
            outside_climate.valid = false;
        }
        pthread_mutex_unlock(&data_mutex);
    }

    http_response_free(&response);
    return 0;
}

/**
 * Internal function to fetch SmartSolar data from the server and update local state
 */
static int fetch_smart_solar_data_internal(void)
{
    char api_url[MAX_URL_LENGTH];
    snprintf(api_url, sizeof(api_url), "%s/sensors/SmartSolar/states/", API_BASE_URL);

    http_response_t response = http_get(api_url, HTTP_TIMEOUT_SECONDS);

    if(!response.success)
    {
        log_error("Failed to fetch SmartSolar data: %s", response.error);
        if(response.body && *response.body)
        {
            log_error("Response body: %s", response.body);
        }
        http_response_free(&response);

        // Mark data as invalid
        pthread_mutex_lock(&data_mutex);
        smart_solar.valid = false;
        pthread_mutex_unlock(&data_mutex);

        return -1;
    }

    // Parse the response
    smart_solar_t temp_solar = {0};
    if(parse_smart_solar(response.body, &temp_solar))
    {
        // Set valid flag
        temp_solar.valid = true;

        // Update the actual data structure in a thread-safe manner
        pthread_mutex_lock(&data_mutex);
        memcpy(&smart_solar, &temp_solar, sizeof(smart_solar_t));
        pthread_mutex_unlock(&data_mutex);

        // Debug output
        log_debug("SmartSolar data updated: battery_v=%.2f, charging_current=%.2f, power=%.2f W, "
                  "yield=%.2f kWh",
                  smart_solar.battery_voltage, smart_solar.battery_charging_current,
                  smart_solar.solar_power, smart_solar.yield_today);
    }
    else
    {
        // Mark data as invalid if parsing failed
        pthread_mutex_lock(&data_mutex);
        smart_solar.valid = false;
        pthread_mutex_unlock(&data_mutex);
    }

    http_response_free(&response);

    return 0;
}

/**
 * Internal function to fetch SmartShunt data from the server and update local state
 */
static int fetch_smart_shunt_data_internal(void)
{
    char api_url[MAX_URL_LENGTH];
    snprintf(api_url, sizeof(api_url), "%s/sensors/SmartShunt/states/", API_BASE_URL);

    http_response_t response = http_get(api_url, HTTP_TIMEOUT_SECONDS);

    if(!response.success)
    {
        log_error("Failed to fetch SmartShunt data: %s", response.error);
        if(response.body && *response.body)
        {
            log_error("Response body: %s", response.body);
        }
        http_response_free(&response);

        // Mark data as invalid
        pthread_mutex_lock(&data_mutex);
        smart_shunt.valid = false;
        pthread_mutex_unlock(&data_mutex);

        return -1;
    }

    // Parse the response
    smart_shunt_t temp_shunt = {0};
    if(parse_smart_shunt(response.body, &temp_shunt))
    {
        // Set valid flag
        temp_shunt.valid = true;

        // Update the actual data structure in a thread-safe manner
        pthread_mutex_lock(&data_mutex);
        memcpy(&smart_shunt, &temp_shunt, sizeof(smart_shunt_t));
        pthread_mutex_unlock(&data_mutex);

        // Debug output
        log_debug(
            "SmartShunt data updated: voltage=%.2f, current=%.2f, SoC=%.1f%%, remaining=%d mins",
            temp_shunt.voltage, temp_shunt.current, temp_shunt.soc, temp_shunt.remaining_mins);
    }
    else
    {
        // Mark data as invalid if parsing failed
        pthread_mutex_lock(&data_mutex);
        smart_shunt.valid = false;
        pthread_mutex_unlock(&data_mutex);
    }

    http_response_free(&response);

    return 0;
}

/**
 * Internal function to fetch entity history data from the server
 */
static int fetch_entity_history_data_internal(void)
{
    char              api_url[MAX_URL_LENGTH];
    history_request_t request;

    // Get a copy of the request parameters
    pthread_mutex_lock(&data_mutex);
    memcpy(&request, &current_history_request, sizeof(history_request_t));
    pthread_mutex_unlock(&data_mutex);

    // Construct the API URL with the request parameters
    snprintf(api_url, sizeof(api_url), "%s/grouped_states_by_name/%s/%s?period=%s&samples=%d",
             API_BASE_URL, request.sensor_name, request.entity_name, request.interval,
             request.samples);

    log_debug("Fetching entity history: %s", api_url);
    http_response_t response = http_get(api_url, HTTP_TIMEOUT_SECONDS);

    if(!response.success)
    {
        log_error("Failed to fetch entity history data: %s", response.error);
        if(response.body && *response.body)
        {
            log_error("Response body: %s", response.body);
        }
        http_response_free(&response);

        // Mark data as invalid
        pthread_mutex_lock(&data_mutex);
        entity_history.valid = false;
        pthread_mutex_unlock(&data_mutex);

        return -1;
    }

    // Parse the response
    entity_history_t temp_history = {0}; // Ensure zero-initialization

    // Pre-set the sensor name in the temp structure before parsing
    strncpy(temp_history.sensor_name, request.sensor_name, sizeof(temp_history.sensor_name) - 1);
    temp_history.sensor_name[sizeof(temp_history.sensor_name) - 1] = '\0';

    if(parse_entity_history(response.body, &temp_history))
    {
        // Set valid flag
        temp_history.valid = true;

        // Update the actual data structure in a thread-safe manner
        pthread_mutex_lock(&data_mutex);

        // Clear old data
        clear_entity_history(&entity_history);

        // Copy new data including sensor_name
        memcpy(&entity_history, &temp_history, sizeof(entity_history_t));

        pthread_mutex_unlock(&data_mutex);

        log_debug("Entity history updated: %s.%s, %d data points", request.sensor_name,
                  request.entity_name, temp_history.count);
    }
    else
    {
        log_error("Failed to parse entity history data");
        clear_entity_history(&temp_history);

        // Mark data as invalid
        pthread_mutex_lock(&data_mutex);
        entity_history.valid = false;
        pthread_mutex_unlock(&data_mutex);

        http_response_free(&response);
        return -1;
    }

    http_response_free(&response);
    return 0;
}

/**
 * Updates a single camper entity value based on its name
 *
 * @param entity_name Name of the entity to update
 * @param state_str String representation of the state value
 * @return true if the entity was updated, false if not found or invalid
 */
bool update_camper_entity(const char* entity_name, const char* state_str)
{
    if(!entity_name || !state_str)
    {
        log_error("Invalid parameters in update_camper_entity");
        return false;
    }

    bool updated = true;

    pthread_mutex_lock(&data_mutex);

    if(strcmp(entity_name, "household_state") == 0)
    {
        camper.household_state = (strcmp(state_str, "ON") == 0);
    }
    else if(strcmp(entity_name, "pump_state") == 0)
    {
        camper.pump_state = (strcmp(state_str, "ON") == 0);
    }
    else
    {
        log_warning("Unknown entity name: %s", entity_name);
        updated = false;
    }

    pthread_mutex_unlock(&data_mutex);

    return updated;
}

// Thread-safe getter functions to access the sensor data
smart_solar_t* get_smart_solar_data(void)
{
    static smart_solar_t safe_copy;

    pthread_mutex_lock(&data_mutex);
    memcpy(&safe_copy, &smart_solar, sizeof(smart_solar_t));
    pthread_mutex_unlock(&data_mutex);

    return &safe_copy;
}

smart_shunt_t* get_smart_shunt_data(void)
{
    static smart_shunt_t safe_copy;

    pthread_mutex_lock(&data_mutex);
    memcpy(&safe_copy, &smart_shunt, sizeof(smart_shunt_t));
    pthread_mutex_unlock(&data_mutex);

    return &safe_copy;
}

climate_sensor_t* get_inside_climate_data(void)
{
    static climate_sensor_t safe_copy;

    pthread_mutex_lock(&data_mutex);
    memcpy(&safe_copy, &inside_climate, sizeof(climate_sensor_t));
    pthread_mutex_unlock(&data_mutex);

    return &safe_copy;
}

climate_sensor_t* get_outside_climate_data(void)
{
    static climate_sensor_t safe_copy;

    pthread_mutex_lock(&data_mutex);
    memcpy(&safe_copy, &outside_climate, sizeof(climate_sensor_t));
    pthread_mutex_unlock(&data_mutex);

    return &safe_copy;
}

camper_sensor_t* get_camper_data(void)
{
    static camper_sensor_t safe_copy;

    pthread_mutex_lock(&data_mutex);
    memcpy(&safe_copy, &camper, sizeof(camper_sensor_t));
    pthread_mutex_unlock(&data_mutex);

    return &safe_copy;
}

/**
 * Get the entity history data
 * @return Pointer to a copy of the entity_history_t data structure
 * @note The caller is responsible for freeing the returned structure with
 * free_entity_history_data()
 */
entity_history_t* get_entity_history_data(void)
{
    history_request_t request;

    // Get a copy of the current request to verify the data matches
    pthread_mutex_lock(&data_mutex);
    memcpy(&request, &current_history_request, sizeof(history_request_t));

    // Allocate memory for history copy
    entity_history_t* history_copy = (entity_history_t*)malloc(sizeof(entity_history_t));
    if(!history_copy)
    {
        log_error("Failed to allocate memory for history data copy");
        pthread_mutex_unlock(&data_mutex);
        return NULL;
    }

    // Zero-initialize the history copy to prevent accessing garbage data
    memset(history_copy, 0, sizeof(entity_history_t));

    /*
    // First verify this is the data we're looking for, but only if entity_history is valid
    if(entity_history.valid && (entity_history.sensor_name[0] != '\0') &&
       (strcmp(entity_history.sensor_name, request.sensor_name) != 0))
    {
        log_warning("Entity history mismatch: requested %s but have %s", request.sensor_name,
                    entity_history.sensor_name);
        // Mark as invalid since it's not the requested data
        history_copy->valid = false;
        pthread_mutex_unlock(&data_mutex);
        return history_copy;
    }
    */

    // Copy basic fields
    history_copy->is_numeric = entity_history.is_numeric;
    strncpy(history_copy->sensor_name, entity_history.sensor_name,
            sizeof(history_copy->sensor_name) - 1);
    history_copy->sensor_name[sizeof(history_copy->sensor_name) - 1] =
        '\0'; // Ensure null termination

    strncpy(history_copy->entity_name, entity_history.entity_name,
            sizeof(history_copy->entity_name) - 1);
    history_copy->entity_name[sizeof(history_copy->entity_name) - 1] =
        '\0'; // Ensure null termination

    strncpy(history_copy->unit, entity_history.unit, sizeof(history_copy->unit) - 1);
    history_copy->unit[sizeof(history_copy->unit) - 1] = '\0'; // Ensure null termination

    history_copy->count = entity_history.count;
    history_copy->valid = entity_history.valid;

    // Allocate and copy arrays
    history_copy->timestamps = NULL;
    history_copy->min        = NULL;
    history_copy->max        = NULL;
    history_copy->mean       = NULL;

    if(entity_history.count > 0)
    {
        // Allocate and copy timestamps
        history_copy->timestamps = (char**)malloc(entity_history.count * sizeof(char*));
        if(history_copy->timestamps)
        {
            memset(history_copy->timestamps, 0, entity_history.count * sizeof(char*));
            for(int i = 0; i < entity_history.count; i++)
            {
                if(entity_history.timestamps[i])
                {
                    history_copy->timestamps[i] = strdup(entity_history.timestamps[i]);
                }
            }
        }

        // Allocate and copy data arrays
        history_copy->min  = (float*)malloc(entity_history.count * sizeof(float));
        history_copy->max  = (float*)malloc(entity_history.count * sizeof(float));
        history_copy->mean = (float*)malloc(entity_history.count * sizeof(float));

        if(history_copy->min && entity_history.min)
        {
            memcpy(history_copy->min, entity_history.min, entity_history.count * sizeof(float));
        }

        if(history_copy->max && entity_history.max)
        {
            memcpy(history_copy->max, entity_history.max, entity_history.count * sizeof(float));
        }

        if(history_copy->mean && entity_history.mean)
        {
            memcpy(history_copy->mean, entity_history.mean, entity_history.count * sizeof(float));
        }
    }

    pthread_mutex_unlock(&data_mutex);

    return history_copy;
}

/**
 * Free memory allocated for an entity_history_t structure
 * @param history Pointer to the entity_history_t structure to free
 */
void free_entity_history_data(entity_history_t* history)
{
    if(!history)
        return;

    clear_entity_history(history);
    free(history);
}
