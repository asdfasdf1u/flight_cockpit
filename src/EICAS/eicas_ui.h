#ifndef EICAS_UI_H
#define EICAS_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "eicas_data.h"

void eicas_ui_render(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Data *data);

#endif
