#include "wifi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include "logger.h"

static wifi_status_t current_status;
static bool          initialized = false;

// Initialize Wi-Fi monitoring
void wifi_init(void)
{
    memset(&current_status, 0, sizeof(wifi_status_t));
    current_status.wifi_connected = false;
    strcpy(current_status.wifi_ssid, "");
    current_status.wifi_signal_strength = 0;

    // Perform initial update
    wifi_update();

    initialized = true;
}

// Parse signal strength from iwconfig output (value in dBm)
static int parse_signal_strength(const char* output)
{
    char* signal_level = strstr(output, "Signal level=");
    if(signal_level)
    {
        int dbm = 0;
        if(sscanf(signal_level + 13, "%d", &dbm) == 1)
        {
            // Convert dBm to percentage (approximation)
            // -50 dBm or greater is ~100%, -100 dBm or less is ~0%
            if(dbm >= -50)
                return 100;
            if(dbm <= -100)
                return 0;
            return 2 * (dbm + 100); // Linear mapping from -100..-50 to 0..100
        }
    }
    return 0;
}

// Get the current Wi-Fi status
wifi_status_t wifi_get_status(void)
{
    if(!initialized)
    {
        wifi_init();
    }
    return current_status;
}

// Update Wi-Fi status
void wifi_update(void)
{
    FILE* fp;
    char  output[2048]; // Larger buffer to hold multiple interfaces
    bool  connected             = false;
    char  ssid[64]              = "";
    int   signal_strength       = 0;
    char  current_interface[32] = "";

    // Run iwconfig without specifying adapter to get all wireless interfaces
    fp = popen("iwconfig 2>/dev/null", "r");
    if(fp)
    {
        char*  buffer    = output;
        size_t remaining = sizeof(output) - 1;
        size_t bytes_read;

        // Read all output from iwconfig command
        while((bytes_read = fread(buffer, 1, remaining, fp)) > 0)
        {
            buffer += bytes_read;
            remaining -= bytes_read;

            if(remaining <= 0)
                break;
        }
        *buffer = '\0'; // Null terminate the string
        pclose(fp);

        // Split output by interface (blank lines typically separate interfaces)
        char* interface_entry    = strtok(output, "\n");
        char  interface_name[32] = "";

        while(interface_entry && !connected)
        {
            // A new interface entry typically starts with the interface name
            if(isalpha(interface_entry[0]) && strchr(interface_entry, ' '))
            {
                // Extract interface name (first word before space)
                sscanf(interface_entry, "%31s", interface_name);
            }

            // Check if this line contains ESSID information
            if(strstr(interface_entry, "ESSID:\""))
            {
                char* essid = strstr(interface_entry, "ESSID:\"");
                if(essid && strcmp(essid + 7, "off/any\"") != 0 && strlen(interface_name) > 0)
                {
                    // Found a connected interface
                    connected = true;
                    strncpy(current_interface, interface_name, sizeof(current_interface) - 1);

                    // Extract SSID
                    essid += 7; // Skip "ESSID:\""
                    char* end = strchr(essid, '"');
                    if(end)
                    {
                        *end = '\0';
                        strncpy(ssid, essid, sizeof(ssid) - 1);
                    }
                }
            }

            // Check for signal level on the same interface we found ESSID for
            if(connected && strstr(interface_entry, "Signal level=") &&
               (strlen(interface_name) == 0 || strcmp(interface_name, current_interface) == 0))
            {
                signal_strength = parse_signal_strength(interface_entry);
            }

            interface_entry = strtok(NULL, "\n");
        }

        // If we found a connected interface but no signal strength yet, get it separately
        if(connected && signal_strength == 0 && strlen(current_interface) > 0)
        {
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "iwconfig %s 2>/dev/null | grep 'Signal level'",
                     current_interface);

            FILE* fp2 = popen(cmd, "r");
            if(fp2)
            {
                if(fgets(output, sizeof(output), fp2) != NULL)
                {
                    signal_strength = parse_signal_strength(output);
                }
                pclose(fp2);
            }
        }
    }

    // Update the current status
    current_status.wifi_connected = connected;
    if(connected)
    {
        strncpy(current_status.wifi_ssid, ssid, sizeof(current_status.wifi_ssid) - 1);
        current_status.wifi_signal_strength = signal_strength;
    }
    else
    {
        strcpy(current_status.wifi_ssid, "");
        current_status.wifi_signal_strength = 0;
    }
}