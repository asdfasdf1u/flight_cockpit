#ifndef PFD_UI_H
#define PFD_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "pfd_data.h"

void pfd_ui_render(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data);
void pfd_ui_clear_text_cache(SDL_Renderer *renderer);

#endif
