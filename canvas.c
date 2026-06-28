#include <SDL3/SDL.h>
#include "include/appstate.h"
#include "include/canvas.h"

static void drawLayerToRenderer(SDL_Renderer* renderer, Layer* cur_layer) {
    if (!cur_layer->texture) {
        cur_layer->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, cur_layer->width, cur_layer->height);
        SDL_SetTextureBlendMode(cur_layer->texture, SDL_BLENDMODE_BLEND);
        cur_layer->is_changed = true;
    }
    if (cur_layer->is_changed) {
        SDL_UpdateTexture(cur_layer->texture, NULL, cur_layer->pixels, cur_layer->width * sizeof(uint32_t));
        cur_layer->is_changed = false;
    }
    SDL_RenderTexture(renderer, cur_layer->texture, NULL, NULL);
}

SDL_Texture* compositeLayers(SDL_Renderer* renderer, Layers* layers){
    if (!layers->canvas_buffer) {
        layers->canvas_buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, layers->width, layers->height);
        SDL_SetTextureScaleMode(layers->canvas_buffer,SDL_SCALEMODE_NEAREST);
    }
    SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, layers->canvas_buffer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    for (size_t index = 0; index < layers->layer_count; index++){
        // Layer* cur_layer = &(layers->layers[index]);
        drawLayerToRenderer(renderer,&(layers->layers[index]));
        if (index==layers->cur_layer) {
            drawLayerToRenderer(renderer,&(layers->edit_layer));
        }
    }
    SDL_SetRenderTarget(renderer, prev_target);
    return layers->canvas_buffer;
}

Layer createLayer(size_t height, size_t width, void* (*calloc_func)(size_t nmemb, size_t size)) {
    return (Layer){
        .height = height,
        .width = width,
        .pixels = (uint32_t*)(calloc_func(height * width, sizeof(uint32_t))),
        .texture = NULL,
        .is_changed = true
    };
}

void addLayer(Layers* layers, void* (*realloc_func)(void* mem, size_t size), void* (*calloc_func)(size_t nmemb, size_t size)) {
    layers->layer_count++;
    layers->layers = (Layer*)(realloc_func(layers->layers, layers->layer_count * sizeof(Layer)));
    layers->layers[layers->layer_count - 1] = createLayer(layers->height,layers->width,calloc_func);
}

void mergeLayers(Layer* restrict dest, const Layer* restrict src) {
    if (dest->width != src->width || dest->height != src->height) {
        return; 
    }

    size_t total_pixels = dest->width * dest->height;
    uint32_t* restrict d_px = dest->pixels;
    const uint32_t* restrict s_px = src->pixels;

    for (size_t i = 0; i < total_pixels; ++i) {
        uint32_t sp = s_px[i];
        uint32_t sa = sp & 0xFF;

        if (sa == 0) {
            continue;
        }
        if (sa == 255) {
            d_px[i] = sp;
            continue;
        }

        uint32_t dp = d_px[i];
        uint32_t da = dp & 0xFF;
        
        uint32_t sr = (sp >> 24) & 0xFF;
        uint32_t sg = (sp >> 16) & 0xFF;
        uint32_t sb = (sp >> 8) & 0xFF;

        uint32_t dr = (dp >> 24) & 0xFF;
        uint32_t dg = (dp >> 16) & 0xFF;
        uint32_t db = (dp >> 8) & 0xFF;

        uint32_t inv_sa = 255 - sa;

        uint8_t out_r = (sr * sa + dr * inv_sa) / 255;
        uint8_t out_g = (sg * sa + dg * inv_sa) / 255;
        uint8_t out_b = (sb * sa + db * inv_sa) / 255;
        
        uint8_t out_a = sa + (da * inv_sa) / 255;

        d_px[i] = makeColor(out_r,out_g,out_b,out_a);
    }

    dest->is_changed = true;
}

void screenToCanvas(AppState *state ,double screen_x, double screen_y, double* out_canvas_x, double* out_canvas_y) {
    double center_x = (state->screen_width / 2.0) + state->canvas_x;
    double center_y = (state->screen_height / 2.0) + state->canvas_y;

    double dx = screen_x - center_x;
    double dy = screen_y - center_y;

    double rad = state->canvas_rotation * (SDL_PI_F / 180.0);
    double cos_theta = SDL_cos(rad);
    double sin_theta = SDL_sin(rad);

    double rx = (dx * cos_theta) + (dy * sin_theta);
    double ry = (-dx * sin_theta) + (dy * cos_theta);

    double scale = SDL_pow(2, state->canvas_zoom);
    double sx = rx / scale;
    double sy = ry / scale;

    *out_canvas_x = sx + (state->layers->width / 2.0);
    *out_canvas_y = sy + (state->layers->height / 2.0);
}

void fillLayer(Layer *layer, uint32_t color) {
    for (size_t index = 0; index < layer->width*layer->height; index++){
        layer->pixels[index] = color;
    }
}


// drawing

static void drawLineSegment(Layer *layer, SDL_FPoint p1, SDL_FPoint p2, uint32_t color) {
    int x0 = (int)SDL_roundf(p1.x);
    int y0 = (int)SDL_roundf(p1.y);
    int x1 = (int)SDL_roundf(p2.x);
    int y1 = (int)SDL_roundf(p2.y);

    int dx = SDL_abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -SDL_abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int e2;

    while (true) {
        if (x0 >= 0 && x0 < (int)layer->width && y0 >= 0 && y0 < (int)layer->height) {
            
            // brush logic here

            layer->pixels[y0 * layer->width + x0] = color;
        }

        if (x0 == x1 && y0 == y1) break;
        
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void drawLinesToLayer(Lines *lines, Layer *layer) {
    if (!lines || !layer || !layer->pixels) return;

    if (lines->point_count >= 2) {
        uint32_t brush_color = makeColor(0, 0, 0, 255);

        for (size_t i = 0; i < lines->point_count - 1; i++) {
            drawLineSegment(layer, lines->points[i], lines->points[i + 1], brush_color);
        }

        layer->is_changed = true;
    }

    if (lines->is_drawing) {
        if (lines->point_count > 0) {
            lines->points[0] = lines->points[lines->point_count - 1];
            lines->point_count = 1;
        }
    } else {
        lines->point_count = 0;
    }
}