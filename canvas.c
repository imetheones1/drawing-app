#include <SDL3/SDL.h>
#include "include/appstate.h"
#include "include/canvas.h"

SDL_Window* window_for_popups;

static void expandDirtyRect(Layer* layer, int x, int y) {
    if (!layer->has_dirty_rect) {
        layer->dirty_x1 = x;
        layer->dirty_y1 = y;
        layer->dirty_x2 = x;
        layer->dirty_y2 = y;
        layer->has_dirty_rect = true;
    } else {
        if (x < layer->dirty_x1) layer->dirty_x1 = x;
        if (x > layer->dirty_x2) layer->dirty_x2 = x;
        if (y < layer->dirty_y1) layer->dirty_y1 = y;
        if (y > layer->dirty_y2) layer->dirty_y2 = y;
    }
}

static void updateLayerTexture(SDL_Renderer* renderer, Layer* cur_layer) {
    if (!cur_layer->texture) {
        cur_layer->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, cur_layer->width, cur_layer->height);
        quitIfNull(cur_layer->texture,"Texture allocation error","Failed to create texture for layer: %s",SDL_GetError());
        SDL_SetTextureBlendMode(cur_layer->texture, SDL_BLENDMODE_BLEND);
        cur_layer->is_changed = true;
    }
    
    if (cur_layer->is_changed) {
        if (cur_layer->has_dirty_rect) {
            int x1 = cur_layer->dirty_x1 < 0 ? 0 : cur_layer->dirty_x1;
            int y1 = cur_layer->dirty_y1 < 0 ? 0 : cur_layer->dirty_y1;
            int x2 = cur_layer->dirty_x2 >= (int)cur_layer->width ? (int)cur_layer->width - 1 : cur_layer->dirty_x2;
            int y2 = cur_layer->dirty_y2 >= (int)cur_layer->height ? (int)cur_layer->height - 1 : cur_layer->dirty_y2;

            if (x1 <= x2 && y1 <= y2) {
                SDL_Rect rect = { x1, y1, x2 - x1 + 1, y2 - y1 + 1 };
                uint32_t* src_pixels = cur_layer->pixels + (y1 * cur_layer->width + x1);
                SDL_UpdateTexture(cur_layer->texture, &rect, src_pixels, cur_layer->width * sizeof(uint32_t));
            }
            cur_layer->has_dirty_rect = false;
        } else {
            SDL_UpdateTexture(cur_layer->texture, NULL, cur_layer->pixels, cur_layer->width * sizeof(uint32_t));
        }
        cur_layer->is_changed = false;
    }
}

static void drawLayerToRenderer(SDL_Renderer* renderer, Layer* cur_layer) {
    updateLayerTexture(renderer, cur_layer);
    SDL_RenderTexture(renderer, cur_layer->texture, NULL, NULL);
}

SDL_Texture* compositeLayers(SDL_Renderer* renderer, Layers* layers) {
    if (!layers->canvas_buffer) {
        layers->canvas_buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, layers->width, layers->height);
        quitIfNull(layers->canvas_buffer,"Texture allocation error","Failed to create texture for canvas buffer: %s",SDL_GetError());
        SDL_SetTextureScaleMode(layers->canvas_buffer, SDL_SCALEMODE_NEAREST);

        layers->below_buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, layers->width, layers->height);
        quitIfNull(layers->below_buffer,"Texture allocation error","Failed to create texture for canvas below buffer: %s",SDL_GetError());
        SDL_SetTextureBlendMode(layers->below_buffer, SDL_BLENDMODE_BLEND);

        layers->above_buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, layers->width, layers->height);
        quitIfNull(layers->above_buffer,"Texture allocation error","Failed to create texture for canvas above buffer: %s",SDL_GetError());
        SDL_SetTextureBlendMode(layers->above_buffer, SDL_BLENDMODE_BLEND);
        
        layers->active_layer_buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, layers->width, layers->height);
        quitIfNull(layers->active_layer_buffer,"Texture allocation error","Failed to create texture for canvas active layer buffer: %s",SDL_GetError());
        SDL_SetTextureBlendMode(layers->active_layer_buffer, SDL_BLENDMODE_BLEND);

        layers->last_cur_layer = (size_t)-1;
        layers->static_layers_changed = true;
    }

    for (size_t i = 0; i < layers->layer_count; i++){
        if (layers->layers[i].is_changed) {
            layers->static_layers_changed = true;
            break;
        }
    }

    bool needs_cache_update = layers->static_layers_changed || (layers->last_cur_layer != layers->cur_layer);
    SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);

    if (needs_cache_update) {
        SDL_SetRenderTarget(renderer, layers->below_buffer);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        for (size_t i = 0; i < layers->cur_layer; i++) {
            drawLayerToRenderer(renderer, &(layers->layers[i]));
        }

        SDL_SetRenderTarget(renderer, layers->above_buffer);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        for (size_t i = layers->cur_layer + 1; i < layers->layer_count; i++) {
            drawLayerToRenderer(renderer, &(layers->layers[i]));
        }

        layers->last_cur_layer = layers->cur_layer;
        layers->static_layers_changed = false;
    }

    bool active_layer_changed = (layers->cur_layer < layers->layer_count && layers->layers[layers->cur_layer].is_changed);
    bool needs_active_update = needs_cache_update || layers->edit_layer.is_changed || active_layer_changed;

    if (needs_active_update) {
        SDL_SetRenderTarget(renderer, layers->active_layer_buffer);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        if (layers->cur_layer < layers->layer_count) {
            drawLayerToRenderer(renderer, &(layers->layers[layers->cur_layer]));
        }
        
        updateLayerTexture(renderer, &(layers->edit_layer));
        if (layers->edit_layer.texture) {
            SDL_BlendMode old_blend;
            SDL_GetTextureBlendMode(layers->edit_layer.texture, &old_blend);
            
            if (layers->current_tool == TOOL_ERASER) {
                SDL_BlendMode erase_mode = SDL_ComposeCustomBlendMode(
                    SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
                    SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD
                );
                SDL_SetTextureBlendMode(layers->edit_layer.texture, erase_mode);
            }
            
            SDL_RenderTexture(renderer, layers->edit_layer.texture, NULL, NULL);
            SDL_SetTextureBlendMode(layers->edit_layer.texture, old_blend);
        }
    }

    if (needs_active_update) {
        SDL_SetRenderTarget(renderer, layers->canvas_buffer);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        SDL_RenderTexture(renderer, layers->below_buffer, NULL, NULL);
        SDL_RenderTexture(renderer, layers->active_layer_buffer, NULL, NULL);
        SDL_RenderTexture(renderer, layers->above_buffer, NULL, NULL);
    }

    SDL_SetRenderTarget(renderer, prev_target);

    return layers->canvas_buffer;
}

Layer createLayer(size_t height, size_t width, void* (*calloc_func)(size_t nmemb, size_t size)) {
    uint32_t* allocated_pixels = (uint32_t*)(calloc_func(height * width, sizeof(uint32_t)));
    quitIfNull(allocated_pixels,"Pixel allocation error","Failed to allocate pixel buffer: %s",SDL_GetError()); // todo make get error function a parameter
    return (Layer){
        .height = height,
        .width = width,
        .pixels = allocated_pixels,
        .texture = NULL,
        .is_changed = true,
        .has_dirty_rect = false,
        .dirty_x1 = 0, .dirty_y1 = 0, .dirty_x2 = 0, .dirty_y2 = 0
    };
}

void addLayer(Layers* layers, void* (*realloc_func)(void* mem, size_t size), void* (*calloc_func)(size_t nmemb, size_t size)) {
    layers->layer_count++;
    layers->layers = (Layer*)(realloc_func(layers->layers, layers->layer_count * sizeof(Layer)));
    quitIfNull(layers->layers,"Layer allocation error","Failed to allocate layers buffer: %s",SDL_GetError()); // todo make get error function a parameter
    layers->layers[layers->layer_count - 1] = createLayer(layers->height, layers->width, calloc_func);
    layers->static_layers_changed = true;
}

void mergeLayers(Layer* restrict dest, const Layer* restrict src, const bool overwrite, const bool is_eraser) {
    if (dest->width != src->width || dest->height != src->height) return; 

    size_t total_pixels = dest->width * dest->height;
    uint32_t* restrict d_px = dest->pixels;
    const uint32_t* restrict s_px = src->pixels;

    for (size_t i = 0; i < total_pixels; ++i) {
        uint32_t sp = s_px[i];
        uint32_t sa = sp & 0xFF;

        if (sa == 0) continue;

        uint32_t dp = d_px[i];
        uint32_t da = dp & 0xFF;

        if (is_eraser) {
            uint32_t inv_sa = 255 - sa;
            uint8_t out_a = (da * inv_sa) / 255;
            d_px[i] = (dp & 0xFFFFFF00) | out_a; 
        } else {
            if (sa == 255 || overwrite) {
                d_px[i] = sp;
                continue;
            }

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
    }

    dest->dirty_x1 = 0;
    dest->dirty_y1 = 0;
    dest->dirty_x2 = dest->width - 1;
    dest->dirty_y2 = dest->height - 1;
    dest->has_dirty_rect = true;
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
    layer->dirty_x1 = 0;
    layer->dirty_y1 = 0;
    layer->dirty_x2 = layer->width - 1;
    layer->dirty_y2 = layer->height - 1;
    layer->has_dirty_rect = true;
    layer->is_changed = true;
}

static void drawHorizontalLine(Layer *layer, int x1, int x2, int y, uint32_t color){
    if (y < 0 || y >= layer->height) return;
    
    expandDirtyRect(layer, x1, y);
    expandDirtyRect(layer, x2, y);

    for (int x = x1; x <= x2; x++){
        if (x < 0) continue;
        if (x >= layer->width) break;
        layer->pixels[y * layer->width + x] = color;
    }
}

static void drawFilledCircle(Layer *layer, int cx, int cy, int r, uint32_t color){
    expandDirtyRect(layer, cx - r, cy - r);
    expandDirtyRect(layer, cx + r, cy + r);

    const int d = r * 2;
    int x = r-1;
    int y = 0;
    int tx = 1;
    int ty = 1;
    int error = tx - d;

    while (x >= y) {
        drawHorizontalLine(layer, cx-y, cx+y, cy+x, color);
        drawHorizontalLine(layer, cx-x, cx+x, cy+y, color);
        drawHorizontalLine(layer, cx-x, cx+x, cy-y, color);
        drawHorizontalLine(layer, cx-y, cx+y, cy-x, color);
        if (error <= 0) {
            ++y;
            error += ty;
            ty += 2;
        }
        if (error > 0) {
            --x;
            tx += 2;
            error += (tx - d);
        }
    }
}

static void drawLineSegment(Layer *layer, SDL_FPoint p1, SDL_FPoint p2, uint32_t color, int radius) {
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

            // layer->pixels[y0 * layer->width + x0] = color;
            drawFilledCircle(layer,x0,y0,radius,color);
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

bool drawLinesToLayer(Lines *lines, Layer *layer, uint32_t color, int radius) {
    if (!lines || !layer || !layer->pixels) return false;

    if (lines->point_count < 2) return false;


    for (size_t i = 0; i < lines->point_count - 1; i++) {
        drawLineSegment(layer, lines->points[i], lines->points[i + 1], color, radius);
    }

    layer->is_changed = true;

    if (lines->is_drawing) {
        if (lines->point_count > 0) {
            lines->points[0] = lines->points[lines->point_count - 1];
            lines->point_count = 1;
        }
    } else {
        lines->point_count = 0;
    }

    return true;
}