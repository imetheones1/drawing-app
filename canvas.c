#include <SDL3/SDL.h>
#include "include/canvas.h"

SDL_Texture* compositeLayers(SDL_Renderer* renderer, Layers* layers){
    if (!layers->canvas_buffer) {
        layers->canvas_buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, layers->width, layers->height);
    }
    SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, layers->canvas_buffer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    for (size_t index = 0; index < layers->layer_count; index++){
        Layer* cur_layer = &(layers->layers[index]);
        if (!cur_layer->texture) {
            cur_layer->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, layers->width, layers->height);
            SDL_SetTextureBlendMode(cur_layer->texture, SDL_BLENDMODE_BLEND);
            cur_layer->is_changed = true;
        }
        if (cur_layer->is_changed) {
            SDL_UpdateTexture(cur_layer->texture, NULL, cur_layer->pixels, layers->width * sizeof(uint32_t));
            cur_layer->is_changed = false;
        }
        SDL_RenderTexture(renderer, cur_layer->texture, NULL, NULL);
    }
    SDL_SetRenderTarget(renderer, prev_target);
    return layers->canvas_buffer;
}

void addLayer(Layers* layers, void* (*realloc_func)(void* mem, size_t size), void* (*calloc_func)(size_t nmemb, size_t size)) {
    layers->layer_count++;
    layers->layers = (Layer*)(realloc_func(layers->layers, layers->layer_count * sizeof(Layer)));
    Layer* new_layer = &(layers->layers[layers->layer_count - 1]);
    new_layer->height = layers->height;
    new_layer->width = layers->width;
    new_layer->pixels = (uint32_t*)(calloc_func(layers->height * layers->width, sizeof(uint32_t)));
    new_layer->texture = NULL; 
    new_layer->is_changed = true;
}