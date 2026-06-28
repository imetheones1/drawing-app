#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "include/appstate.h"
#include "include/assets.h"
#include "include/canvas.h"

#define ARENA_IMPLEMENTATION
#include "include/arena.h"


SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]){
    AppState *state = SDL_calloc(1,sizeof(AppState));

    if (!SDL_CreateWindowAndRenderer("Hello World", 800, 600, SDL_WINDOW_RESIZABLE, &(state->window), &(state->renderer))) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderDrawBlendMode(state->renderer,SDL_BLENDMODE_BLEND);

    state->assets = SDL_malloc(sizeof(Assets));

    // SDL_Surface *test_surface = loadAsset(asset_please,asset_please_len);
    // state->assets->test_texture = SDL_CreateTextureFromSurface(state->renderer,test_surface);
    // SDL_DestroySurface(test_surface);

    state->layers = SDL_calloc(1,sizeof(Layers));
    state->layers->layer_count = 0;
    state->layers->width = 400;
    state->layers->height = 400;
    addLayer(state->layers,&SDL_realloc,&SDL_calloc);
    fillLayer(&(state->layers->layers[0]),makeColor(255,255,255,255));
    addLayer(state->layers,&SDL_realloc,&SDL_calloc);
    fillLayer(&(state->layers->layers[1]),makeColor(0,0,0,0));
    state->cur_layer = 1;

    state->cur_lines = SDL_malloc(sizeof(Lines));
    state->cur_lines->point_capacity = 10;
    state->cur_lines->points = SDL_calloc(state->cur_lines->point_capacity,sizeof(SDL_FPoint));
    state->cur_lines->point_count = 0;
    state->cur_lines->is_drawing = false;

    state->canvas_zoom = 0;
    state->canvas_x = 0;
    state->canvas_y = 0;

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event){
    AppState* state = (AppState*)appstate;
    switch (event->type) {
        case SDL_EVENT_QUIT: {
            return SDL_APP_SUCCESS;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            switch (event->button.button) {
                case SDL_BUTTON_LEFT: {
                    state->mouse1=true;
                    double canvas_x = 0;
                    double canvas_y = 0;
                    screenToCanvas(state,event->button.x,event->button.y,&canvas_x,&canvas_y);
                    if (canvas_x < 0 || canvas_x >= state->layers->width || canvas_y < 0 || canvas_y >= state->layers->height) break;
                    // SDL_Log("clicked canvas: %f, %f",canvas_x,canvas_y);
                    state->cur_lines->is_drawing = true;

                    // todo make it draw if theres points

                    state->cur_lines->point_count = 1;
                    state->cur_lines->points[0] = (SDL_FPoint){
                        .x = canvas_x,
                        .y = canvas_y
                    };
                    break;
                }
                case SDL_BUTTON_RIGHT: {state->mouse2=true;break;}
                case SDL_BUTTON_MIDDLE: {state->mouse3=true;break;}
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            if (state->mouse3) {
                // drag
                state->canvas_x += event->motion.xrel;
                state->canvas_y += event->motion.yrel;
            }
            else if (state->mouse2){
                // rotate
                // todo make this a keybind
                state->canvas_rotation -= event->motion.xrel*0.1;
            }
            else if (state->mouse1) {
                double canvas_x = 0;
                double canvas_y = 0;
                screenToCanvas(state,event->motion.x,event->motion.y,&canvas_x,&canvas_y);

                if (state->cur_lines->point_count>=state->cur_lines->point_capacity) break;
                state->cur_lines->points[state->cur_lines->point_count++] = (SDL_FPoint){
                    .x = canvas_x,
                    .y = canvas_y
                };
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            switch (event->button.button) {
                case SDL_BUTTON_LEFT: {
                    state->mouse1=false;
                    if (!state->cur_lines->is_drawing) break;

                    double canvas_x = 0;
                    double canvas_y = 0;
                    screenToCanvas(state,event->button.x,event->button.y,&canvas_x,&canvas_y);

                    if (state->cur_lines->point_count>=state->cur_lines->point_capacity) break;
                    state->cur_lines->points[state->cur_lines->point_count++] = (SDL_FPoint){
                        .x = canvas_x,
                        .y = canvas_y
                    };

                    state->cur_lines->is_drawing = false;
                }
                case SDL_BUTTON_RIGHT: {state->mouse2=false;break;}
                case SDL_BUTTON_MIDDLE: {state->mouse3=false;break;}
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            state->canvas_zoom += event->wheel.y*0.1;
            break;
        }
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate){
    AppState* state = (AppState*)appstate;

    // todo move this to events
    SDL_GetCurrentRenderOutputSize(state->renderer, &state->screen_width, &state->screen_height);

    drawLinesToLayer(state->cur_lines,&(state->layers->layers[state->cur_layer]));

    SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 255);
    SDL_RenderClear(state->renderer);

    compositeLayers(state->renderer,state->layers);
    // SDL_RenderTexture(state->renderer, state->layers->canvas_buffer, NULL, NULL);
    double zoom_factor = SDL_pow(2,state->canvas_zoom);
    SDL_FRect canvas_dest = {
        .w = state->layers->width * zoom_factor,
        .h = state->layers->height * zoom_factor,
    };
    canvas_dest.x = state->canvas_x + state->screen_width/2 - canvas_dest.w/2;
    canvas_dest.y = state->canvas_y + state->screen_height/2 - canvas_dest.h/2;
    SDL_RenderTextureRotated(state->renderer, state->layers->canvas_buffer,NULL,&canvas_dest,state->canvas_rotation,NULL,SDL_FLIP_NONE);

    SDL_RenderPresent(state->renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result){
    AppState* state = (AppState*)appstate;

}