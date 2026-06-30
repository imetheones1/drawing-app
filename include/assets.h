#pragma once

#ifndef ASSET_H_
#define ASSET_H_
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

extern const unsigned char asset_font_roboto[];
extern const unsigned int asset_font_roboto_len;

SDL_Surface* loadAsset(const unsigned char asset[], const unsigned int asset_len);
TTF_Font* loadFontAsset(const unsigned char asset[], const unsigned int asset_len, float pt_size);

#endif