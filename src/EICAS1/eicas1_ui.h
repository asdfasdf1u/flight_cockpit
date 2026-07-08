#ifndef EICAS1_UI_H
#define EICAS1_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "../Systems/aircraft_systems_data.h"
#include "../Util/eicas_ui_common.h"

void eicas1_ui_render(SDL_Renderer *renderer, TTF_Font *font, const AircraftSystems_Data *data);

#endif
