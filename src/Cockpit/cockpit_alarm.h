#ifndef COCKPIT_ALARM_H
#define COCKPIT_ALARM_H

#include <SDL2/SDL.h>
#include "../Systems/aircraft_systems_data.h"
#include "cockpit_layout.h"

typedef struct Cockpit_AlarmState
{
    int warning_active;
    int caution_active;
    int warning_acknowledged;
    int caution_acknowledged;
    SDL_AudioDeviceID audio_device;
    void *audio_context;
} Cockpit_AlarmState;

void cockpit_alarm_init(Cockpit_AlarmState *state);
void cockpit_alarm_destroy(Cockpit_AlarmState *state);
void cockpit_alarm_update(Cockpit_AlarmState *state, const AircraftSystems_Data *systems);
void cockpit_alarm_render(SDL_Renderer *renderer, const Cockpit_Layout *layout, const Cockpit_AlarmState *state, Uint32 ticks);
int cockpit_alarm_handle_click(Cockpit_AlarmState *state, float world_x, float world_y, const Cockpit_Layout *layout);

#endif
