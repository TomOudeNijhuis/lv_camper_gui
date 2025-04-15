/*******************************************************************
 *
 * wifi.h - Wi-Fi status monitoring for Camper GUI on Raspberry Pi
 *
 ******************************************************************/
#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

/**
 * Structure containing the Wi-Fi connection status information
 */
typedef struct
{
    bool wifi_connected;       // True if connected to a Wi-Fi network
    char wifi_ssid[64];        // Name of the connected Wi-Fi network
    int  wifi_signal_strength; // Signal strength as percentage (0-100%)
} wifi_status_t;

/**
 * Initialize Wi-Fi monitoring
 * Must be called before any other Wi-Fi functions
 */
void wifi_init(void);

/**
 * Update Wi-Fi status information
 * Should be called periodically to refresh connection data
 */
void wifi_update(void);

/**
 * Get the current Wi-Fi connection status
 * @return Current Wi-Fi status information
 */
wifi_status_t wifi_get_status(void);

#endif /* WIFI_H */