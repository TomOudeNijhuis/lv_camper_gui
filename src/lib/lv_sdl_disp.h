//SPDX-License-Identifier: MIT

#ifndef LV_SDL_DISP_H
#define LV_SDL_DISP_H

#include "lvgl/lvgl.h"
#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_Texture *texture;

/**
 * Initialize the SDL display
 * @param width display width in pixels
 * @param height display height in pixels
 */
void lv_port_disp_init(int width, int height);
void lv_port_disp_deinit(void);

int drm_blank_display(SDL_Window *window, int blank);

/**
 * Set display backlight brightness
 * 
 * @param window SDL window to get DRM fd from
 * @param percent Brightness percentage (0-100)
 * @return 0 on success, -1 on failure or no backlight control available
 */
int drm_set_brightness(SDL_Window *window, int percent);

/**
 * Create an SDL mouse input device
 * @return pointer to the created input device or NULL on failure
 */
lv_indev_t* lv_sdl_mouse_create(void);
lv_indev_t* lv_sdl_touch_create(void);

/**
 * Mouse read callback for LVGL
 * @param indev input device driver
 * @param data pointer to the input data
 */
static void sdl_mouse_read(lv_indev_t * indev, lv_indev_data_t * data);

/**
 * Handle SDL events (called in the main loop)
 */
void lv_sdl_handle_events(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_SDL_DISP_H*/