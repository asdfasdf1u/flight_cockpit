#ifndef FMC_EVENT_H
#define FMC_EVENT_H

#include <SDL2/SDL.h>

#include "fmc_key.h"

typedef struct FMC_Event_State
{
    FMC_ButtonId hovered_button;
    int hovered_button_index;
} FMC_Event_State;

void fmc_event_state_init(FMC_Event_State *state);
void fmc_event_update_hover(SDL_Renderer *renderer, FMC_Event_State *state, int x, int y);
int fmc_event_handle_mouse_button(SDL_Renderer *renderer, FMC_Event_State *state, FMC_Data *data, int x, int y);
void fmc_event_update_hover_base(FMC_Event_State *state, int base_x, int base_y);
int fmc_event_handle_mouse_button_base(FMC_Event_State *state, FMC_Data *data, int base_x, int base_y);
const FMC_Button *fmc_event_hit_test_button(SDL_Renderer *renderer, int x, int y);

#endif
