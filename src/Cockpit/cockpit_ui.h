#ifndef COCKPIT_UI_H
#define COCKPIT_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "cockpit_layout.h"

typedef enum Cockpit_ViewMode
{
    COCKPIT_VIEW_MAIN,
    COCKPIT_VIEW_PFD_ZOOM,
    COCKPIT_VIEW_ND_ZOOM,
    COCKPIT_VIEW_EICAS1_ZOOM,
    COCKPIT_VIEW_EICAS2_ZOOM,
    COCKPIT_VIEW_FMC_ZOOM
} Cockpit_ViewMode;

void cockpit_ui_render_scene(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Cockpit_Layout *layout,
    SDL_Texture *background_texture,
    SDL_Texture *capt_pfd_texture,
    SDL_Texture *capt_nd_texture,
    SDL_Texture *eicas1_texture,
    SDL_Texture *fo_nd_texture,
    SDL_Texture *fo_pfd_texture,
    SDL_Texture *eicas2_texture,
    SDL_Texture *fmc_texture);

SDL_Rect cockpit_ui_fmc_zoom_rect(int window_width, int window_height);
SDL_Rect cockpit_ui_module_zoom_rect(int window_width, int window_height, int texture_width, int texture_height);
void cockpit_ui_render_module_zoom_overlay(
    SDL_Renderer *renderer,
    TTF_Font *font,
    SDL_Texture *module_texture,
    SDL_Rect zoom_rect,
    const char *label);
void cockpit_ui_render_fmc_zoom_overlay(
    SDL_Renderer *renderer,
    TTF_Font *font,
    SDL_Texture *fmc_texture,
    SDL_Texture *fmc_background_texture,
    SDL_Rect zoom_rect,
    Cockpit_FmcSide selected_fmc);

#endif
