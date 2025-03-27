/*******************************************************************
 *
 * main.c - LVGL Camper GUI for GNU/Linux
 *
 * Integrated SDL backend directly in this file
 *
 ******************************************************************/
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "./lib/lv_sdl_disp.h"
#include "ui/ui.h" 

/* Simulator settings */
typedef struct {
    int window_width;
    int window_height;
} simulator_settings_t;

simulator_settings_t settings = {
    .window_width = 1024,
    .window_height = 600
};

/* Internal functions */
static void configure(int argc, char **argv);
static void print_lvgl_version(void);
static void print_usage(void);
static void tick_thread_init(void);

/**
 * @brief Print LVGL version
 */
static void print_lvgl_version(void)
{
    fprintf(stdout, "%d.%d.%d-%s\n",
            LVGL_VERSION_MAJOR,
            LVGL_VERSION_MINOR,
            LVGL_VERSION_PATCH,
            LVGL_VERSION_INFO);
}

/**
 * @brief Print usage information
 */
static void print_usage(void)
{
    fprintf(stdout, "\ncamper-gui [-V] [-W width] [-H height]\n\n");
    fprintf(stdout, "-V      Print Camper GUI version\n");
    fprintf(stdout, "-W      Set window width\n");
    fprintf(stdout, "-H      Set window height\n");
}

/**
 * @brief Helper function to exit with error message
 */
static void die(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

/**
 * @brief Parse command line arguments and configure the application
 */
static void configure(int argc, char **argv)
{
    int opt = 0;

    /* Default values already set in global settings */

    /* Parse the command-line options. */
    while ((opt = getopt(argc, argv, "W:H:Vh")) != -1) {
        switch (opt) {
        case 'h':
            print_usage();
            exit(EXIT_SUCCESS);
            break;
        case 'V':
            print_lvgl_version();
            exit(EXIT_SUCCESS);
            break;
        case 'W':
            settings.window_width = atoi(optarg);
            break;
        case 'H':
            settings.window_height = atoi(optarg);
            break;
        case ':':
            print_usage();
            die("Option -%c requires an argument.\n", optopt);
            break;
        case '?':
            print_usage();
            die("Unknown option -%c.\n", optopt);
        }
    }
}

/**
 * @brief LVGL tick thread function
 */
static void *tick_thread_cb(void *data)
{
    (void)data;

    while (1) {
        usleep(5000);    /* Sleep for 5 milliseconds */
        lv_tick_inc(5);  /* Tell LVGL that 5 milliseconds has elapsed */
    }

    return NULL;
}

/**
 * @brief Initialize the tick thread for LVGL timing
 */
static void tick_thread_init(void)
{
    pthread_t thread;
    int ret = pthread_create(&thread, NULL, tick_thread_cb, NULL);
    if (ret != 0) {
        die("Failed to create tick thread: %s\n", strerror(ret));
    }
    pthread_detach(thread);
}

/**
 * @brief Initialize LVGL
 */
static void lvgl_init(void)
{
    /* Initialize LVGL library */
    lv_init();
    
    /* Initialize tick thread for LVGL timing */
    tick_thread_init();
}


int main(int argc, char **argv)
{
    /* Parse command line arguments */
    configure(argc, argv);
    
    /* Initialize LVGL first */
    lvgl_init();
    
    /* Initialize the SDL display with the configured size */
    lv_port_disp_init(settings.window_width, settings.window_height);
    
    /* Create an SDL mouse input device */
    lv_indev_t *mouse = lv_sdl_mouse_create();
    if (!mouse) {
        fprintf(stderr, "Warning: Failed to create mouse input device\n");
    }
    
    /* Create a Demo */
    create_ui();
    
    /* Main loop */
    while (1) {
        /* Handle SDL events */
        lv_sdl_handle_events();
        
        /* Let LVGL do its work */
        lv_task_handler();
        
        /* Sleep a bit to reduce CPU usage */
        usleep(10000); /* 10ms */
    }
    
    /* Clean up resources (never reached in normal execution) */
    lv_port_disp_deinit();
    
    return 0;
}