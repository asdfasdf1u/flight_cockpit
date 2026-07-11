#include "cockpit_alarm.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct AlarmAudioContext
{
    double phase;
    double pulse_phase;
    int warning_on;
    int caution_on;
} AlarmAudioContext;

#define COCKPIT_ALARM_FLASH_PERIOD_MS 1100u

static void alarm_audio_callback(void *userdata, Uint8 *stream, int len)
{
    AlarmAudioContext *audio = (AlarmAudioContext *)userdata;
    float *samples = (float *)stream;
    const int count = len / (int)sizeof(float);
    const double frequency = audio->warning_on ? 880.0 : 520.0;
    const double step = 6.283185307179586 * frequency / 48000.0;
    const double pulse_step = 6.283185307179586 * 1.6 / 48000.0;
    const float base_volume = audio->warning_on ? 0.16f : (audio->caution_on ? 0.10f : 0.0f);

    for (int i = 0; i < count; ++i)
    {
        const float envelope = 0.35f + 0.65f * (0.5f + 0.5f * (float)sin(audio->pulse_phase));
        samples[i] = base_volume * envelope * (float)sin(audio->phase);
        audio->phase += step;
        audio->pulse_phase += pulse_step;
        if (audio->phase >= 6.283185307179586)
        {
            audio->phase -= 6.283185307179586;
        }
        if (audio->pulse_phase >= 6.283185307179586)
        {
            audio->pulse_phase -= 6.283185307179586;
        }
    }
}

static void set_audio_flags(Cockpit_AlarmState *state)
{
    if (state == NULL || state->audio_context == NULL || state->audio_device == 0)
    {
        return;
    }
    SDL_LockAudioDevice(state->audio_device);
    AlarmAudioContext *audio = (AlarmAudioContext *)state->audio_context;
    audio->warning_on = state->warning_active && !state->warning_acknowledged;
    audio->caution_on = state->caution_active && !state->caution_acknowledged;
    SDL_UnlockAudioDevice(state->audio_device);
}

void cockpit_alarm_init(Cockpit_AlarmState *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));

    AlarmAudioContext *audio = (AlarmAudioContext *)calloc(1, sizeof(*audio));
    if (audio == NULL) return;
    SDL_AudioSpec wanted = {0};
    wanted.freq = 48000;
    wanted.format = AUDIO_F32SYS;
    wanted.channels = 1;
    wanted.samples = 1024;
    wanted.callback = alarm_audio_callback;
    wanted.userdata = audio;
    state->audio_device = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
    if (state->audio_device == 0)
    {
        free(audio);
        return;
    }
    state->audio_context = audio;
    SDL_PauseAudioDevice(state->audio_device, 0);
}

void cockpit_alarm_destroy(Cockpit_AlarmState *state)
{
    if (state == NULL) return;
    if (state->audio_device != 0) SDL_CloseAudioDevice(state->audio_device);
    free(state->audio_context);
    memset(state, 0, sizeof(*state));
}

static int is_engine_warning_text(const char *text)
{
    if (text == NULL)
    {
        return 0;
    }

    return strstr(text, "ENG 1") != NULL ||
           strstr(text, "ENG1") != NULL ||
           strstr(text, "ENG 2") != NULL ||
           strstr(text, "ENG2") != NULL;
}

void cockpit_alarm_update(Cockpit_AlarmState *state, const AircraftSystems_Data *systems)
{
    if (state == NULL || systems == NULL) return;
    int engine_warning = 0;
    for (int i = 0; i < systems->warning_count; ++i)
    {
        const AircraftSystems_WarningItem *item = &systems->warnings[i];
        if (!item->active || item->level == AIRCRAFT_SYSTEMS_WARNING_INFO)
        {
            continue;
        }
        if (is_engine_warning_text(item->text))
        {
            engine_warning = 1;
            break;
        }
    }

    if (engine_warning && (!state->warning_active || !state->caution_active))
    {
        state->warning_acknowledged = 0;
        state->caution_acknowledged = 0;
    }
    if (!engine_warning)
    {
        state->warning_acknowledged = 0;
        state->caution_acknowledged = 0;
    }

    state->warning_active = engine_warning;
    state->caution_active = engine_warning;
    set_audio_flags(state);
}

static void fill(SDL_Renderer *renderer, SDL_Rect rect, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

static Uint8 scaled_alpha(Uint8 alpha, float intensity)
{
    const int value = (int)((float)alpha * intensity + 0.5f);
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (Uint8)value;
}

static SDL_Color with_intensity(SDL_Color color, float intensity)
{
    color.a = scaled_alpha(color.a, intensity);
    return color;
}

static float smooth_flash_intensity(Uint32 ticks)
{
    const float phase = (float)(ticks % COCKPIT_ALARM_FLASH_PERIOD_MS) / (float)COCKPIT_ALARM_FLASH_PERIOD_MS;
    const float wave = 0.5f + 0.5f * sinf(phase * 6.283185307179586f - 1.5707963267948966f);
    const float eased = wave * wave * (3.0f - 2.0f * wave);
    return 0.22f + 0.78f * eased;
}

static void draw_lamp_glow(SDL_Renderer *renderer, SDL_Rect rect, SDL_Color color, float intensity)
{
    if (intensity <= 0.0f) return;
    SDL_BlendMode previous;
    SDL_GetRenderDrawBlendMode(renderer, &previous);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    fill(renderer, (SDL_Rect){rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2}, with_intensity(color, intensity * 0.34f));
    fill(renderer, (SDL_Rect){rect.x + 7, rect.y + 7, rect.w - 14, rect.h - 14}, with_intensity(color, intensity * 0.48f));
    fill(renderer, (SDL_Rect){rect.x + 16, rect.y + 16, rect.w - 32, rect.h - 32}, with_intensity(color, intensity * 0.28f));
    SDL_SetRenderDrawBlendMode(renderer, previous);
}

void cockpit_alarm_render(SDL_Renderer *renderer, const Cockpit_Layout *layout, const Cockpit_AlarmState *state, Uint32 ticks)
{
    if (renderer == NULL || layout == NULL || state == NULL) return;
    const float flash = smooth_flash_intensity(ticks);
    const float warning_intensity = state->warning_active ? (state->warning_acknowledged ? 0.55f : flash) : 0.0f;
    const float caution_intensity = state->caution_active ? (state->caution_acknowledged ? 0.55f : flash) : 0.0f;
    draw_lamp_glow(renderer, layout->fire_warn_rect, (SDL_Color){205, 16, 18, 170}, warning_intensity);
    draw_lamp_glow(renderer, layout->master_caution_rect, (SDL_Color){220, 135, 5, 160}, caution_intensity);
}

int cockpit_alarm_handle_click(Cockpit_AlarmState *state, float world_x, float world_y, const Cockpit_Layout *layout)
{
    if (state == NULL || layout == NULL) return 0;
    SDL_Point point = {(int)world_x, (int)world_y};
    int handled = 0;
    if (SDL_PointInRect(&point, &layout->fire_warn_rect) || SDL_PointInRect(&point, &layout->master_caution_rect))
    {
        state->warning_acknowledged = 1;
        state->caution_acknowledged = 1;
        handled = 1;
    }
    if (!handled) return 0;
    set_audio_flags(state);
    return 1;
}
