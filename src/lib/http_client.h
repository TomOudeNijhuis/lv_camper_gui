#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Response structure for HTTP requests
 */
typedef struct
{
    int   status_code; // HTTP status code
    char* body;        // Response body (dynamically allocated)
    bool  success;     // Overall success of the request
    char  error[256];  // Error message if request failed
} http_response_t;

/**
 * Initialize the HTTP client library.
 * Must be called once at application startup.
 *
 * @return true if initialization succeeded, false otherwise
 */
bool http_client_init(void);

/**
 * Clean up the HTTP client library.
 * Should be called before application exit.
 */
void http_client_cleanup(void);

/**
 * Make a GET request
 *
 * @param url The URL to request
 * @param timeout_seconds Maximum time to wait for response (0 for default)
 * @return Response structure (caller must free response.body)
 */
http_response_t http_get(const char* url, int timeout_seconds);

/**
 * Make a POST request with JSON payload
 *
 * @param url The URL to request
 * @param json_payload JSON string to send
 * @param timeout_seconds Maximum time to wait for response (0 for default)
 * @return Response structure (caller must free response.body)
 */
http_response_t http_post_json(const char* url, const char* json_payload, int timeout_seconds);

/**
 * Free resources allocated in an HTTP response
 *
 * @param response The response to free
 */
void http_response_free(http_response_t* response);

#endif /* HTTP_CLIENT_H */