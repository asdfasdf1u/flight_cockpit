#ifndef FMC_DISPLAY_H
#define FMC_DISPLAY_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "fmc_ui_adapter.h"
#include "fmc_event.h"

typedef struct FMC_Display_Assets
{
    SDL_Texture *panel_texture;
} FMC_Display_Assets;

int fmc_display_assets_load(SDL_Renderer *renderer, FMC_Display_Assets *assets);
void fmc_display_assets_destroy(FMC_Display_Assets *assets);
int fmc_display_window_to_base(SDL_Renderer *renderer, int x, int y, int *base_x, int *base_y);
void fmc_display_render(SDL_Renderer *renderer, TTF_Font *font, const FMC_Display_Assets *assets, const FMC_Event_State *state, const FMC_Data *data);
void fmc_display_render_screen_only(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data, const SDL_Rect *screen_rect);
void fmc_display_render_exec_light_only(SDL_Renderer *renderer, const FMC_Data *data);
void fmc_display_render_hover_only(SDL_Renderer *renderer, const FMC_Event_State *state);

#endif
