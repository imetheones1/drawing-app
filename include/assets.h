#ifndef ASSET_H_
#define ASSET_H_
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

extern const unsigned char asset_please[];
extern const unsigned int asset_please_len;

SDL_Surface* loadAsset(const unsigned char asset[], const unsigned int asset_len);

#endif