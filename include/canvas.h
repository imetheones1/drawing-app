#ifndef CANVAS_H_
#define CANVAS_H_
#include <SDL3/SDL.h>
#include "appstate.h"
typedef struct AppState AppState;
typedef struct Lines Lines;

typedef struct Layer {
    uint32_t* pixels; // RGBA888
    SDL_Texture* texture;
    size_t width, height;
    bool is_changed; // True when pixels is updated and texture needs to be
} Layer;

typedef struct Layers {
    Layer* layers;
    size_t layer_count;
    size_t width, height; // width & height in pixels
    SDL_Texture* canvas_buffer; // Combined buffer
} Layers;

/**
 * Composites all of the layers within a Layers struct into a single buffer.
 *
 * @param renderer The renderer of the textures
 * @param layers The Layers struct to composite
 * @return The composited buffer
 */
SDL_Texture* compositeLayers(SDL_Renderer* renderer, Layers* layers);

void addLayer(Layers* layers, void* (*realloc_func)(void* mem, size_t size), void* (*calloc_func)(size_t nmemb, size_t size));

// drawing functions

void screenToCanvas(AppState *state ,double screen_x, double screen_y, double* out_canvas_x, double* out_canvas_y);

#define makeColor(r,g,b,a) ((uint32_t)( ( (uint32_t)(r) << 24 ) | ( (uint32_t)(g) << 16 ) | ( (uint32_t)(b) << 8 ) | (uint32_t)(a) ))

void fillLayer(Layer *layer, uint32_t color);

void drawLinesToLayer(Lines *lines, Layer *layer);

#endif