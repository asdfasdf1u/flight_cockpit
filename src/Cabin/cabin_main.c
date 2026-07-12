#include "cabin_main.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cabin_api.h"
#include "cabin_data.h"
#include "cabin_ui.h"

#define CABIN_WINDOW_WIDTH 1600
#define CABIN_WINDOW_HEIGHT 900
#define CABIN_TARGET_FRAME_MS 16

#define CABIN_CRASH_AUDIO_RATE 44100
#define CABIN_CRASH_AUDIO_CHUNK_MS 180
#define CABIN_CRASH_AUDIO_SAMPLES (CABIN_CRASH_AUDIO_RATE * CABIN_CRASH_AUDIO_CHUNK_MS / 1000)
#define CABIN_CRASH_BEEP_HZ 1350.0f
#define CABIN_CRASH_BEEP_PERIOD_MS 260
#define CABIN_CRASH_BEEP_ON_MS 160
#define CABIN_CRASH_BEEP_EDGE_MS 6

#define CABIN_MAP_PATH "assets/20260303110928.png"
#define CABIN_PLANE_PATH "assets/plane.png"
#define CABIN_FULLSCREEN_PATH "assets/full_screen.png"
#define CABIN_ADD_PATH "assets/add.png"
#define CABIN_SUB_PATH "assets/sub.png"
#define CABIN_FONT_PATH "assets/ALIBABAPUHUITI-2-45-LIGHT.TTF"

typedef struct Cabin_Crash_Audio
{
    SDL_AudioDeviceID device;
    int was_active;
    Uint64 sample_cursor;
} Cabin_Crash_Audio;

static void cabin_crash_audio_init(Cabin_Crash_Audio *audio)
{
    SDL_AudioSpec desired;

    if (audio == NULL)
    {
        return;
    }

    memset(audio, 0, sizeof(*audio));
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0 && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        printf("Cabin CRASH DEMO: audio subsystem unavailable: %s\n", SDL_GetError());
        return;
    }

    SDL_zero(desired);
    desired.freq = CABIN_CRASH_AUDIO_RATE;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = 1024;
    audio->device = SDL_OpenAudioDevice(NULL, 0, &desired, NULL, 0);
    if (audio->device == 0)
    {
        printf("Cabin CRASH DEMO: audio device unavailable: %s\n", SDL_GetError());
    }
}

static int cabin_crash_audio_queue_chunk(Cabin_Crash_Audio *audio)
{
    Sint16 samples[CABIN_CRASH_AUDIO_SAMPLES];
    const float two_pi = 6.28318530717958647692f;
    const Uint64 period_samples = (Uint64)CABIN_CRASH_AUDIO_RATE * CABIN_CRASH_BEEP_PERIOD_MS / 1000u;
    const Uint64 on_samples = (Uint64)CABIN_CRASH_AUDIO_RATE * CABIN_CRASH_BEEP_ON_MS / 1000u;
    const Uint64 edge_samples = (Uint64)CABIN_CRASH_AUDIO_RATE * CABIN_CRASH_BEEP_EDGE_MS / 1000u;

    if (audio == NULL || audio->device == 0)
    {
        return 0;
    }

    for (int i = 0; i < CABIN_CRASH_AUDIO_SAMPLES; ++i)
    {
        const Uint64 cycle_sample = audio->sample_cursor % period_samples;
        float envelope = 0.0f;

        if (cycle_sample < on_samples)
        {
            envelope = 1.0f;
            if (cycle_sample < edge_samples)
            {
                envelope = (float)cycle_sample / (float)edge_samples;
            }
            else if (cycle_sample > on_samples - edge_samples)
            {
                envelope = (float)(on_samples - cycle_sample) / (float)edge_samples;
            }
        }

        const float time = (float)audio->sample_cursor / (float)CABIN_CRASH_AUDIO_RATE;
        const float fundamental = sinf(time * two_pi * CABIN_CRASH_BEEP_HZ);
        const float harmonic = sinf(time * two_pi * CABIN_CRASH_BEEP_HZ * 2.0f);
        const float buzzer = fundamental * 0.82f + harmonic * 0.18f;
        samples[i] = (Sint16)(buzzer * envelope * 11800.0f);
        ++audio->sample_cursor;
    }
    if (SDL_QueueAudio(audio->device, samples, (Uint32)sizeof(samples)) != 0)
    {
        printf("Cabin CRASH DEMO: failed to queue alarm audio: %s\n", SDL_GetError());
        return 0;
    }
    SDL_PauseAudioDevice(audio->device, 0);
    return 1;
}

static void cabin_crash_audio_update(Cabin_Crash_Audio *audio, int active)
{
    const Uint32 chunk_bytes = (Uint32)(CABIN_CRASH_AUDIO_SAMPLES * (int)sizeof(Sint16));

    if (audio == NULL || audio->device == 0)
    {
        return;
    }

    if (!active)
    {
        if (audio->was_active)
        {
            SDL_ClearQueuedAudio(audio->device);
            SDL_PauseAudioDevice(audio->device, 1);
            audio->sample_cursor = 0;
        }
        audio->was_active = 0;
        return;
    }

    audio->was_active = 1;
    while (SDL_GetQueuedAudioSize(audio->device) < chunk_bytes * 2u)
    {
        if (!cabin_crash_audio_queue_chunk(audio))
        {
            break;
        }
    }
}

static void cabin_crash_audio_destroy(Cabin_Crash_Audio *audio)
{
    if (audio == NULL)
    {
        return;
    }
    if (audio->device != 0)
    {
        SDL_ClearQueuedAudio(audio->device);
        SDL_CloseAudioDevice(audio->device);
    }
    memset(audio, 0, sizeof(*audio));
}

static void resolve_weather_city(const Cabin_Data *data, char *city, size_t city_size, char *adcode, size_t adcode_size)
{
    if (city != NULL && city_size > 0)
    {
        city[0] = '\0';
    }
    if (adcode != NULL && adcode_size > 0)
    {
        adcode[0] = '\0';
    }

    if (data == NULL)
    {
        return;
    }

    if (strcmp(data->current_city, "北京") == 0 ||
        strcmp(data->current_city, "北京市") == 0 ||
        strcmp(data->current_district, "北京") == 0 ||
        strcmp(data->current_district, "北京市") == 0)
    {
        snprintf(city, city_size, "%s", "北京");
        snprintf(adcode, adcode_size, "%s", "110000");
        return;
    }
    if (strcmp(data->current_city, "陕西省") == 0 ||
        strcmp(data->current_district, "西安") == 0 ||
        strcmp(data->current_district, "西安市") == 0)
    {
        snprintf(city, city_size, "%s", "西安");
        snprintf(adcode, adcode_size, "%s", "610100");
        return;
    }
    if (strcmp(data->current_city, "成都") == 0 ||
        strcmp(data->current_city, "成都市") == 0 ||
        strcmp(data->current_city, "四川省") == 0 ||
        strcmp(data->current_district, "成都") == 0 ||
        strcmp(data->current_district, "成都市") == 0)
    {
        snprintf(city, city_size, "%s", "成都");
        snprintf(adcode, adcode_size, "%s", "510100");
        return;
    }

    snprintf(city, city_size, "%s", "飞行途中");
    snprintf(adcode, adcode_size, "%s", "");
    return;
}

static void update_weather_if_city_changed(Cabin_Data *data, char *last_city, size_t last_city_size, char *last_adcode, size_t last_adcode_size)
{
    char city[CABIN_TEXT_LEN];
    char adcode[CABIN_TEXT_LEN];

    resolve_weather_city(data, city, sizeof(city), adcode, sizeof(adcode));
    if (city[0] == '\0')
    {
        return;
    }

    if (strcmp(city, last_city) == 0 && strcmp(adcode, last_adcode) == 0)
    {
        return;
    }

    printf("Cabin Weather: current position lat=%.6f lon=%.6f progress=%.3f.\n",
           data->current_lat,
           data->current_lon,
           data->progress);
    printf("Cabin Weather: city changed from %s/%s to %s/%s, update weather.\n",
           last_city[0] != '\0' ? last_city : "none",
           last_adcode[0] != '\0' ? last_adcode : "none",
           city,
           adcode[0] != '\0' ? adcode : "none");

    if (adcode[0] == '\0')
    {
        printf("Cabin Weather: enroute segment has no city adcode, keep current weather source=%s.\n",
               data->weather_source);
        snprintf(last_city, last_city_size, "%s", city);
        snprintf(last_adcode, last_adcode_size, "%s", adcode);
        return;
    }

    cabin_api_update_weather_for_city(data, city, adcode);

    printf("Cabin Weather: weather source=%s city=%s weather=%s temperature=%.1f humidity=%.1f.\n",
           data->weather_source,
           data->weather_city,
           data->weather,
           data->temperature,
           data->humidity);

    snprintf(last_city, last_city_size, "%s", city);
    snprintf(last_adcode, last_adcode_size, "%s", adcode);
}

static TTF_Font *open_font(int size)
{
    TTF_Font *font = TTF_OpenFont(CABIN_FONT_PATH, size);
    if (font != NULL)
    {
        return font;
    }

    font = TTF_OpenFont("C:/Windows/Fonts/msyh.ttc", size);
    if (font != NULL)
    {
        return font;
    }

    font = TTF_OpenFont("C:/Windows/Fonts/simhei.ttf", size);
    if (font != NULL)
    {
        return font;
    }

    return TTF_OpenFont("C:/Windows/Fonts/arial.ttf", size);
}

static SDL_Texture *load_texture(SDL_Renderer *renderer, const char *path, const char *label)
{
    SDL_Texture *texture = IMG_LoadTexture(renderer, path);
    if (texture == NULL)
    {
        printf("Cabin: failed to load %s (%s): %s\n", label, path, IMG_GetError());
    }
    else
    {
        printf("Cabin: loaded %s from %s.\n", label, path);
    }

    return texture;
}

static SDL_Texture *load_cabin_map_texture(SDL_Renderer *renderer, Cabin_Data *data)
{
    char api_map_path[256];
    SDL_Texture *texture = NULL;

    api_map_path[0] = '\0';
    if (cabin_api_prepare_static_map(data, api_map_path, sizeof(api_map_path)) && api_map_path[0] != '\0')
    {
        texture = load_texture(renderer, api_map_path, "cached/API Beijing-Chengdu map background");
        if (texture != NULL)
        {
            printf("Cabin Map: final map source=%s path=%s.\n", data->map_source, api_map_path);
            return texture;
        }

        printf("Cabin Map: cached/API map failed to load as SDL texture, fallback to local map.\n");
    }

    printf("Cabin Map: using local fallback map for Beijing-Chengdu mock route.\n");
    texture = load_texture(renderer, CABIN_MAP_PATH, "local map background");
    if (texture != NULL)
    {
        snprintf(data->map_source, sizeof(data->map_source), "%s", "LOCAL");
        printf("Cabin Map: final map source=LOCAL path=%s.\n", CABIN_MAP_PATH);
        return texture;
    }

    snprintf(data->map_source, sizeof(data->map_source), "%s", "FALLBACK");
    printf("Cabin Map: final map source=FALLBACK, using drawn map background.\n");
    return NULL;
}

static void destroy_assets(Cabin_Assets *assets)
{
    if (assets == NULL)
    {
        return;
    }

    if (assets->map_texture != NULL)
    {
        SDL_DestroyTexture(assets->map_texture);
    }
    if (assets->plane_texture != NULL)
    {
        SDL_DestroyTexture(assets->plane_texture);
    }
    if (assets->fullscreen_texture != NULL)
    {
        SDL_DestroyTexture(assets->fullscreen_texture);
    }
    if (assets->add_texture != NULL)
    {
        SDL_DestroyTexture(assets->add_texture);
    }
    if (assets->sub_texture != NULL)
    {
        SDL_DestroyTexture(assets->sub_texture);
    }
    if (assets->title_font != NULL)
    {
        TTF_CloseFont(assets->title_font);
    }
    if (assets->emergency_font != NULL)
    {
        TTF_CloseFont(assets->emergency_font);
    }
    if (assets->font != NULL)
    {
        TTF_CloseFont(assets->font);
    }
    if (assets->small_font != NULL)
    {
        TTF_CloseFont(assets->small_font);
    }
}

int cabin_main_run(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Cabin: SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    if (TTF_Init() != 0)
    {
        printf("Cabin: TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return -1;
    }

    const int image_flags = IMG_INIT_PNG;
    if ((IMG_Init(image_flags) & image_flags) != image_flags)
    {
        printf("Cabin: IMG_Init PNG failed: %s\n", IMG_GetError());
    }

    SDL_Window *window = SDL_CreateWindow(
        "Cabin Public Information Display",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        CABIN_WINDOW_WIDTH,
        CABIN_WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN);
    if (window == NULL)
    {
        printf("Cabin: SDL_CreateWindow failed: %s\n", SDL_GetError());
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL)
    {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == NULL)
    {
        printf("Cabin: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    /* Load fonts early so the API key dialog can render text. */
    TTF_Font *title_font = open_font(24);
    TTF_Font *font = open_font(20);
    TTF_Font *small_font = open_font(17);
    if (title_font == NULL || font == NULL || small_font == NULL)
    {
        printf("Cabin: font load failed, text rendering will be skipped where font is missing: %s\n", TTF_GetError());
    }

    Cabin_Data data;
    cabin_data_init(&data);
    Cabin_Crash_Audio crash_audio;
    cabin_crash_audio_init(&crash_audio);
    printf("Cabin Route: using Beijing-Chengdu mock route; FMC route integration disabled for now.\n");
    fflush(stdout);

    Cabin_Assets assets;
    assets.map_texture = load_cabin_map_texture(renderer, &data);
    assets.plane_texture = load_texture(renderer, CABIN_PLANE_PATH, "plane icon");
    assets.fullscreen_texture = load_texture(renderer, CABIN_FULLSCREEN_PATH, "fullscreen control");
    assets.add_texture = load_texture(renderer, CABIN_ADD_PATH, "zoom plus");
    assets.sub_texture = load_texture(renderer, CABIN_SUB_PATH, "zoom minus");
    assets.title_font = title_font;
    assets.font = font;
    assets.small_font = small_font;

    if (cabin_api_has_key())
    {
        printf("Cabin: existing API key found for this run, skip API key dialog.\n");
    }
    else
    {
        cabin_ui_render(renderer, &assets, &data);
        SDL_RenderPresent(renderer);

        Cabin_ApiKeyDialogResult dialog_result;
        cabin_ui_run_apikey_dialog(window, renderer, &assets, &data, title_font, font, small_font, &dialog_result);
        if (dialog_result.confirmed && dialog_result.api_key[0] != '\0')
        {
            cabin_api_set_key(dialog_result.api_key, dialog_result.remember);
            printf("Cabin: API key entered via dialog, remember=%d.\n", dialog_result.remember);
            if (assets.map_texture != NULL)
            {
                SDL_DestroyTexture(assets.map_texture);
                assets.map_texture = NULL;
            }
            assets.map_texture = load_cabin_map_texture(renderer, &data);
        }
        else
        {
            cabin_api_set_key(NULL, 0);
            printf("Cabin: API key dialog cancelled, using mock data.\n");
        }
    }

    char last_weather_city[CABIN_TEXT_LEN] = "";
    char last_weather_adcode[CABIN_TEXT_LEN] = "";
    update_weather_if_city_changed(&data,
                                   last_weather_city,
                                   sizeof(last_weather_city),
                                   last_weather_adcode,
                                   sizeof(last_weather_adcode));

    Cabin_Assets assets;
    assets.map_texture = load_cabin_map_texture(renderer, &data);
    assets.plane_texture = load_texture(renderer, CABIN_PLANE_PATH, "plane icon");
    assets.fullscreen_texture = load_texture(renderer, CABIN_FULLSCREEN_PATH, "fullscreen control");
    assets.add_texture = load_texture(renderer, CABIN_ADD_PATH, "zoom plus");
    assets.sub_texture = load_texture(renderer, CABIN_SUB_PATH, "zoom minus");
    assets.title_font = open_font(24);
    assets.emergency_font = open_font(54);
    assets.font = open_font(20);
    assets.small_font = open_font(17);

    if (assets.title_font == NULL || assets.font == NULL || assets.small_font == NULL)
    {
        printf("Cabin: font load failed, text rendering will be skipped where font is missing: %s\n", TTF_GetError());
    }

    int running = 1;
    SDL_Event event;
    Uint32 last_ticks = SDL_GetTicks();

    while (running)
    {
        const Uint32 frame_start = SDL_GetTicks();

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = 0;
            }
            else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            {
                running = 0;
            }
            else if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_y)
            {
                if (!data.crash_demo_active)
                {
                    data.crash_demo_active = 1;
                    data.crash_demo_started_ticks = SDL_GetTicks();
                    printf("Cabin CRASH DEMO: triggered by Y.\n");
                    fflush(stdout);
                }
            }
            else if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_r)
            {
                if (data.crash_demo_active)
                {
                    data.crash_demo_active = 0;
                    data.crash_demo_started_ticks = 0;
                    cabin_crash_audio_update(&crash_audio, 0);
                    printf("Cabin CRASH DEMO: reset by R.\n");
                    fflush(stdout);
                }
            }
            else
            {
                cabin_ui_handle_event(window, &event);
            }
        }

        const Uint32 now = SDL_GetTicks();
        float delta_time = (float)(now - last_ticks) / 1000.0f;
        last_ticks = now;
        cabin_data_update_mock(&data, delta_time);
        update_weather_if_city_changed(&data,
                                       last_weather_city,
                                       sizeof(last_weather_city),
                                       last_weather_adcode,
                                       sizeof(last_weather_adcode));
        cabin_crash_audio_update(&crash_audio, data.crash_demo_active);

        SDL_SetRenderDrawColor(renderer, 45, 72, 96, 255);
        SDL_RenderClear(renderer);
        cabin_ui_render(renderer, &assets, &data);
        SDL_RenderPresent(renderer);

        const Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < CABIN_TARGET_FRAME_MS)
        {
            SDL_Delay(CABIN_TARGET_FRAME_MS - frame_time);
        }
    }

    cabin_crash_audio_destroy(&crash_audio);
    destroy_assets(&assets);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
