#ifndef FMC_UI_H
#define FMC_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "fmc_data.h"

void fmc_ui_render(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data);
FMC_Page fmc_ui_hit_test_page_button(int x, int y, int *hit);
int fmc_ui_hit_test_clear_button(int x, int y);

#endif
