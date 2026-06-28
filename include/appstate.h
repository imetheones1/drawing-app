#ifndef APPSTATE_H_
#define APPSTATE_H_
#include <SDL3/SDL.h>
#include "canvas.h"

typedef struct Assets {
    // SDL_Texture *test_texture;
} Assets;

typedef struct Lines {
    SDL_FPoint *points;
    size_t point_count;
    size_t point_capacity;
    bool is_drawing;
} Lines;

typedef struct AppState {
    SDL_Window *window;
    SDL_Renderer *renderer;
    Assets *assets;
    Layers *layers;

    int screen_width, screen_height; // screen size in pixels

    double canvas_zoom; // 1 = double size, 0 = no change, -1 = half size
    double canvas_x, canvas_y; // transform from center of screen
    double canvas_rotation; // clockwise rotation in degrees

    bool mouse1; // true if mouse1 is held
    bool mouse2; // true if mouse2 is held
    bool mouse3; // true if mouse3 is held

    Lines *cur_lines;
    bool is_edit_finish;
} AppState;

#endif