#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

SDL_Surface* loadAsset(const unsigned char asset[], const unsigned int asset_len){
    SDL_IOStream *stream = SDL_IOFromConstMem(asset,asset_len);
    SDL_Surface *surface = IMG_Load_IO(stream,true);
    return surface;
}