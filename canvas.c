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

static inline uint8_t getBrushOpacity(float distance, float radius, float softness) {
    if (distance >= radius) return 0;
    if (softness <= 0.0001f) return 255;

    float solid_radius = radius * (1.0f - softness);
    float opacity_float = 1.0f;

    if (distance > solid_radius) {
        float transition_range = radius - solid_radius;
        float x = 1.0f - ((distance - solid_radius) / transition_range);
        opacity_float = x * x * (3.0f - 2.0f * x);
    }

    if (distance == 0.0f && radius < 1.0f) {
        opacity_float *= (1.0f - softness * (1.0f - radius));
    }

    return (uint8_t)(opacity_float * 255.0f);
}

ToolStamp* updateToolStamp(ToolStamp* old_stamp, float radius, float softness) {
    if (radius < 0.5f) {
        radius = 0.5f;
    }

    if (old_stamp && old_stamp->radius == radius && old_stamp->softness == softness) {
        return old_stamp;
    }

    if (old_stamp) {
        SDL_free(old_stamp->stamp);
        SDL_free(old_stamp);
    }

    ToolStamp* stamp_ptr = (ToolStamp*)SDL_malloc(sizeof(ToolStamp));
    if (!stamp_ptr) return NULL;

    int r_ceil = (int)SDL_ceilf(radius);
    size_t dim = (size_t)(r_ceil * 2 + 1);
    
    stamp_ptr->width = dim;
    stamp_ptr->height = dim;
    stamp_ptr->radius = radius;
    stamp_ptr->softness = softness;

    stamp_ptr->stamp = (uint8_t*)SDL_calloc(dim * dim, sizeof(uint8_t));
    if (!stamp_ptr->stamp) {
        SDL_free(stamp_ptr);
        return NULL;
    }

    for (int y = -r_ceil; y <= r_ceil; y++) {
        for (int x = -r_ceil; x <= r_ceil; x++) {
            float dist = SDL_sqrtf((float)(x * x + y * y));
            uint8_t opacity = getBrushOpacity(dist, radius, softness);
            
            stamp_ptr->stamp[(y + r_ceil) * dim + (x + r_ceil)] = opacity;
        }
    }
    
    return stamp_ptr;
}

static void drawStamp(Layer *layer, float cx, float cy, uint32_t color, ToolStamp *stamp) {
    if (!stamp) return;

    const float r = stamp->radius;
    const int dim = (int)stamp->width;

    float offset_x = (float)r - cx;
    float offset_y = (float)r - cy;
    
    int base_iu = (int)SDL_floorf(offset_x);
    int base_iv = (int)SDL_floorf(offset_y);
    
    float fu = offset_x - base_iu;
    float fv = offset_y - base_iv;
    
    int wu = (int)(fu * 256.0f);
    int wv = (int)(fv * 256.0f);
    int iu0 = 256 - wu;
    int iv0 = 256 - wv;

    int start_x = (int)SDL_floorf(cx - r);
    int start_y = (int)SDL_floorf(cy - r);
    
    int end_x = start_x + dim + 1; 
    int end_y = start_y + dim + 1;

    int x1 = SDL_max(0, start_x);
    int y1 = SDL_max(0, start_y);
    int x2 = SDL_min((int)layer->width - 1, end_x);
    int y2 = SDL_min((int)layer->height - 1, end_y);

    if (x1 <= x2 && y1 <= y2) {
        expandDirtyRect(layer, x1, y1);
        expandDirtyRect(layer, x2, y2);
    }

    uint8_t cr = color >> 24;
    uint8_t cg = color >> 16;
    uint8_t cb = color >> 8;
    uint8_t ca = color;

    for (int y = y1; y <= y2; y++) {
        int iv = y + base_iv;
        
        bool y_valid = (iv >= 0 && iv < dim);
        bool y1_valid = (iv + 1 >= 0 && iv + 1 < dim);
        const uint8_t* row0 = y_valid ? (stamp->stamp + iv * dim) : NULL;
        const uint8_t* row1 = y1_valid ? (stamp->stamp + (iv + 1) * dim) : NULL;

        for (int x = x1; x <= x2; x++) {
            int iu = x + base_iu;
            
            bool x_valid = (iu >= 0 && iu < dim);
            bool x1_valid = (iu + 1 >= 0 && iu + 1 < dim);
            
            uint32_t a00 = (row0 && x_valid)  ? row0[iu] : 0;
            uint32_t a10 = (row0 && x1_valid) ? row0[iu + 1] : 0;
            uint32_t a01 = (row1 && x_valid)  ? row1[iu] : 0;
            uint32_t a11 = (row1 && x1_valid) ? row1[iu + 1] : 0;

            uint32_t top = a00 * iu0 + a10 * wu;
            uint32_t bot = a01 * iu0 + a11 * wu;
            uint32_t stamp_alpha = (top * iv0 + bot * wv) >> 16;

            if (!stamp_alpha) continue;

            stamp_alpha = (stamp_alpha * ca) >> 8;

            if (stamp->softness <= 0.0001f) {
                stamp_alpha = (stamp_alpha > 50) ? 255 : 0;
                if (!stamp_alpha) continue; 
            }

            int idx = y * layer->width + x;
            uint32_t dst = layer->pixels[idx];
            uint32_t da = dst & 255;

            da = stamp_alpha + (da * (255 - stamp_alpha)) / 255;
            if (da > 255) da = 255;

            layer->pixels[idx] = makeColor(cr, cg, cb, da);
        }
    }
}

static void drawLineSegment(Layer *layer, SDL_FPoint p1, SDL_FPoint p2, uint32_t color, ToolStamp *stamp, float spacing, float *remainder_dist) {
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float dist = SDL_sqrtf(dx * dx + dy * dy);
    
    float step_dist = spacing * stamp->radius;
    if (step_dist < 0.1f) step_dist = 0.1f;

    if (dist == 0.0f) return;

    float start_dist = *remainder_dist;
    float current_dist = start_dist;
    int step = 0;
    
    while (current_dist <= dist) {
        float t = current_dist / dist;
        float cx = p1.x + dx * t;
        float cy = p1.y + dy * t;
        
        if (cx >= -stamp->radius && cx < layer->width + stamp->radius && 
            cy >= -stamp->radius && cy < layer->height + stamp->radius) {
            drawStamp(layer, cx, cy, color, stamp);
        }
        
        step++;
        current_dist = start_dist + (float)step * step_dist; 
    }
    
    *remainder_dist = current_dist - dist;
}

bool drawLinesToLayer(Lines *lines, Layer *layer, uint32_t color, ToolStamp *stamp, float spacing) {
    if (!lines || !layer || !layer->pixels || !stamp || lines->point_count == 0) return false;

    if (lines->drawn_point_count == 0) {
        drawStamp(layer, lines->points[0].x, lines->points[0].y, color, stamp);
        
        float step_dist = spacing * stamp->radius;
        lines->remainder_dist = (step_dist < 0.1f) ? 0.1f : step_dist;
        lines->drawn_point_count = 1;
        lines->processed_point_count = 1;
        layer->is_changed = true;
    }

    for (size_t i = lines->processed_point_count; i < lines->point_count; i++) {
        if (i > 0) {
            float factor = 0.35f;
            lines->points[i].x = (lines->points[i - 1].x * factor) + (lines->points[i].x * (1.0f - factor));
            lines->points[i].y = (lines->points[i - 1].y * factor) + (lines->points[i].y * (1.0f - factor));
        }
    }
    lines->processed_point_count = lines->point_count;

    if (lines->drawn_point_count >= lines->point_count) {
        if (!lines->is_drawing && lines->point_count > 0) {
            if (lines->point_count > 1 && lines->remainder_dist > 0.1f) {
                SDL_FPoint last_p = lines->points[lines->point_count - 1];
                drawStamp(layer, last_p.x, last_p.y, color, stamp);
                layer->is_changed = true;
            }
            lines->point_count = 0;
            lines->drawn_point_count = 0;
            lines->processed_point_count = 0;
        }
        return layer->is_changed;
    }

    size_t start_idx = lines->drawn_point_count - 1;
    for (size_t i = start_idx; i < lines->point_count - 1; i++) {
        drawLineSegment(layer, lines->points[i], lines->points[i + 1], color, stamp, spacing, &lines->remainder_dist);
        lines->drawn_point_count++;
        layer->is_changed = true;
    }

    return layer->is_changed;
}