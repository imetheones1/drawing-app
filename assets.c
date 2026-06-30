#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "appstate.h"
#include "assets.h"

SDL_Surface* loadAsset(const unsigned char asset[], const unsigned int asset_len){
    SDL_IOStream *stream = SDL_IOFromConstMem(asset,asset_len);
    quitIfNull(stream,"Asset load error","Failed to open IOStream for image asset: %s",SDL_GetError());
    SDL_Surface *surface = IMG_Load_IO(stream,true);
    quitIfNull(surface,"Asset load error","Failed to load image asset: %s",SDL_GetError());
    return surface;
}

TTF_Font* loadFontAsset(const unsigned char asset[], const unsigned int asset_len, float pt_size){
    SDL_IOStream *stream = SDL_IOFromConstMem(asset, asset_len);
    quitIfNull(stream,"Asset load error","Failed to open IOStream for font asset: %s",SDL_GetError());
    TTF_Font *font = TTF_OpenFontIO(stream, true, pt_size);
    quitIfNull(font,"Asset load error","Failed to load font asset: %s",SDL_GetError());
    return font;
}