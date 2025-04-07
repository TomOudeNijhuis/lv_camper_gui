#include "http_client.h"
#include "logger.h"
#include "mem_debug.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

static bool http_initialized = false;

// Structure to store the response data
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} http_buffer_t;

// Callback function for libcurl to write received data
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    http_buffer_t *buffer = (http_buffer_t *)userdata;
    size_t real_size = size * nmemb;
    
    // Check if we need to resize the buffer
    if (buffer->size + real_size + 1 > buffer->capacity) {
        size_t new_capacity = buffer->capacity * 2;
        if (new_capacity < buffer->size + real_size + 1) {
            new_capacity = buffer->size + real_size + 1;
        }
        
        char *new_data = mem_realloc(buffer->data, new_capacity);
        if (!new_data) {
            log_error("Failed to allocate memory for HTTP response");
            return 0; // Signal error to libcurl
        }
        
        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }
    
    // Copy the data to our buffer
    memcpy(buffer->data + buffer->size, ptr, real_size);
    buffer->size += real_size;
    buffer->data[buffer->size] = '\0'; // Null-terminate the string
    
    return real_size;
}

bool http_client_init(void) {
    if (http_initialized) {
        return true;
    }
    
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK) {
        log_error("Failed to initialize libcurl: %s", curl_easy_strerror(res));
        return false;
    }
    
    http_initialized = true;
    log_info("HTTP client initialized");
    return true;
}

void http_client_cleanup(void) {
    if (http_initialized) {
        curl_global_cleanup();
        http_initialized = false;
        log_info("HTTP client cleaned up");
    }
}

// Helper function to set up common curl options
static CURL* setup_curl_handle(const char *url, http_buffer_t *buffer, 
                              char *error_buffer, int timeout_seconds) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        log_error("Failed to create curl handle");
        return NULL;
    }
    
    // Set the URL
    curl_easy_setopt(curl, CURLOPT_URL, url);
    
    // Set up the response buffer
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
    
    // Set up error buffer
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
    
    // Set timeout
    if (timeout_seconds > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout_seconds);
    } else {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // Default 30 seconds
    }
    
    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    return curl;
}

http_response_t http_get(const char *url, int timeout_seconds) {
    http_response_t response = {0};
    response.status_code = -1;
    
    if (!http_initialized && !http_client_init()) {
        strcpy(response.error, "HTTP client not initialized");
        return response;
    }
    
    log_debug("Making GET request to %s", url);
    
    // Initialize the response buffer
    http_buffer_t buffer = {0};
    buffer.data = mem_malloc(4096); // Start with 4KB buffer
    if (!buffer.data) {
        log_error("Failed to allocate memory for HTTP response");
        strcpy(response.error, "Memory allocation failed");
        return response;
    }
    buffer.capacity = 4096;
    buffer.size = 0;
    buffer.data[0] = '\0';
    
    // Error buffer for curl
    char error_buffer[CURL_ERROR_SIZE] = {0};
    
    // Set up curl
    CURL *curl = setup_curl_handle(url, &buffer, error_buffer, timeout_seconds);
    if (!curl) {
        mem_free(buffer.data);
        strcpy(response.error, "Failed to initialize curl");
        return response;
    }
    
    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    
    // Check for errors
    if (res != CURLE_OK) {
        log_error("GET request failed: %s", curl_easy_strerror(res));
        if (error_buffer[0]) {
            log_error("Additional info: %s", error_buffer);
            strcpy(response.error, error_buffer);
        } else {
            strcpy(response.error, curl_easy_strerror(res));
        }
        mem_free(buffer.data);
        curl_easy_cleanup(curl);
        return response;
    }
    
    // Get the HTTP status code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    response.status_code = (int)http_code;
    
    // Check if it's a successful HTTP status
    response.success = (http_code >= 200 && http_code < 300);
    
    if (!response.success) {
        log_warning("GET request returned HTTP %ld", http_code);
        snprintf(response.error, sizeof(response.error), "HTTP status %ld", http_code);
    } else {
        log_debug("GET request succeeded with HTTP %ld", http_code);
    }
    
    // Set the response body
    response.body = buffer.data;
    
    // Clean up
    curl_easy_cleanup(curl);
    
    return response;
}

http_response_t http_post_json(const char *url, const char *json_payload, int timeout_seconds) {
    http_response_t response = {0};
    response.status_code = -1;
    
    if (!http_initialized && !http_client_init()) {
        strcpy(response.error, "HTTP client not initialized");
        return response;
    }
    
    log_debug("Making POST request to %s", url);
    if (json_payload) {
        log_debug("Payload: %s", json_payload);
    }
    
    // Initialize the response buffer
    http_buffer_t buffer = {0};
    buffer.data = mem_malloc(4096); // Start with 4KB buffer
    if (!buffer.data) {
        log_error("Failed to allocate memory for HTTP response");
        strcpy(response.error, "Memory allocation failed");
        return response;
    }
    buffer.capacity = 4096;
    buffer.size = 0;
    buffer.data[0] = '\0';
    
    // Error buffer for curl
    char error_buffer[CURL_ERROR_SIZE] = {0};
    
    // Set up curl
    CURL *curl = setup_curl_handle(url, &buffer, error_buffer, timeout_seconds);
    if (!curl) {
        mem_free(buffer.data);
        strcpy(response.error, "Failed to initialize curl");
        return response;
    }
    
    // Set up headers for JSON
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set the request method to POST
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    
    // Set the request body
    if (json_payload) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    } else {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}");
    }
    
    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    
    // Check for errors
    if (res != CURLE_OK) {
        log_error("POST request failed: %s", curl_easy_strerror(res));
        if (error_buffer[0]) {
            log_error("Additional info: %s", error_buffer);
            strcpy(response.error, error_buffer);
        } else {
            strcpy(response.error, curl_easy_strerror(res));
        }
        mem_free(buffer.data);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return response;
    }
    
    // Get the HTTP status code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    response.status_code = (int)http_code;
    
    // Check if it's a successful HTTP status
    response.success = (http_code >= 200 && http_code < 300);
    
    if (!response.success) {
        log_warning("POST request returned HTTP %ld", http_code);
        snprintf(response.error, sizeof(response.error), "HTTP status %ld", http_code);
    } else {
        log_debug("POST request succeeded with HTTP %ld", http_code);
    }
    
    // Set the response body
    response.body = buffer.data;
    
    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return response;
}

void http_response_free(http_response_t *response) {
    if (response) {
        mem_free(response->body);
        response->body = NULL;
    }
}