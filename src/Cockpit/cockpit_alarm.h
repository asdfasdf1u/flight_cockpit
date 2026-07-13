#ifndef COCKPIT_ALARM_H
#define COCKPIT_ALARM_H

#include <SDL2/SDL.h>
#include "../Data/alert_manager.h"
#include "cockpit_layout.h"

typedef enum CockpitAlarmLevel
{
    COCKPIT_ALARM_LEVEL_NONE,
    COCKPIT_ALARM_LEVEL_CAUTION,
    COCKPIT_ALARM_LEVEL_WARNING,
    COCKPIT_ALARM_LEVEL_STALL,
    COCKPIT_ALARM_LEVEL_EVAC
} CockpitAlarmLevel;

typedef enum CockpitAlarmAudioMode
{
    COCKPIT_ALARM_AUDIO_NONE,
    COCKPIT_ALARM_AUDIO_MASTER_WARNING,
    COCKPIT_ALARM_AUDIO_STALL
} CockpitAlarmAudioMode;

typedef struct CockpitAlarmEvent
{
    CockpitAlarmLevel level;
    int active;
    unsigned int sequence;
    char source[128];
} CockpitAlarmEvent;

typedef void (*CockpitAlarmEventSink)(const CockpitAlarmEvent *event, void *user_data);

typedef struct CockpitAlarmState
{
    int fire_active;
    int caution_active;
    int stall_active;
    int evac_active;
    int fire_acknowledged;
    int caution_acknowledged;
    int caution_tone_played;
    SDL_AudioDeviceID audio_device;
    int audio_subsystem_owned;
    CockpitAlarmAudioMode audio_mode;
    Uint32 fire_flash_started_ticks;
    char fire_signature[128];
    char caution_signature[128];
    char stall_signature[128];
    char last_log_signature[384];
    CockpitAlarmEvent last_event;
    CockpitAlarmEventSink event_sink;
    void *event_sink_user_data;
} CockpitAlarmState;

typedef CockpitAlarmState Cockpit_AlarmState;

void cockpit_alarm_init(CockpitAlarmState *state);
void cockpit_alarm_destroy(CockpitAlarmState *state);
void cockpit_alarm_update(CockpitAlarmState *state, const AlertSnapshot *alerts);
void cockpit_alarm_render(SDL_Renderer *renderer, const Cockpit_Layout *layout, const CockpitAlarmState *state, Uint32 ticks);
int cockpit_alarm_handle_click(CockpitAlarmState *state, AlertManager *manager, float world_x, float world_y, const Cockpit_Layout *layout);
void cockpit_alarm_set_event_sink(CockpitAlarmState *state, CockpitAlarmEventSink sink, void *user_data);
const CockpitAlarmEvent *cockpit_alarm_last_event(const CockpitAlarmState *state);
int cockpit_alarm_request_evac(CockpitAlarmState *state, int ground_mode);
void cockpit_alarm_clear_evac(CockpitAlarmState *state);

#endif
