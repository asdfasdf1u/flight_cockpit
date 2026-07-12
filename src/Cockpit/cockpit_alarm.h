#ifndef COCKPIT_ALARM_H
#define COCKPIT_ALARM_H

#include <SDL2/SDL.h>
#include "../Data/sim_snapshot.h"
#include "cockpit_layout.h"

typedef struct CockpitAlarmState
{
    int fire_active;
    int caution_active;
    int fire_acknowledged;
    int caution_acknowledged;
    SDL_AudioDeviceID caution_audio_device;
    int caution_audio_subsystem_owned;
    char fire_signature[128];
    char caution_signature[128];
    char last_log_signature[384];
} CockpitAlarmState;

typedef CockpitAlarmState Cockpit_AlarmState;

void cockpit_alarm_init(CockpitAlarmState *state);
void cockpit_alarm_destroy(CockpitAlarmState *state);
void cockpit_alarm_update(CockpitAlarmState *state, const SimSnapshot *snapshot);
void cockpit_alarm_render(SDL_Renderer *renderer, const Cockpit_Layout *layout, const CockpitAlarmState *state, Uint32 ticks);
int cockpit_alarm_handle_click(CockpitAlarmState *state, float world_x, float world_y, const Cockpit_Layout *layout);

#endif
