/*******************************************************************
 *
 * data_actions.c - Functions for controlling camper devices
 *
 ******************************************************************/
#include <stdio.h>
#include <string.h>
#include <json-c/json.h>
#include "data_actions.h"
#include "data_manager.h"
#include "../lib/http_client.h"
#include "../lib/logger.h"
#include "../main.h"

/**
 * Internal function to set camper action
 */
/**
 * Shared response handling for camper action POSTs. Parses the "state" field
 * out of the JSON body and writes it through update_camper_entity.
 */
static void handle_camper_action_response(const char* entity_name, const http_response_t* response)
{
    if(!response->success)
    {
        log_error("Failed to update switch status: %s", response->error);
        if(response->body && *response->body)
        {
            log_error("Response body: %s", response->body);
        }
        return;
    }

    log_info("Switch status updated successfully");
    log_debug("Response: %s", response->body);

    struct json_object* parsed = json_tokener_parse(response->body);
    struct json_object* state_obj;
    if(parsed && json_object_object_get_ex(parsed, "state", &state_obj))
    {
        update_camper_entity(entity_name, json_object_get_string(state_obj));
    }
    else
    {
        log_warning("Action response missing 'state' field: %s", response->body);
    }
    if(parsed)
    {
        json_object_put(parsed);
    }
}

int set_camper_action_internal(const char* entity_name, int state)
{
    char json_payload[MAX_JSON_ACTION_PAYLOAD_LENGTH];
    snprintf(json_payload, sizeof(json_payload), "{\"state\": %d}", state);

    char api_url[MAX_URL_LENGTH];
    snprintf(api_url, sizeof(api_url), "%s/action_by_name/camper/%s", API_BASE_URL, entity_name);

    http_response_t response = http_post_json(api_url, json_payload, HTTP_TIMEOUT_SECONDS);
    handle_camper_action_response(entity_name, &response);
    int success = response.success ? 0 : 1;
    http_response_free(&response);
    return success;
}

int set_camper_mask_action_internal(const char* entity_name, uint16_t mask)
{
    char json_payload[MAX_JSON_ACTION_PAYLOAD_LENGTH];
    snprintf(json_payload, sizeof(json_payload), "{\"mask\": \"0x%04X\"}", mask);

    char api_url[MAX_URL_LENGTH];
    snprintf(api_url, sizeof(api_url), "%s/action_by_name/camper/%s", API_BASE_URL, entity_name);

    http_response_t response = http_post_json(api_url, json_payload, HTTP_TIMEOUT_SECONDS);
    handle_camper_action_response(entity_name, &response);
    int success = response.success ? 0 : 1;
    http_response_free(&response);
    return success;
}
