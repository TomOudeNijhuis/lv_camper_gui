/*******************************************************************
 *
 * data_actions.c - Functions for controlling camper devices
 *
 ******************************************************************/
#include <stdio.h>
#include <string.h>
#include "data_actions.h"
#include "data_manager.h"
#include "../lib/http_client.h"
#include "../lib/logger.h"
#include "../main.h"

/**
 * Internal function to set camper action
 */
int set_camper_action_internal(const char *entity_name, const char *status) {
    // Prepare the JSON payload
    char json_payload[MAX_JSON_ACTION_PAYLOAD_LENGTH];
    snprintf(json_payload, sizeof(json_payload), "{\"state\": \"%s\"}", status);
    
    // Prepare the URL
    char api_url[MAX_URL_LENGTH];
    snprintf(api_url, sizeof(api_url), "%s/action_by_name/camper/%s", API_BASE_URL, entity_name);

    // Make the POST request
    http_response_t response = http_post_json(api_url, json_payload, HTTP_TIMEOUT_SECONDS);
    
    // Log the result
    if (!response.success) {
        log_error("Failed to update switch status: %s", response.error);
        if (response.body && *response.body) {
            log_error("Response body: %s", response.body);
        }
    } else {
        log_info("Switch status updated successfully");
        log_debug("Response: %s", response.body);

        update_camper_entity(entity_name, status);
    }
    
    // Free the response
    http_response_free(&response);
    
    return response.success ? 0 : 1;
}
