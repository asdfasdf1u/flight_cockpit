#include "cockpit_alarm.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define COCKPIT_ALARM_FLASH_PERIOD_MS 900u
#define COCKPIT_ALARM_CAUTION_TONE_HZ 880
#define COCKPIT_ALARM_CAUTION_TONE_MS 280
#define COCKPIT_ALARM_AUDIO_RATE 44100

static int text_contains(const char *text, const char *needle)
{
    return text != NULL && needle != NULL && strstr(text, needle) != NULL;
}

static int is_fire_warning(const SimWarning *warning)
{
    if (warning == NULL || !warning->active || warning->level != SIM_WARNING_WARNING)
    {
        return 0;
    }

    return text_contains(warning->text, "FIRE");
}

static int is_master_caution_warning(const SimWarning *warning)
{
    if (warning == NULL || !warning->active || warning->level == SIM_WARNING_INFO)
    {
        return 0;
    }

    if (is_fire_warning(warning))
    {
        return 0;
    }

    return 1;
}

static void append_signature(char *signature, size_t signature_size, const char *text)
{
    if (signature == NULL || signature_size == 0 || text == NULL)
    {
        return;
    }

    const size_t used = strlen(signature);
    if (used >= signature_size - 1)
    {
        return;
    }

    snprintf(signature + used, signature_size - used, "%s%s", used > 0 ? "|" : "", text);
}

static void first_source(char *dest, size_t dest_size, const char *signature)
{
    const char *separator = NULL;
    size_t len = 0;

    if (dest == NULL || dest_size == 0)
    {
        return;
    }

    if (signature == NULL || signature[0] == '\0')
    {
        snprintf(dest, dest_size, "NONE");
        return;
    }

    separator = strchr(signature, '|');
    len = separator != NULL ? (size_t)(separator - signature) : strlen(signature);
    if (len >= dest_size)
    {
        len = dest_size - 1;
    }

    memcpy(dest, signature, len);
    dest[len] = '\0';
}

static void log_alarm_state(const CockpitAlarmState *state)
{
    char log_signature[sizeof(state->last_log_signature)];
    char fire_source[64];
    char caution_source[64];

    if (state == NULL)
    {
        return;
    }

    snprintf(
        log_signature,
        sizeof(log_signature),
        "fire=%d:%d:%s caution=%d:%d:%s",
        state->fire_active,
        state->fire_acknowledged,
        state->fire_signature,
        state->caution_active,
        state->caution_acknowledged,
        state->caution_signature);

    if (strcmp(log_signature, state->last_log_signature) == 0)
    {
        return;
    }

    first_source(fire_source, sizeof(fire_source), state->fire_signature);
    first_source(caution_source, sizeof(caution_source), state->caution_signature);
    printf(
        "Cockpit Alarm: FIRE_WARN active=%d ack=%d source=%s; MASTER_CAUTION active=%d ack=%d source=%s\n",
        state->fire_active,
        state->fire_acknowledged,
        fire_source,
        state->caution_active,
        state->caution_acknowledged,
        caution_source);
    fflush(stdout);
}

static void remember_logged_state(CockpitAlarmState *state)
{
    if (state == NULL)
    {
        return;
    }

    snprintf(
        state->last_log_signature,
        sizeof(state->last_log_signature),
        "fire=%d:%d:%s caution=%d:%d:%s",
        state->fire_active,
        state->fire_acknowledged,
        state->fire_signature,
        state->caution_active,
        state->caution_acknowledged,
        state->caution_signature);
}

static void play_master_caution_tone(CockpitAlarmState *state)
{
    const int sample_count = COCKPIT_ALARM_AUDIO_RATE * COCKPIT_ALARM_CAUTION_TONE_MS / 1000;
    Sint16 samples[COCKPIT_ALARM_AUDIO_RATE * COCKPIT_ALARM_CAUTION_TONE_MS / 1000];

    if (state == NULL || state->caution_audio_device == 0)
    {
        return;
    }

    for (int i = 0; i < sample_count; ++i)
    {
        const float time = (float)i / (float)COCKPIT_ALARM_AUDIO_RATE;
        const float edge = 0.025f;
        const float duration = (float)COCKPIT_ALARM_CAUTION_TONE_MS / 1000.0f;
        float envelope = 1.0f;

        if (time < edge)
        {
            envelope = time / edge;
        }
        else if (time > duration - edge)
        {
            envelope = (duration - time) / edge;
        }

        samples[i] = (Sint16)(sinf(time * 6.283185307179586f * (float)COCKPIT_ALARM_CAUTION_TONE_HZ) * envelope * 7200.0f);
    }

    SDL_ClearQueuedAudio(state->caution_audio_device);
    if (SDL_QueueAudio(state->caution_audio_device, samples, (Uint32)sizeof(samples)) != 0)
    {
        printf("Cockpit Alarm: master caution tone queue failed: %s\n", SDL_GetError());
        fflush(stdout);
        return;
    }

    SDL_PauseAudioDevice(state->caution_audio_device, 0);
    printf("Cockpit Alarm: MASTER_CAUTION single tone played.\n");
    fflush(stdout);
}

void cockpit_alarm_init(CockpitAlarmState *state)
{
    if (state == NULL)
    {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->last_log_signature[0] = '\0';

    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0)
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        {
            printf("Cockpit Alarm: audio subsystem unavailable: %s\n", SDL_GetError());
            fflush(stdout);
            return;
        }
        state->caution_audio_subsystem_owned = 1;
    }

    SDL_AudioSpec desired;
    SDL_zero(desired);
    desired.freq = COCKPIT_ALARM_AUDIO_RATE;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = 1024;
    desired.callback = NULL;
    state->caution_audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, NULL, 0);
    if (state->caution_audio_device == 0)
    {
        printf("Cockpit Alarm: master caution tone unavailable: %s\n", SDL_GetError());
        fflush(stdout);
    }
}

void cockpit_alarm_destroy(CockpitAlarmState *state)
{
    if (state == NULL)
    {
        return;
    }

    if (state->caution_audio_device != 0)
    {
        SDL_CloseAudioDevice(state->caution_audio_device);
    }
    if (state->caution_audio_subsystem_owned)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    memset(state, 0, sizeof(*state));
}

void cockpit_alarm_update(CockpitAlarmState *state, const SimSnapshot *snapshot)
{
    char fire_signature[sizeof(state->fire_signature)];
    char caution_signature[sizeof(state->caution_signature)];
    int fire_active = 0;
    int caution_active = 0;
    int caution_is_new = 0;

    if (state == NULL)
    {
        return;
    }

    fire_signature[0] = '\0';
    caution_signature[0] = '\0';

    if (snapshot != NULL)
    {
        for (int i = 0; i < snapshot->warning_count; ++i)
        {
            const SimWarning *warning = &snapshot->warnings[i];
            if (is_fire_warning(warning))
            {
                fire_active = 1;
                append_signature(fire_signature, sizeof(fire_signature), warning->text);
            }
            else if (is_master_caution_warning(warning))
            {
                caution_active = 1;
                append_signature(caution_signature, sizeof(caution_signature), warning->text);
            }
        }
    }

    if (!fire_active)
    {
        state->fire_acknowledged = 0;
    }
    else if (!state->fire_active || strcmp(state->fire_signature, fire_signature) != 0)
    {
        state->fire_acknowledged = 0;
    }

    caution_is_new = caution_active &&
                     (!state->caution_active || strcmp(state->caution_signature, caution_signature) != 0);

    if (!caution_active)
    {
        state->caution_acknowledged = 0;
    }
    else if (caution_is_new)
    {
        state->caution_acknowledged = 0;
    }

    state->fire_active = fire_active;
    state->caution_active = caution_active;
    snprintf(state->fire_signature, sizeof(state->fire_signature), "%s", fire_signature);
    snprintf(state->caution_signature, sizeof(state->caution_signature), "%s", caution_signature);

    if (caution_is_new)
    {
        play_master_caution_tone(state);
    }

    log_alarm_state(state);
    remember_logged_state(state);
}

static void fill(SDL_Renderer *renderer, SDL_Rect rect, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

static Uint8 scaled_alpha(Uint8 alpha, float intensity)
{
    const int value = (int)((float)alpha * intensity + 0.5f);
    if (value < 0)
    {
        return 0;
    }
    if (value > 255)
    {
        return 255;
    }
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
    return 0.24f + 0.76f * eased;
}

static void draw_lamp_glow(SDL_Renderer *renderer, SDL_Rect rect, SDL_Color color, float intensity)
{
    SDL_BlendMode previous;

    if (intensity <= 0.0f)
    {
        return;
    }

    SDL_GetRenderDrawBlendMode(renderer, &previous);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    fill(renderer, (SDL_Rect){rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2}, with_intensity(color, intensity * 0.34f));
    fill(renderer, (SDL_Rect){rect.x + 7, rect.y + 7, rect.w - 14, rect.h - 14}, with_intensity(color, intensity * 0.48f));
    fill(renderer, (SDL_Rect){rect.x + 16, rect.y + 16, rect.w - 32, rect.h - 32}, with_intensity(color, intensity * 0.28f));
    SDL_SetRenderDrawBlendMode(renderer, previous);
}

void cockpit_alarm_render(SDL_Renderer *renderer, const Cockpit_Layout *layout, const CockpitAlarmState *state, Uint32 ticks)
{
    const float flash = smooth_flash_intensity(ticks);

    if (renderer == NULL || layout == NULL || state == NULL)
    {
        return;
    }

    draw_lamp_glow(
        renderer,
        layout->fire_warn_rect,
        (SDL_Color){205, 16, 18, 170},
        state->fire_active && !state->fire_acknowledged ? flash : 0.0f);
    draw_lamp_glow(
        renderer,
        layout->master_caution_rect,
        (SDL_Color){220, 135, 5, 160},
        state->caution_active && !state->caution_acknowledged ? 1.0f : 0.0f);
}

int cockpit_alarm_handle_click(CockpitAlarmState *state, float world_x, float world_y, const Cockpit_Layout *layout)
{
    SDL_Point point = {(int)world_x, (int)world_y};
    int handled = 0;

    if (state == NULL || layout == NULL)
    {
        return 0;
    }

    if (SDL_PointInRect(&point, &layout->fire_warn_rect))
    {
        state->fire_acknowledged = 1;
        handled = 1;
    }
    if (SDL_PointInRect(&point, &layout->master_caution_rect))
    {
        state->caution_acknowledged = 1;
        if (state->caution_audio_device != 0)
        {
            SDL_ClearQueuedAudio(state->caution_audio_device);
        }
        handled = 1;
    }

    if (handled)
    {
        log_alarm_state(state);
        remember_logged_state(state);
    }

    return handled;
}
