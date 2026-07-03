#pragma once

#ifndef CANVAS_H_
#define CANVAS_H_
#include <SDL3/SDL.h>
#include "appstate.h"
typedef struct AppState AppState;
typedef struct Lines Lines;

extern SDL_Window* window_for_popups;

typedef enum ToolType {
    TOOL_PEN,
    TOOL_ERASER,
    TOOL_COUNT // do not use as a tool
} ToolType;

typedef struct ToolStamp {
    uint8_t *stamp; // array of opacities
    size_t width;
    size_t height;
    float radius;
    float softness;
} ToolStamp;

ToolStamp* updateToolStamp(ToolStamp* old_stamp, float radius, float softness);

typedef struct Layer {
    uint32_t* pixels; // RGBA8888
    SDL_Texture* texture;
    size_t width, height;
    bool is_changed; // True when pixels is updated and texture needs to be
    
    int dirty_x1;
    int dirty_y1;
    int dirty_x2;
    int dirty_y2;
    bool has_dirty_rect;
} Layer;

typedef struct Layers {
    Layer* layers;
    size_t layer_count;
    size_t cur_layer;
    Layer edit_layer;
    size_t width, height; // width & height in pixels
    SDL_Texture* canvas_buffer; // Combined buffer

    SDL_Texture* below_buffer;
    SDL_Texture* above_buffer;
    SDL_Texture* active_layer_buffer;
    size_t last_cur_layer;
    bool static_layers_changed;

    ToolType current_tool;
    uint32_t current_color;
    float current_tool_radius;
    float current_tool_softness;
    float current_tool_spacing;
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
void addLayer(Layers* layers, size_t index, void* (*realloc_func)(void* mem, size_t size), void* (*calloc_func)(size_t nmemb, size_t size), void* (*memmove_func)(void *_Dst, const void *_Src, size_t _Size));

// remove a layer from a layers struct
void removeLayer(Layers* layers, size_t index, void (*free_func)(void* ptr), void* (*realloc_func)(void* mem, size_t size), void* (*memmove_func)(void *_Dst, const void *_Src, size_t _Size));

// apply all information from src to dest
void mergeLayers(Layer* restrict dest, const Layer* restrict src, const bool overwrite, const bool is_eraser);

#define isInsideRectangle(x,y,rx,ry,rw,rh) ((x)>=(rx)&&(x)<((rx)+(rw))&&(y)>=(ry)&&(y)<((ry)+(rh)))

// drawing functions

// convert from screen space to canvas space
void screenToCanvas(AppState *state ,double screen_x, double screen_y, double* out_canvas_x, double* out_canvas_y);

// convert a value to uint8 then to uint32
#define cT8T32(v) (uint32_t)((uint8_t)(v))

// generate a color from 4 uint8_t
#define makeColor(r,g,b,a) ((uint32_t)( ( cT8T32(r) << 24 ) | ( cT8T32(g) << 16 ) | ( cT8T32(b) << 8 ) | cT8T32(a) ))

// convert HSV to RGB
uint32_t HSVtoRGB(float h, float s, float v);

// fill a layer completely with a solid color
void fillLayer(Layer *layer, uint32_t color);

// apply all lines from a lines object to a given layer
bool drawLinesToLayer(Lines *lines, Layer *layer, uint32_t color, float radius, float softness, float spacing);

#endif