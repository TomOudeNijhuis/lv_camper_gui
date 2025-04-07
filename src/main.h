/**
 * @file main.h
 * @brief Main header file for the Camper GUI application
 * 
 * Contains global constants, defines, and configuration settings
 * for the Camper GUI application.
 */
#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 * Application Configuration
 ****************************************************************************/

/* 
 * Uncomment the following line to enable debug mode with additional 
 * logging, memory tracking, and developer features
 */
#define LV_CAMPER_DEBUG

/* Application version information */
#define APP_VERSION_MAJOR    1
#define APP_VERSION_MINOR    0
#define APP_VERSION_PATCH    0
#define APP_VERSION_STRING   "1.0.0"

/****************************************************************************
 * Memory Management Constants
 ****************************************************************************/

/* Memory monitoring */
#ifdef LV_CAMPER_DEBUG
    #define MEM_MONITOR_INTERVAL_MS     30000  /* Memory monitoring interval in ms */
    #define MEM_CHANGE_THRESHOLD_BYTES  1024   /* Log if memory changes by this amount */
#endif

/****************************************************************************
 * Display Constants
 ****************************************************************************/

/* Display power management */
#define DISPLAY_INACTIVITY_TIMEOUT_MS   30000  /* Time until screen blanks in ms */
#define DISPLAY_BRIGHTNESS_DEFAULT      100    /* Default display brightness (0-100) */
#define DISPLAY_BRIGHTNESS_DIMMED       20     /* Dimmed display brightness (0-100) */

/****************************************************************************
 * Network Constants
 ****************************************************************************/

/* API endpoints */
#define API_BASE_URL             "http://camperpi.local:8000"
#define API_SENSOR_ENDPOINT      API_BASE_URL "/sensors"
#define API_ACTION_ENDPOINT      API_BASE_URL "/action"

/* Network timeouts */
#define HTTP_TIMEOUT_SECONDS     5     /* HTTP request timeout in seconds */

/****************************************************************************
 * Data Update Intervals
 ****************************************************************************/

#define DATA_UPDATE_INTERVAL_MS  10000  /* Data refresh interval in ms */
#define LOG_REFRESH_INTERVAL_MS  1000   /* Log display refresh interval in ms */
#define BACKGROUND_FETCH_SLEEP_US 100000  /* Background fetcher sleep time in microseconds */

/****************************************************************************
 * Other Application Constants
 ****************************************************************************/

/* Maximum lengths */
#define MAX_URL_LENGTH           128   /* Maximum URL length */
#define MAX_JSON_PAYLOAD_LENGTH  256   /* Maximum JSON payload length */

/* Status indicators */
#define BATTERY_LOW_THRESHOLD    20    /* Battery low warning threshold (%) */
#define WATER_LOW_THRESHOLD      20    /* Water low warning threshold (%) */
#define WASTE_HIGH_THRESHOLD     80    /* Waste high warning threshold (%) */

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
