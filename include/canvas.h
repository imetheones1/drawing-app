#ifndef CANVAS_H_
#define CANVAS_H_
#include <SDL3/SDL.h>

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
 * @param layers The Layers struct to composite
 * @param save_to_buffer True if it should overwrite the canvas_buffer field of Layers
 * @return The composited buffer
 */
SDL_Texture* compositeLayers(Layers* layers, bool save_to_buffer);

#endif