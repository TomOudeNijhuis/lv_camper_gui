//SPDX-License-Identifier: MIT

#include <assert.h>
#include <SDL2/SDL.h>

#include "lv_sdl_disp.h"
#include "lvgl/lvgl.h"

static void *fb1, *fb2;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;
static lv_display_t *display = NULL;
static int DISPLAY_WIDTH;
static int DISPLAY_HEIGHT;
#ifndef WINDOW_NAME
#define WINDOW_NAME "LVGL"
#endif

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

void lv_port_disp_init(int width, int height)
{
    assert(LV_COLOR_DEPTH == 16 || LV_COLOR_DEPTH == 32);
    DISPLAY_WIDTH = width;
    DISPLAY_HEIGHT = height;
    SDL_InitSubSystem(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(WINDOW_NAME,
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              DISPLAY_WIDTH, DISPLAY_HEIGHT, 0);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    texture = SDL_CreateTexture(renderer,
                                (LV_COLOR_DEPTH == 32) ? (SDL_PIXELFORMAT_ARGB8888) : (SDL_PIXELFORMAT_RGB565),
                                SDL_TEXTUREACCESS_STREAMING,
                                DISPLAY_WIDTH,
                                DISPLAY_HEIGHT);

    // Calculate the buffer size in bytes
    size_t buf_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * ((LV_COLOR_DEPTH + 7) / 8);
    
    // Allocate the frame buffers
    fb1 = malloc(buf_size);
    fb2 = malloc(buf_size);
    
    if(!fb1 || !fb2) {
        fprintf(stderr, "Failed to allocate display buffers\n");
        return;
    }

    // Create a display
    display = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if(!display) {
        fprintf(stderr, "Failed to create display\n");
        return;
    }
    
    // Set up buffers for the display
    lv_display_set_buffers(
        display,
        fb1,
        fb2,
        buf_size,
        LV_DISPLAY_RENDER_MODE_PARTIAL  // You can use DIRECT if you want full refreshes
    );
    
    // Set the flush callback
    lv_display_set_flush_cb(display, disp_flush);
    
    // You can set the DPI if needed
    // lv_display_set_dpi(display, 130);
    
    // Set rotation if needed
    // lv_display_set_rotation(display, LV_DISPLAY_ROTATION_0);
    
    // Enable anti-aliasing if needed
    // lv_display_set_anti_aliasing(display, true);
    
    // Set other properties as needed
    if(LV_COLOR_DEPTH == 32) {
        lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
    } else {
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    }
}
void lv_port_disp_deinit()
{
    if(fb1) {
        free(fb1);
        fb1 = NULL;
    }
    
    if(fb2) {
        free(fb2);
        fb2 = NULL;
    }
    
    if(texture) {
        SDL_DestroyTexture(texture);
        texture = NULL;
    }
    
    if(renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    
    if(window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

// Helper function to create an SDL mouse input device
lv_indev_t* lv_sdl_mouse_create()
{
    lv_indev_t * mouse_indev = lv_indev_create();
    if(mouse_indev == NULL) return NULL;
    
    lv_indev_set_type(mouse_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(mouse_indev, sdl_mouse_read);
    
    return mouse_indev;
}

// Mouse read callback
static void sdl_mouse_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    int x, y;
    uint32_t buttons = SDL_GetMouseState(&x, &y);
    
    data->point.x = x;
    data->point.y = y;
    data->state = (buttons & SDL_BUTTON(1)) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// Touch data structure to store touch state
typedef struct {
    bool touched;
    int16_t last_x;
    int16_t last_y;
} touch_data_t;

static touch_data_t touch_data = {0};

// Touch read callback
static void sdl_touch_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    data->point.x = touch_data.last_x;
    data->point.y = touch_data.last_y;
    data->state = touch_data.touched ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// Helper function to create an SDL touch input device
lv_indev_t* lv_sdl_touch_create()
{
    // Initialize SDL touch subsystem if needed
    SDL_InitSubSystem(SDL_INIT_EVENTS);
    
    lv_indev_t * touch_indev = lv_indev_create();
    if(touch_indev == NULL) return NULL;
    
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, sdl_touch_read);
    
    return touch_indev;
}

// Function to handle SDL events (can be called in your main loop)
void lv_sdl_handle_events()
{
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch(event.type) {
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
    }
}