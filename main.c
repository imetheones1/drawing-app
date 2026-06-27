#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "include/assets.h"

#define ARENA_IMPLEMENTATION
#include "include/arena.h"

typedef struct AppState {
    SDL_Window *window;
    SDL_Renderer *renderer;
    size_t debug;
} AppState;

static SDL_Texture *test_texture = NULL;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]){
    AppState *state = SDL_malloc(sizeof(AppState));

    if (!SDL_CreateWindowAndRenderer("Hello World", 800, 600, SDL_WINDOW_RESIZABLE, &(state->window), &(state->renderer))) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Surface *test_surface = loadAsset(asset_please,asset_please_len);
    test_texture = SDL_CreateTextureFromSurface(state->renderer,test_surface);
    SDL_DestroySurface(test_surface);

    state->debug = 0;

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event){
    AppState* state = (AppState*)appstate;
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate){
    AppState* state = (AppState*)appstate;
    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(state->renderer, &w, &h);

    float x, y;

    x = (w/2);
    y = ((h) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2;

    SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 255);
    SDL_RenderClear(state->renderer);

    SDL_RenderTexture(state->renderer, test_texture, NULL, NULL);
    SDL_RenderDebugTextFormat(state->renderer, x, y, "%zu",state->debug);


    SDL_RenderPresent(state->renderer);

    state->debug++;

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result){
    AppState* state = (AppState*)appstate;

}