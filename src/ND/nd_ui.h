#ifndef ND_UI_H
#define ND_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "nd_data.h"

void nd_ui_render(SDL_Renderer *renderer, TTF_Font *font, const ND_Data *data);

#endif
