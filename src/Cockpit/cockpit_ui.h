#ifndef COCKPIT_UI_H
#define COCKPIT_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef enum Cockpit_Page
{
    COCKPIT_PAGE_OVERVIEW,
    COCKPIT_PAGE_PFD,
    COCKPIT_PAGE_ND,
    COCKPIT_PAGE_EICAS,
    COCKPIT_PAGE_FMC
} Cockpit_Page;

void cockpit_ui_render_overview(
    SDL_Renderer *renderer,
    TTF_Font *font,
    float speed,
    float altitude,
    float heading,
    float fuel,
    const char *active_waypoint);

#endif
