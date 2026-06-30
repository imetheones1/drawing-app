#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "assets.h"

SDL_Surface* loadAsset(const unsigned char asset[], const unsigned int asset_len){
    SDL_IOStream *stream = SDL_IOFromConstMem(asset,asset_len);
    SDL_Surface *surface = IMG_Load_IO(stream,true);
    return surface;
}

TTF_Font* loadFontAsset(const unsigned char asset[], const unsigned int asset_len, float pt_size){
    SDL_IOStream *stream = SDL_IOFromConstMem(asset, asset_len);
    TTF_Font *font = TTF_OpenFontIO(stream, true, pt_size);
    return font;
}