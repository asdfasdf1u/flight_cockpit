#include "cockpit_alarm.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define COCKPIT_ALARM_FLASH_PERIOD_MS 900u
#define COCKPIT_ALARM_CRASH_FLASH_PERIOD_MS 220u
#define COCKPIT_ALARM_AUDIO_RATE 44100
#define COCKPIT_ALARM_CAUTION_TONE_HZ 880
#define COCKPIT_ALARM_WARNING_TONE_HZ 1450
#define COCKPIT_ALARM_STALL_TONE_HZ 115
#define COCKPIT_ALARM_CAUTION_TONE_MS 280
#define COCKPIT_ALARM_AUDIO_CHUNK_MS 180
#define COCKPIT_ALARM_MAX_AUDIO_SAMPLES (COCKPIT_ALARM_AUDIO_RATE * COCKPIT_ALARM_CAUTION_TONE_MS / 1000)

static int text_contains(const char *text, const char *needle)
{
    return text != NULL && needle != NULL && strstr(text, needle) != NULL;
}

/*
 * AlertManager owns the normal master-warning categories.  These two flight
 * conditions are kept from the raw snapshot because AlertManager has no
 * dedicated STALL or OVERSPEED alert type.
 */
static int is_overspeed_warning(const SimWarning *warning)
{
    return warning != NULL && warning->active && warning->level != SIM_WARNING_INFO &&
           text_contains(warning->text, "OVERSPEED");
}

static int is_stall_warning(const SimWarning *warning)
{
    return warning != NULL && warning->active && text_contains(warning->text, "STALL");
}

static void append_signature(char *signature, size_t signature_size, const char *text)
{
    const size_t used = signature != NULL ? strlen(signature) : 0;

    if (signature == NULL || signature_size == 0 || text == NULL || used >= signature_size - 1)
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

static void publish_event(CockpitAlarmState *state, CockpitAlarmLevel level, int active, const char *source)
{
    if (state == NULL)
    {
        return;
    }

    state->last_event.level = level;
    state->last_event.active = active;
    ++state->last_event.sequence;
    snprintf(state->last_event.source, sizeof(state->last_event.source), "%s", source != NULL ? source : "NONE");

    if (state->event_sink != NULL)
    {
        state->event_sink(&state->last_event, state->event_sink_user_data);
    }
}

static void format_log_signature(char *dest, size_t dest_size, const CockpitAlarmState *state)
{
    if (dest == NULL || dest_size == 0 || state == NULL)
    {
        return;
    }

    snprintf(
        dest,
        dest_size,
        "warning=%d:%d:%.96s caution=%d:%d:%d:%.96s stall=%d:%.96s evac=%d",
        state->fire_active,
        state->fire_acknowledged,
        state->fire_signature,
        state->caution_active,
        state->caution_acknowledged,
        state->caution_tone_played,
        state->caution_signature,
        state->stall_active,
        state->stall_signature,
        state->evac_active);
}

static void log_alarm_state(const CockpitAlarmState *state)
{
    char log_signature[sizeof(state->last_log_signature)];
    char warning_source[64];
    char caution_source[64];
    char stall_source[64];

    if (state == NULL)
    {
        return;
    }

    format_log_signature(log_signature, sizeof(log_signature), state);
    if (strcmp(log_signature, state->last_log_signature) == 0)
    {
        return;
    }

    first_source(warning_source, sizeof(warning_source), state->fire_signature);
    first_source(caution_source, sizeof(caution_source), state->caution_signature);
    first_source(stall_source, sizeof(stall_source), state->stall_signature);
    printf(
        "Cockpit Alarm: MASTER_WARNING active=%d ack=%d source=%s; MASTER_CAUTION active=%d ack=%d source=%s; STALL active=%d source=%s; EVAC active=%d\n",
        state->fire_active,
        state->fire_acknowledged,
        warning_source,
        state->caution_active,
        state->caution_acknowledged,
        caution_source,
        state->stall_active,
        stall_source,
        state->evac_active);
    fflush(stdout);
}

static void remember_logged_state(CockpitAlarmState *state)
{
    if (state != NULL)
    {
        format_log_signature(state->last_log_signature, sizeof(state->last_log_signature), state);
    }
}

static void clear_audio(CockpitAlarmState *state)
{
    if (state == NULL)
    {
        return;
    }

    if (state->audio_device != 0)
    {
        SDL_ClearQueuedAudio(state->audio_device);
    }
    state->audio_mode = COCKPIT_ALARM_AUDIO_NONE;
}

static int queue_tone(CockpitAlarmState *state, int frequency_hz, int duration_ms, float amplitude, int pulse_mode)
{
    const int sample_count = COCKPIT_ALARM_AUDIO_RATE * duration_ms / 1000;
    Sint16 samples[COCKPIT_ALARM_MAX_AUDIO_SAMPLES];

    if (state == NULL || state->audio_device == 0 || sample_count <= 0 || sample_count > COCKPIT_ALARM_MAX_AUDIO_SAMPLES)
    {
        return 0;
    }

    for (int i = 0; i < sample_count; ++i)
    {
        const float time = (float)i / (float)COCKPIT_ALARM_AUDIO_RATE;
        const float duration = (float)duration_ms / 1000.0f;
        const float edge = 0.008f;
        float envelope = 1.0f;
        float pulse = 1.0f;
        float waveform = 0.0f;

        if (time < edge)
        {
            envelope = time / edge;
        }
        else if (time > duration - edge)
        {
            envelope = (duration - time) / edge;
        }

        if (pulse_mode == 1)
        {
            const float phase = fmodf(time, 0.090f);
            pulse = phase < 0.052f ? 1.0f : 0.0f;
        }
        else if (pulse_mode == 2)
        {
            const float shaker = 0.40f + 0.60f * sinf(time * 6.283185307179586f * 32.0f);
            waveform = sinf(time * 6.283185307179586f * (float)frequency_hz) * shaker;
        }

        if (pulse_mode != 2)
        {
            waveform = sinf(time * 6.283185307179586f * (float)frequency_hz);
        }

        samples[i] = (Sint16)(waveform * pulse * envelope * amplitude);
    }

    if (SDL_QueueAudio(state->audio_device, samples, (Uint32)((size_t)sample_count * sizeof(samples[0]))) != 0)
    {
        printf("Cockpit Alarm: audio queue failed: %s\n", SDL_GetError());
        fflush(stdout);
        return 0;
    }

    SDL_PauseAudioDevice(state->audio_device, 0);
    return 1;
}

static void service_continuous_audio(CockpitAlarmState *state)
{
    CockpitAlarmAudioMode wanted_mode = COCKPIT_ALARM_AUDIO_NONE;
    const Uint32 chunk_bytes = (Uint32)(COCKPIT_ALARM_AUDIO_RATE * COCKPIT_ALARM_AUDIO_CHUNK_MS / 1000 * (int)sizeof(Sint16));

    if (state == NULL || state->audio_device == 0)
    {
        return;
    }

    if (state->crash_active || (state->engine_fire_active && !state->fire_acknowledged))
    {
        wanted_mode = COCKPIT_ALARM_AUDIO_MASTER_WARNING;
    }
    else if (state->stall_active)
    {
        wanted_mode = COCKPIT_ALARM_AUDIO_STALL;
    }
    else if (state->caution_active && (!state->caution_acknowledged || state->demo_caution_active))
    {
        wanted_mode = COCKPIT_ALARM_AUDIO_MASTER_CAUTION;
    }

    if (wanted_mode != state->audio_mode)
    {
        SDL_ClearQueuedAudio(state->audio_device);
        state->audio_mode = wanted_mode;
    }

    if (wanted_mode == COCKPIT_ALARM_AUDIO_MASTER_WARNING && SDL_GetQueuedAudioSize(state->audio_device) < chunk_bytes)
    {
        (void)queue_tone(state, COCKPIT_ALARM_WARNING_TONE_HZ, COCKPIT_ALARM_AUDIO_CHUNK_MS, 9500.0f, 1);
    }
    else if (wanted_mode == COCKPIT_ALARM_AUDIO_MASTER_CAUTION &&
             !state->caution_tone_played &&
             SDL_GetQueuedAudioSize(state->audio_device) < chunk_bytes)
    {
        state->caution_tone_played = queue_tone(
            state,
            COCKPIT_ALARM_CAUTION_TONE_HZ,
            COCKPIT_ALARM_AUDIO_CHUNK_MS,
            7600.0f,
            1);
    }
    else if (wanted_mode == COCKPIT_ALARM_AUDIO_STALL && SDL_GetQueuedAudioSize(state->audio_device) < chunk_bytes)
    {
        (void)queue_tone(state, COCKPIT_ALARM_STALL_TONE_HZ, COCKPIT_ALARM_AUDIO_CHUNK_MS, 7600.0f, 2);
    }
}

void cockpit_alarm_set_caution_demo(CockpitAlarmState *state, int active)
{
    const int demo_active = active != 0;

    if (state == NULL || state->demo_caution_active == demo_active)
    {
        return;
    }

    state->demo_caution_active = demo_active;
    if (demo_active)
    {
        state->caution_active = 1;
        state->caution_acknowledged = 0;
        state->caution_tone_played = 0;
        snprintf(state->caution_signature, sizeof(state->caution_signature), "%s", "FAULT DEMO");
        publish_event(state, COCKPIT_ALARM_LEVEL_CAUTION, 1, state->caution_signature);
    }
    else
    {
        publish_event(state, COCKPIT_ALARM_LEVEL_CAUTION, 0, state->caution_signature);
        state->caution_active = 0;
        state->caution_acknowledged = 0;
        state->caution_tone_played = 0;
        state->caution_signature[0] = '\0';
    }

    service_continuous_audio(state);
    log_alarm_state(state);
    remember_logged_state(state);
}

void cockpit_alarm_init(CockpitAlarmState *state)
{
    SDL_AudioSpec desired;

    if (state == NULL)
    {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->audio_mode = COCKPIT_ALARM_AUDIO_NONE;

    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0)
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        {
            printf("Cockpit Alarm: audio subsystem unavailable: %s\n", SDL_GetError());
            fflush(stdout);
            return;
        }
        state->audio_subsystem_owned = 1;
    }

    SDL_zero(desired);
    desired.freq = COCKPIT_ALARM_AUDIO_RATE;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = 1024;
    state->audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, NULL, 0);
    if (state->audio_device == 0)
    {
        printf("Cockpit Alarm: audio device unavailable: %s\n", SDL_GetError());
        fflush(stdout);
    }
}

void cockpit_alarm_destroy(CockpitAlarmState *state)
{
    if (state == NULL)
    {
        return;
    }

    if (state->audio_device != 0)
    {
        SDL_CloseAudioDevice(state->audio_device);
    }
    if (state->audio_subsystem_owned)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    memset(state, 0, sizeof(*state));
}

void cockpit_alarm_update(
    CockpitAlarmState *state,
    const AlertSnapshot *alerts,
    const SimSnapshot *snapshot,
    int takeoff_detected)
{
    char warning_signature[sizeof(state->fire_signature)];
    char stall_signature[sizeof(state->stall_signature)];
    int warning_active = 0;
    int engine_fire_active = 0;
    int crash_active = 0;
    int stall_active = 0;
    int warning_is_new = 0;
    int engine_fire_is_new = 0;
    int crash_is_new = 0;
    int stall_is_new = 0;
    int warning_acknowledged = 1;
    int direct_warning_active = 0;

    if (state == NULL)
    {
        return;
    }
    (void)takeoff_detected;

    warning_signature[0] = '\0';
    stall_signature[0] = '\0';

    if (alerts != NULL)
    {
        for (int i = 0; i < ALERT_MANAGER_MAX_ALERTS; ++i)
        {
            const AlertState *alert = &alerts->alerts[i];
            if (!alert->active)
            {
                continue;
            }
            if (alert->type == ALERT_TYPE_ENGINE_FIRE)
            {
                engine_fire_active = 1;
            }
            if (alert->type == ALERT_TYPE_CRASH)
            {
                crash_active = 1;
            }
            if (alert->level == ALERT_LEVEL_WARNING)
            {
                warning_active = 1;
                warning_acknowledged &= alert->acknowledged;
                append_signature(warning_signature, sizeof(warning_signature), alert->message);
            }
        }
    }

    if (snapshot != NULL)
    {
        for (int i = 0; i < snapshot->warning_count; ++i)
        {
            const SimWarning *warning = &snapshot->warnings[i];

            if (is_overspeed_warning(warning))
            {
                warning_active = 1;
                direct_warning_active = 1;
                append_signature(warning_signature, sizeof(warning_signature), warning->text);
            }
            if (is_stall_warning(warning))
            {
                stall_active = 1;
                append_signature(stall_signature, sizeof(stall_signature), warning->text);
            }
        }
    }

    warning_is_new = warning_active &&
                     (!state->fire_active || strcmp(state->fire_signature, warning_signature) != 0);
    engine_fire_is_new = engine_fire_active && !state->engine_fire_active;
    crash_is_new = crash_active && !state->crash_active;
    stall_is_new = stall_active &&
                   (!state->stall_active || strcmp(state->stall_signature, stall_signature) != 0);

    if (state->fire_active && !warning_active)
    {
        publish_event(state, COCKPIT_ALARM_LEVEL_WARNING, 0, state->fire_signature);
    }
    if (state->stall_active && !stall_active)
    {
        publish_event(state, COCKPIT_ALARM_LEVEL_STALL, 0, state->stall_signature);
    }

    if (!warning_active)
    {
        state->fire_acknowledged = 0;
    }
    else if (warning_is_new)
    {
        state->fire_acknowledged = 0;
        publish_event(state, COCKPIT_ALARM_LEVEL_WARNING, 1, warning_signature);
    }
    else
    {
        /* Raw OVERSPEED warnings remain unacknowledged until the pilot presses MASTER WARNING. */
        state->fire_acknowledged = warning_acknowledged &&
                                   (!direct_warning_active || state->fire_acknowledged);
    }

    if (engine_fire_is_new)
    {
        state->fire_flash_started_ticks = SDL_GetTicks();
    }
    if (crash_is_new)
    {
        state->crash_flash_started_ticks = SDL_GetTicks();
    }

    if (stall_is_new)
    {
        publish_event(state, COCKPIT_ALARM_LEVEL_STALL, 1, stall_signature);
    }

    state->fire_active = warning_active;
    state->engine_fire_active = engine_fire_active;
    state->crash_active = crash_active;
    state->stall_active = stall_active;
    snprintf(state->fire_signature, sizeof(state->fire_signature), "%s", warning_signature);
    snprintf(state->stall_signature, sizeof(state->stall_signature), "%s", stall_signature);

    service_continuous_audio(state);
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
    return (Uint8)(value < 0 ? 0 : (value > 255 ? 255 : value));
}

static SDL_Color with_intensity(SDL_Color color, float intensity)
{
    color.a = scaled_alpha(color.a, intensity);
    return color;
}

static float smooth_flash_intensity(Uint32 elapsed_ticks, Uint32 period_ms)
{
    const float phase = (float)(elapsed_ticks % period_ms) / (float)period_ms;
    const float wave = 0.5f + 0.5f * sinf(phase * 6.283185307179586f - 1.5707963267948966f);
    return 0.24f + 0.76f * (wave * wave * (3.0f - 2.0f * wave));
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
    float warning_flash = 0.0f;

    if (renderer == NULL || layout == NULL || state == NULL)
    {
        return;
    }

    if (state->crash_active)
    {
        warning_flash = smooth_flash_intensity(ticks - state->crash_flash_started_ticks, COCKPIT_ALARM_CRASH_FLASH_PERIOD_MS);
    }
    else if (state->engine_fire_active)
    {
        warning_flash = smooth_flash_intensity(ticks - state->fire_flash_started_ticks, COCKPIT_ALARM_FLASH_PERIOD_MS);
    }

    draw_lamp_glow(
        renderer,
        layout->fire_warn_rect,
        (SDL_Color){205, 16, 18, 170},
        warning_flash);
    draw_lamp_glow(
        renderer,
        layout->master_caution_rect,
        (SDL_Color){220, 135, 5, 160},
        state->caution_active && !state->caution_acknowledged ? 1.0f : 0.0f);
}

int cockpit_alarm_handle_click(CockpitAlarmState *state, AlertManager *manager, float world_x, float world_y, const Cockpit_Layout *layout)
{
    SDL_Point point = {(int)world_x, (int)world_y};
    int handled = 0;

    if (state == NULL || layout == NULL)
    {
        return 0;
    }

    if (state->fire_active && SDL_PointInRect(&point, &layout->fire_warn_rect))
    {
        for (int i = 0; manager != NULL && i < ALERT_MANAGER_MAX_ALERTS; ++i)
        {
            if (manager->snapshot.alerts[i].active && manager->snapshot.alerts[i].level == ALERT_LEVEL_WARNING)
            {
                alert_manager_acknowledge(manager, manager->snapshot.alerts[i].type);
            }
        }
        state->fire_acknowledged = 1;
        clear_audio(state);
        handled = 1;
    }
    if (state->caution_active && SDL_PointInRect(&point, &layout->master_caution_rect))
    {
        if (state->demo_caution_active)
        {
            cockpit_alarm_set_caution_demo(state, 0);
            return 1;
        }

        for (int i = 0; manager != NULL && i < ALERT_MANAGER_MAX_ALERTS; ++i)
        {
            if (manager->snapshot.alerts[i].active && manager->snapshot.alerts[i].level == ALERT_LEVEL_CAUTION)
            {
                alert_manager_acknowledge(manager, manager->snapshot.alerts[i].type);
            }
        }
        publish_event(state, COCKPIT_ALARM_LEVEL_CAUTION, 0, state->caution_signature);
        state->caution_active = 0;
        state->caution_acknowledged = 1;
        if (state->audio_mode == COCKPIT_ALARM_AUDIO_NONE)
        {
            clear_audio(state);
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

void cockpit_alarm_set_event_sink(CockpitAlarmState *state, CockpitAlarmEventSink sink, void *user_data)
{
    if (state != NULL)
    {
        state->event_sink = sink;
        state->event_sink_user_data = user_data;
    }
}

const CockpitAlarmEvent *cockpit_alarm_last_event(const CockpitAlarmState *state)
{
    return state != NULL ? &state->last_event : NULL;
}

int cockpit_alarm_request_evac(CockpitAlarmState *state, int ground_mode)
{
    if (state == NULL || !ground_mode)
    {
        return 0;
    }

    if (!state->evac_active)
    {
        state->evac_active = 1;
        publish_event(state, COCKPIT_ALARM_LEVEL_EVAC, 1, "EVAC");
        log_alarm_state(state);
        remember_logged_state(state);
    }
    return 1;
}

void cockpit_alarm_clear_evac(CockpitAlarmState *state)
{
    if (state != NULL && state->evac_active)
    {
        state->evac_active = 0;
        publish_event(state, COCKPIT_ALARM_LEVEL_EVAC, 0, "EVAC");
        log_alarm_state(state);
        remember_logged_state(state);
    }
}
