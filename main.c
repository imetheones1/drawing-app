#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "include/assets.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

// static SDL_Surface *test_surface = NULL;
static SDL_Texture *test_texture = NULL;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]){
    if (!SDL_CreateWindowAndRenderer("Hello World", 800, 600, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Surface *test_surface = loadAsset(asset_please,asset_please_len);
    test_texture = SDL_CreateTextureFromSurface(renderer,test_surface);
    SDL_DestroySurface(test_surface);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event){
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate){
    // const char *message = "Hello World!";
    // int w = 0, h = 0;
    // float x, y;
    // const float scale = 4.0f;

    // SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
    // SDL_SetRenderScale(renderer, scale, scale);
    // x = ((w / scale) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(message)) / 2;
    // y = ((h / scale) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2;

    // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    // SDL_RenderClear(renderer);
    // SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    // SDL_RenderDebugText(renderer, x, y, message);

    SDL_RenderTexture(renderer, test_texture, NULL, NULL);

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result){
}