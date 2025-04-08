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
    
    /* Memory debugging options */
    #define MEM_DEBUG_TRACK_ALLOCATIONS 1      /* Track all memory allocations */
    #define MEM_DEBUG_LOG_THRESHOLD     10240  /* Log allocations larger than this size */
#endif

/****************************************************************************
 * Display Constants
 ****************************************************************************/

/* Display power management */
#define DISPLAY_INACTIVITY_TIMEOUT_MS   120000  /* Time until screen blanks in ms */

/****************************************************************************
 * Network Constants
 ****************************************************************************/

/* API endpoints */
#define API_BASE_URL             "http://camperpi.local:8000"

/* Network timeouts */
#define HTTP_TIMEOUT_SECONDS     5      /* HTTP request timeout in seconds */

/****************************************************************************
 * Data Update Intervals
 ****************************************************************************/

#define DATA_UPDATE_INTERVAL_MS  10000  /* Data refresh interval in ms */
#define LOG_REFRESH_INTERVAL_MS  3000   /* Log display refresh interval in ms */
#define BACKGROUND_FETCH_SLEEP_US 200000  /* Background fetcher sleep time in microseconds */

#define MAX_LOG_ENTRIES     100  /* Maximum number of log entries to keep */
#define INITIAL_LOG_LEVEL   LOG_LEVEL_INFO /* Initial log level for displaying logs */

/****************************************************************************
 * Other Application Constants
 ****************************************************************************/

/* Maximum lengths */
#define MAX_URL_LENGTH           256   /* Maximum URL length */
#define MAX_JSON_ACTION_PAYLOAD_LENGTH  128   /* Maximum JSON payload length */

/* Status indicators */
#define WATER_LOW_THRESHOLD      20    /* Water low warning threshold (%) */
#define WASTE_HIGH_THRESHOLD     80    /* Waste high warning threshold (%) */

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
