#ifndef CANVAS_H_
#define CANVAS_H_
#include <SDL3/SDL.h>
#include "appstate.h"
typedef struct AppState AppState;
typedef struct Lines Lines;

typedef struct Layer {
    uint32_t* pixels; // RGBA8888
    SDL_Texture* texture;
    size_t width, height;
    bool is_changed; // True when pixels is updated and texture needs to be
} Layer;

typedef struct Layers {
    Layer* layers;
    size_t layer_count;
    size_t cur_layer;
    Layer edit_layer;
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

// create a default layer of the given size
Layer createLayer(size_t height, size_t width, void* (*calloc_func)(size_t nmemb, size_t size));

// append a new layer to a layers struct
void addLayer(Layers* layers, void* (*realloc_func)(void* mem, size_t size), void* (*calloc_func)(size_t nmemb, size_t size));

// apply all information from src to dest
void mergeLayers(Layer* restrict dest, const Layer* restrict src);

// drawing functions

// convert from screen space to canvas space
void screenToCanvas(AppState *state ,double screen_x, double screen_y, double* out_canvas_x, double* out_canvas_y);

// generate a color from 4 uint8_t
#define makeColor(r,g,b,a) ((uint32_t)( ( (uint32_t)(r) << 24 ) | ( (uint32_t)(g) << 16 ) | ( (uint32_t)(b) << 8 ) | (uint32_t)(a) ))

// fill a layer completely with a solid color
void fillLayer(Layer *layer, uint32_t color);

// apply all lines from a lines object to a given layer
void drawLinesToLayer(Lines *lines, Layer *layer);

#endif