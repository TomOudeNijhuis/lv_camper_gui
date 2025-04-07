#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_mode.h>

#include "lv_sdl_disp.h"
#include "lvgl/lvgl.h"
#include "../ui/ui.h" 
#include "../lib/logger.h"
#include "../lib/mem_debug.h"

/*********************
 *      DEFINES
 *********************/
#ifndef WINDOW_NAME
#define WINDOW_NAME "LVGL"
#endif

/*********************
 *      TYPEDEFS
 *********************/
// Touch data structure to store touch state
typedef struct {
    bool touched;
    int16_t last_x;
    int16_t last_y;
} touch_data_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static void *fb1, *fb2;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;
static lv_display_t *display = NULL;
static int DISPLAY_WIDTH;
static int DISPLAY_HEIGHT;
static touch_data_t touch_data = {0};

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
static int get_sdl_drm_fd(SDL_Window *window);
static uint32_t find_dpms_property_id(int drm_fd, drmModeConnector *conn);
static int drm_set_dpms(int drm_fd, drmModeConnector *conn, int off);
static void sdl_mouse_read(lv_indev_t * indev, lv_indev_data_t * data);
static void sdl_touch_read(lv_indev_t * indev, lv_indev_data_t * data);
static void printWMInfo(SDL_Window *window);
/**********************
 *   DISPLAY FUNCTIONS
 **********************/

/**
 * Display flush callback for LVGL
 */
static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    SDL_Rect r;
    r.x = area->x1;
    r.y = area->y1;
    r.w = area->x2 - area->x1 + 1;
    r.h = area->y2 - area->y1 + 1;
    
    lv_color_t * color_p = (lv_color_t *)px_map;
    
    SDL_UpdateTexture(texture, &r, color_p, r.w * ((LV_COLOR_DEPTH + 7) / 8));
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    lv_display_flush_ready(disp);
}

/**
 * Initialize the SDL display
 * @param width display width in pixels
 * @param height display height in pixels
 */
void lv_port_disp_init(int width, int height)
{
    assert(LV_COLOR_DEPTH == 16 || LV_COLOR_DEPTH == 32);
    DISPLAY_WIDTH = width;
    DISPLAY_HEIGHT = height;
    
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_InitSubSystem failed: %s\n", SDL_GetError());
        exit(1);
    }

    window = SDL_CreateWindow(WINDOW_NAME,
                             SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                             DISPLAY_WIDTH, DISPLAY_HEIGHT, 0);
    
    printWMInfo(window);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        exit(1);
    }

    texture = SDL_CreateTexture(renderer,
                               (LV_COLOR_DEPTH == 32) ? (SDL_PIXELFORMAT_ARGB8888) : (SDL_PIXELFORMAT_RGB565),
                               SDL_TEXTUREACCESS_STREAMING,
                               DISPLAY_WIDTH,
                               DISPLAY_HEIGHT);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        exit(1);
    }

    // Calculate the buffer size in bytes
    size_t buf_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * ((LV_COLOR_DEPTH + 7) / 8);
    
    // Allocate the frame buffers using memory wrappers
    fb1 = mem_malloc(buf_size);
    fb2 = mem_malloc(buf_size);
    
    if (!fb1 || !fb2) {
        fprintf(stderr, "Failed to allocate display buffers\n");
        exit(1);
    }

    // Create a display
    display = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (!display) {
        fprintf(stderr, "Failed to create display\n");
        exit(1);
    }
    
    // Set up buffers for the display
    lv_display_set_buffers(
        display,
        fb1,
        fb2,
        buf_size,
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );
    
    // Set the flush callback
    lv_display_set_flush_cb(display, disp_flush);
    
    // Set color format
    if (LV_COLOR_DEPTH == 32) {
        lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
    } else {
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    }
}

/**
 * Clean up SDL display resources
 */
void lv_port_disp_deinit(void)
{
    if (fb1) {
        mem_free(fb1);
        fb1 = NULL;
    }
    
    if (fb2) {
        mem_free(fb2);
        fb2 = NULL;
    }
    
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = NULL;
    }
    
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/**********************
 *   DRM FUNCTIONS
 **********************/

/**
 * Print SDL window manager information
 */
static void printWMInfo(SDL_Window *window)
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        fprintf(stderr, "Failed to get SDL window manager info: %s\n", SDL_GetError());
        return;
    }

    printf("SDL Window Manager Info:\n");

    switch (wmInfo.subsystem) {
        case SDL_SYSWM_UNKNOWN: 
            printf("  Subsystem: Unknown\n");
            break;
        case SDL_SYSWM_X11: 
            printf("  Subsystem: X11\n");
            printf("  Display: %p\n", (void*)wmInfo.info.x11.display);
            printf("  Window: %lu\n", (unsigned long)wmInfo.info.x11.window);
            break;
        case SDL_SYSWM_WAYLAND: 
            printf("  Subsystem: Wayland\n");
            printf("  Display: %p\n", (void*)wmInfo.info.wl.display);
            printf("  Surface: %p\n", (void*)wmInfo.info.wl.surface);
            break;
        case SDL_SYSWM_DIRECTFB: 
            printf("  Subsystem: DirectFB\n");
            break;
        case SDL_SYSWM_COCOA: 
            printf("  Subsystem: macOS (Cocoa)\n");
            break;
        case SDL_SYSWM_UIKIT: 
            printf("  Subsystem: iOS (UIKit)\n");
            break;
        case SDL_SYSWM_ANDROID: 
            printf("  Subsystem: Android\n");
            break;
        case SDL_SYSWM_VIVANTE:
            printf("  Subsystem: Vivante\n");
            break;
        case SDL_SYSWM_OS2:
            printf("  Subsystem: OS/2\n");
            break;
        case SDL_SYSWM_KMSDRM:
            printf("  Subsystem: KMS/DRM\n");
            printf("  DRM Device File Descriptor: %d\n", wmInfo.info.kmsdrm.dev_index);
            printf("  DRM File Descriptor: %d\n", wmInfo.info.kmsdrm.drm_fd);
            break;
        default:
            printf("  Subsystem: Other\n");
            break;
    }
}

/**
 * Get the KMS/DRM file descriptor from SDL's window
 * Must be called after SDL_CreateWindow(...).
 */
static int get_sdl_drm_fd(SDL_Window *window)
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        log_error("SDL_GetWindowWMInfo failed: %s", SDL_GetError());
        return -1;
    }

    if (wmInfo.subsystem != SDL_SYSWM_KMSDRM) {
        log_error("SDL is not using KMSDRM backend.");
        return -1;
    }

    // The fd SDL is using under the KMS/DRM backend
    return wmInfo.info.kmsdrm.drm_fd;
}

/**
 * Find the DPMS property ID for a given connector
 */
static uint32_t find_dpms_property_id(int drm_fd, drmModeConnector *conn)
{
    drmModeObjectProperties *props =
        drmModeObjectGetProperties(drm_fd, conn->connector_id, DRM_MODE_OBJECT_CONNECTOR);
    if (!props) {
        log_error("drmModeObjectGetProperties failed: %s", strerror(errno));
        return 0;
    }

    // Search properties for "DPMS"
    uint32_t dpms_prop_id = 0;
    for (uint32_t i = 0; i < props->count_props; i++) {
        drmModePropertyRes *prop = drmModeGetProperty(drm_fd, props->props[i]);
        if (prop) {
            if (strcmp(prop->name, "DPMS") == 0) {
                dpms_prop_id = prop->prop_id;
                drmModeFreeProperty(prop);
                break;
            }
            drmModeFreeProperty(prop);
        }
    }

    drmModeFreeObjectProperties(props);
    return dpms_prop_id;
}

/**
 * Blank/unblank the connector by setting DPMS
 * drm_fd : already-open DRM file descriptor (from SDL)
 * off    : 1 to blank, 0 to unblank
 * returns 0 on success, -1 on failure
 */
static int drm_set_dpms(int drm_fd, drmModeConnector *conn, int off)
{
    uint32_t dpms_prop_id = find_dpms_property_id(drm_fd, conn);
    if (dpms_prop_id == 0) {
        log_error("DPMS property not found on connector %u", conn->connector_id);
        return -1;
    }

    // DRM_MODE_DPMS_OFF == 3, DRM_MODE_DPMS_ON == 0
    uint64_t dpms_val = off ? 3 : 0;
    int ret = drmModeConnectorSetProperty(drm_fd, conn->connector_id, dpms_prop_id, dpms_val);
    if (ret != 0) {
        log_error("Failed to set DPMS property to %s on connector %u: %s (err=%d)",
                off ? "OFF" : "ON", conn->connector_id, strerror(errno), ret);
        return -1;
    }
    return 0;
}

/**
 * Blank the display using the same DRM file descriptor SDL is using.
 * @param window SDL window to get DRM fd from
 * @param blank 1 to blank (turn off), 0 to unblank (turn on)
 * @return 0 on success, -1 on failure
 */
int drm_blank_display(SDL_Window *window, int blank)
{
    int drm_fd = get_sdl_drm_fd(window);
    if (drm_fd < 0) {
        return -1;
    }

    drmModeRes *res = drmModeGetResources(drm_fd);
    if (!res) {
        log_error("drmModeGetResources failed: %s", strerror(errno));
        return -1;
    }

    int i;
    int rv = -1;
    // Try all connectors
    for (i = 0; i < res->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(drm_fd, res->connectors[i]);
        if (!conn) continue;

        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            // Found an active connector, set DPMS
            if (drm_set_dpms(drm_fd, conn, blank) == 0) {
                rv = 0;
            }
        }
        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);
    return rv;
}

/**********************
 *   INPUT FUNCTIONS
 **********************/

/**
 * Mouse read callback for LVGL
 */
static void sdl_mouse_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    int x, y;
    uint32_t buttons = SDL_GetMouseState(&x, &y);
    
    data->point.x = x;
    data->point.y = y;
    data->state = (buttons & SDL_BUTTON(1)) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/**
 * Create an SDL mouse input device
 * @return pointer to the created input device or NULL on failure
 */
lv_indev_t* lv_sdl_mouse_create(void)
{
    lv_indev_t * mouse_indev = lv_indev_create();
    if (mouse_indev == NULL) return NULL;
    
    lv_indev_set_type(mouse_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(mouse_indev, sdl_mouse_read);
    
    return mouse_indev;
}

/**
 * Touch read callback for LVGL
 */
static void sdl_touch_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    data->point.x = touch_data.last_x;
    data->point.y = touch_data.last_y;
    data->state = touch_data.touched ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/**
 * Create an SDL touch input device
 * @return pointer to the created input device or NULL on failure
 */
lv_indev_t* lv_sdl_touch_create(void)
{
    // Initialize SDL touch subsystem if needed
    SDL_InitSubSystem(SDL_INIT_EVENTS);
    
    lv_indev_t * touch_indev = lv_indev_create();
    if (touch_indev == NULL) return NULL;
    
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, sdl_touch_read);
    
    return touch_indev;
}

/**
 * Handle SDL events (should be called in the main loop)
 */
void lv_sdl_handle_events(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                exit(0);
                break;

            case SDL_FINGERDOWN:
                touch_data.touched = true;
                touch_data.last_x = event.tfinger.x * DISPLAY_WIDTH;
                touch_data.last_y = event.tfinger.y * DISPLAY_HEIGHT;
                break;
                
            case SDL_FINGERUP:
                touch_data.touched = false;
                break;
                
            case SDL_FINGERMOTION:
                touch_data.last_x = event.tfinger.x * DISPLAY_WIDTH;
                touch_data.last_y = event.tfinger.y * DISPLAY_HEIGHT;
                break;

            default:
                break;
        }

        // Handle sleep mode wake events
        if (ui_is_sleeping()) {
            if (event.type == SDL_KEYDOWN || 
                event.type == SDL_MOUSEMOTION ||
                event.type == SDL_MOUSEBUTTONDOWN ||
                event.type == SDL_FINGERDOWN) {
                ui_exit_sleep_mode();
                continue;
            }
        } else {
            // Reset inactivity timer on any user interaction when not sleeping
            if (event.type == SDL_KEYDOWN || 
                event.type == SDL_MOUSEWHEEL ||
                event.type == SDL_MOUSEMOTION ||
                event.type == SDL_MOUSEBUTTONDOWN ||
                event.type == SDL_FINGERDOWN ||
                event.type == SDL_FINGERMOTION) {
                ui_reset_inactivity_timer();
            }
        }
    }
}