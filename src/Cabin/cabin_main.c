#include "cabin_main.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cabin_api.h"
#include "cabin_data.h"
#include "cabin_ui.h"
#include "../Data/sim_data_center.h"
#include "../Util/xplane_live_data.h"

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

#if 0 /* Audio effects are outside the current Cabin data integration scope. */
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

#endif

typedef enum Cabin_Place_Target
{
    CABIN_PLACE_TARGET_CURRENT = 0,
    CABIN_PLACE_TARGET_ORIGIN,
    CABIN_PLACE_TARGET_DESTINATION
} Cabin_Place_Target;

typedef struct Cabin_Place_Request
{
    Cabin_Place_Target target;
    double latitude;
    double longitude;
    int latitude_grid;
    int longitude_grid;
    int route_revision;
    char data_source[CABIN_TEXT_LEN];
    char endpoint_ident[CABIN_TEXT_LEN];
} Cabin_Place_Request;

typedef struct Cabin_Place_Resolver
{
    SDL_Thread *thread;
    SDL_atomic_t complete;
    int success;
    Cabin_Place_Request request;
    Cabin_Place result;
} Cabin_Place_Resolver;

#define CABIN_PLACE_GRID_SCALE 10.0
#define CABIN_PLACE_RETRY_SECONDS 30.0f

static int cabin_place_grid(double value)
{
    return (int)floor(value * CABIN_PLACE_GRID_SCALE);
}

static int cabin_place_coordinates_valid(double latitude, double longitude)
{
    return isfinite(latitude) && isfinite(longitude) &&
           latitude >= -90.0 && latitude <= 90.0 && longitude >= -180.0 && longitude <= 180.0;
}

static Cabin_Place *cabin_place_target(Cabin_Data *data, Cabin_Place_Target target)
{
    if (data == NULL)
    {
        return NULL;
    }
    if (target == CABIN_PLACE_TARGET_ORIGIN)
    {
        return &data->origin_place;
    }
    if (target == CABIN_PLACE_TARGET_DESTINATION)
    {
        return &data->destination_place;
    }
    return &data->current_place;
}

static int cabin_place_worker(void *user_data)
{
    Cabin_Place_Resolver *resolver = (Cabin_Place_Resolver *)user_data;
    if (resolver == NULL)
    {
        return 0;
    }

    memset(&resolver->result, 0, sizeof(resolver->result));
    resolver->result.latitude = resolver->request.latitude;
    resolver->result.longitude = resolver->request.longitude;
    resolver->result.latitude_grid = resolver->request.latitude_grid;
    resolver->result.longitude_grid = resolver->request.longitude_grid;
    resolver->result.route_revision = resolver->request.route_revision;
    snprintf(resolver->result.source, sizeof(resolver->result.source), "%s", "AMAP");
    snprintf(resolver->result.snapshot_source, sizeof(resolver->result.snapshot_source), "%s", resolver->request.data_source);
    resolver->success = cabin_api_reverse_geocode(resolver->request.latitude,
                                                   resolver->request.longitude,
                                                   resolver->result.province,
                                                   sizeof(resolver->result.province),
                                                   resolver->result.city,
                                                   sizeof(resolver->result.city),
                                                   resolver->result.district,
                                                   sizeof(resolver->result.district));
    resolver->result.status = resolver->success ? CABIN_PLACE_VALID : CABIN_PLACE_FAILED;
    if (resolver->request.target != CABIN_PLACE_TARGET_CURRENT)
    {
        printf("Cabin Place: endpoint code=%s lat=%.6f lon=%.6f result=%s.\n",
               resolver->request.endpoint_ident,
               resolver->request.latitude,
               resolver->request.longitude,
               resolver->success ? "VALID" : "FAILED");
    }
    SDL_AtomicSet(&resolver->complete, 1);
    return 0;
}

static int cabin_place_request_matches(const Cabin_Data *data, const Cabin_Place_Request *request)
{
    double latitude = 0.0;
    double longitude = 0.0;

    if (data == NULL || request == NULL)
    {
        return 0;
    }
    if (request->target == CABIN_PLACE_TARGET_CURRENT)
    {
        latitude = data->latitude;
        longitude = data->longitude;
        return data->snapshot_valid &&
               request->latitude_grid == cabin_place_grid(latitude) &&
               request->longitude_grid == cabin_place_grid(longitude) &&
               strcmp(request->data_source, data->data_source) == 0;
    }

    latitude = request->target == CABIN_PLACE_TARGET_ORIGIN ? data->origin_lat : data->destination_lat;
    longitude = request->target == CABIN_PLACE_TARGET_ORIGIN ? data->origin_lon : data->destination_lon;
    return data->route_valid && data->route_revision == request->route_revision &&
           strcmp(request->endpoint_ident,
                  request->target == CABIN_PLACE_TARGET_ORIGIN ? data->origin_airport : data->destination_airport) == 0 &&
           request->latitude_grid == cabin_place_grid(latitude) &&
           request->longitude_grid == cabin_place_grid(longitude);
}

static void cabin_place_apply_completed(Cabin_Place_Resolver *resolver, Cabin_Data *data)
{
    Cabin_Place *target;

    if (resolver == NULL || data == NULL || resolver->thread == NULL || SDL_AtomicGet(&resolver->complete) == 0)
    {
        return;
    }

    SDL_WaitThread(resolver->thread, NULL);
    resolver->thread = NULL;
    target = cabin_place_target(data, resolver->request.target);
    if (target != NULL && cabin_place_request_matches(data, &resolver->request))
    {
        *target = resolver->result;
        target->next_retry_sim_time = resolver->success ? 0.0f : data->snapshot_time + CABIN_PLACE_RETRY_SECONDS;
        printf("Cabin Place: target=%d status=%s lat=%.6f lon=%.6f route_rev=%d city=%s.\n",
               resolver->request.target, resolver->success ? "VALID" : "FAILED",
               resolver->request.latitude, resolver->request.longitude, resolver->request.route_revision,
               resolver->success ? target->city : "----");
    }
    else
    {
        printf("Cabin Place: discarded stale target=%d route_rev=%d.\n",
               resolver->request.target, resolver->request.route_revision);
        if (target != NULL && target->status == CABIN_PLACE_PENDING)
        {
            memset(target, 0, sizeof(*target));
        }
    }
}

static int cabin_place_should_request(const Cabin_Place *place, double latitude, double longitude, int route_revision, float sim_time, const char *source)
{
    const int latitude_grid = cabin_place_grid(latitude);
    const int longitude_grid = cabin_place_grid(longitude);

    if (place == NULL || !cabin_place_coordinates_valid(latitude, longitude) || place->status == CABIN_PLACE_PENDING)
    {
        return 0;
    }
    if (place->status == CABIN_PLACE_VALID && place->latitude_grid == latitude_grid &&
        place->longitude_grid == longitude_grid && place->route_revision == route_revision &&
        (source == NULL || strcmp(place->snapshot_source, source) == 0))
    {
        return 0;
    }
    return place->status != CABIN_PLACE_FAILED || sim_time >= place->next_retry_sim_time;
}

static void cabin_place_schedule(Cabin_Place_Resolver *resolver, Cabin_Data *data)
{
    Cabin_Place_Target targets[] = {CABIN_PLACE_TARGET_ORIGIN, CABIN_PLACE_TARGET_DESTINATION, CABIN_PLACE_TARGET_CURRENT};

    if (resolver == NULL || data == NULL || resolver->thread != NULL)
    {
        return;
    }

    for (int i = 0; i < (int)(sizeof(targets) / sizeof(targets[0])); ++i)
    {
        const Cabin_Place_Target type = targets[i];
        Cabin_Place *place = cabin_place_target(data, type);
        const double latitude = type == CABIN_PLACE_TARGET_ORIGIN ? data->origin_lat :
                                (type == CABIN_PLACE_TARGET_DESTINATION ? data->destination_lat : data->latitude);
        const double longitude = type == CABIN_PLACE_TARGET_ORIGIN ? data->origin_lon :
                                 (type == CABIN_PLACE_TARGET_DESTINATION ? data->destination_lon : data->longitude);
        const int route_revision = type == CABIN_PLACE_TARGET_CURRENT ? -1 : data->route_revision;
        const char *source = type == CABIN_PLACE_TARGET_CURRENT ? data->data_source : "ROUTE";
        const int available = type == CABIN_PLACE_TARGET_CURRENT ? data->snapshot_valid : data->route_valid;

        if (!available || !cabin_place_should_request(place, latitude, longitude, route_revision, data->snapshot_time, source))
        {
            continue;
        }

        memset(&resolver->request, 0, sizeof(resolver->request));
        resolver->request.target = type;
        resolver->request.latitude = latitude;
        resolver->request.longitude = longitude;
        resolver->request.latitude_grid = cabin_place_grid(latitude);
        resolver->request.longitude_grid = cabin_place_grid(longitude);
        resolver->request.route_revision = route_revision;
        snprintf(resolver->request.data_source, sizeof(resolver->request.data_source), "%s", source);
        if (type == CABIN_PLACE_TARGET_ORIGIN)
        {
            snprintf(resolver->request.endpoint_ident, sizeof(resolver->request.endpoint_ident), "%s", data->origin_airport);
        }
        else if (type == CABIN_PLACE_TARGET_DESTINATION)
        {
            snprintf(resolver->request.endpoint_ident, sizeof(resolver->request.endpoint_ident), "%s", data->destination_airport);
        }
        place->status = CABIN_PLACE_PENDING;
        place->latitude = latitude;
        place->longitude = longitude;
        place->latitude_grid = resolver->request.latitude_grid;
        place->longitude_grid = resolver->request.longitude_grid;
        place->route_revision = route_revision;
        SDL_AtomicSet(&resolver->complete, 0);
        resolver->thread = SDL_CreateThread(cabin_place_worker, "cabin-place", resolver);
        if (resolver->thread == NULL)
        {
            place->status = CABIN_PLACE_FAILED;
            place->next_retry_sim_time = data->snapshot_time + CABIN_PLACE_RETRY_SECONDS;
            printf("Cabin Place: failed to start resolver thread.\n");
        }
        return;
    }
}

static void cabin_place_apply_labels(Cabin_Data *data)
{
    if (data == NULL)
    {
        return;
    }
    if (data->origin_place.status == CABIN_PLACE_VALID)
    {
        snprintf(data->origin_city, sizeof(data->origin_city), "%s", cabin_place_display_name(&data->origin_place));
    }
    if (data->destination_place.status == CABIN_PLACE_VALID)
    {
        snprintf(data->destination_city, sizeof(data->destination_city), "%s", cabin_place_display_name(&data->destination_place));
    }
    if (data->current_place.status == CABIN_PLACE_VALID)
    {
        snprintf(data->current_city, sizeof(data->current_city), "%s", cabin_place_display_name(&data->current_place));
        snprintf(data->current_district, sizeof(data->current_district), "%s", data->current_place.district);
        snprintf(data->current_town, sizeof(data->current_town), "%s", data->current_place.province);
    }
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

    if (!data->route_valid)
    {
        snprintf(data->map_source, sizeof(data->map_source), "%s", "FALLBACK");
        printf("Cabin Map: no route-specific cache/API map for current position; using drawn map background.\n");
        return NULL;
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

typedef enum Cabin_Run_Mode
{
    CABIN_RUN_STANDALONE = 0,
    CABIN_RUN_SHARED_SIM_CENTER,
    CABIN_RUN_SHARED_RUNTIME
} Cabin_Run_Mode;

static int cabin_main_run_internal(SimDataCenter *sim_data_center, XPlaneSharedRuntime *runtime, Cabin_Run_Mode run_mode)
{
    const int shared_mode = run_mode != CABIN_RUN_STANDALONE;

    if (runtime != NULL)
    {
        sim_data_center = xplane_shared_runtime_data_center(runtime);
    }

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
    TTF_Font *emergency_font = open_font(54);
    TTF_Font *font = open_font(20);
    TTF_Font *small_font = open_font(17);
    if (title_font == NULL || font == NULL || small_font == NULL)
    {
        printf("Cabin: font load failed, text rendering will be skipped where font is missing: %s\n", TTF_GetError());
    }

    Cabin_Data data;
    cabin_data_init(&data);
    cabin_data_apply_sim_data_center(&data, sim_data_center, 0.0f);
    Cabin_Place_Resolver place_resolver;
    memset(&place_resolver, 0, sizeof(place_resolver));
    printf("Cabin %s: SimDataCenter=%p planned_route=%p revision=%d origin=%s destination=%s points=%d; active view is the unified updater.\n",
           shared_mode ? "SHARED" : "STANDALONE",
           (void *)sim_data_center,
           sim_data_center != NULL ? (void *)&sim_data_center->planned_route : NULL,
           data.route_revision,
           data.origin_airport[0] != '\0' ? data.origin_airport : "----",
           data.destination_airport[0] != '\0' ? data.destination_airport : "----",
           data.route_point_count);
    fflush(stdout);

    Cabin_Assets assets;
    assets.map_texture = load_cabin_map_texture(renderer, &data);
    assets.plane_texture = load_texture(renderer, CABIN_PLANE_PATH, "plane icon");
    assets.fullscreen_texture = load_texture(renderer, CABIN_FULLSCREEN_PATH, "fullscreen control");
    assets.add_texture = load_texture(renderer, CABIN_ADD_PATH, "zoom plus");
    assets.sub_texture = load_texture(renderer, CABIN_SUB_PATH, "zoom minus");
    assets.title_font = title_font;
    assets.emergency_font = emergency_font;
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

    int running = 1;
    SDL_Event event;
    Uint32 last_ticks = SDL_GetTicks();
    Uint32 last_snapshot_log_ticks = 0;

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
                sim_data_center_set_demo_alert(sim_data_center, ALERT_TYPE_CRASH, 1);
            }
            else if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_r)
            {
                sim_data_center_set_demo_alert(sim_data_center, ALERT_TYPE_CRASH, 0);
            }
            else
            {
                cabin_ui_handle_event(window, &event);
            }
        }

        const Uint32 now = SDL_GetTicks();
        float delta_time = (float)(now - last_ticks) / 1000.0f;
        last_ticks = now;
        if (run_mode == CABIN_RUN_SHARED_RUNTIME)
        {
            xplane_shared_runtime_update(runtime, delta_time);
        }
        else if (sim_data_center != NULL)
        {
            sim_data_center_update(sim_data_center, delta_time);
        }
        const int data_changes = cabin_data_apply_sim_data_center(&data, sim_data_center, delta_time);
        cabin_place_apply_completed(&place_resolver, &data);
        cabin_place_schedule(&place_resolver, &data);
        cabin_place_apply_labels(&data);
        if (last_snapshot_log_ticks == 0 || now - last_snapshot_log_ticks >= 1000u)
        {
            const SimSnapshot *snapshot = sim_data_center_snapshot(sim_data_center);
            last_snapshot_log_ticks = now;
            printf("Cabin Snapshot: SimDataCenter=%p frame=%d updated_frame=%d time=%.2f lat=%.6f lon=%.6f alt=%.0f gs=%.0f vs=%.0f source=%s valid=%d.\n",
                   (void *)sim_data_center,
                   snapshot != NULL ? snapshot->current_frame : -1,
                   snapshot != NULL ? snapshot->updated_frame : -1,
                   snapshot != NULL ? snapshot->sim_time : 0.0f,
                   snapshot != NULL ? snapshot->latitude : 0.0,
                   snapshot != NULL ? snapshot->longitude : 0.0,
                   snapshot != NULL ? snapshot->altitude : 0.0f,
                   snapshot != NULL ? snapshot->ground_speed : 0.0f,
                   snapshot != NULL ? snapshot->vertical_speed : 0.0f,
                   snapshot != NULL ? sim_snapshot_source_name(snapshot->source) : "NONE",
                   snapshot != NULL ? snapshot->data_valid : 0);
        }
        if ((data_changes & CABIN_DATA_UPDATE_ROUTE) != 0)
        {
            if (assets.map_texture != NULL)
            {
                SDL_DestroyTexture(assets.map_texture);
                assets.map_texture = NULL;
            }
            assets.map_texture = load_cabin_map_texture(renderer, &data);
        }
        update_weather_if_city_changed(&data,
                                       last_weather_city,
                                       sizeof(last_weather_city),
                                       last_weather_adcode,
                                       sizeof(last_weather_adcode));
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

    if (place_resolver.thread != NULL)
    {
        SDL_WaitThread(place_resolver.thread, NULL);
        place_resolver.thread = NULL;
    }
    destroy_assets(&assets);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}

int cabin_main_run(void)
{
    SimDataCenter *sim_data_center = (SimDataCenter *)malloc(sizeof(*sim_data_center));
    if (sim_data_center == NULL)
    {
        printf("Cabin: unable to allocate SimDataCenter.\n");
        return -1;
    }

    sim_data_center_init(sim_data_center);
    const int result = cabin_main_run_internal(sim_data_center, NULL, CABIN_RUN_STANDALONE);
    sim_data_center_destroy(sim_data_center);
    free(sim_data_center);
    return result;
}

int cabin_main_run_with_sim_data_center(SimDataCenter *sim_data_center)
{
    if (sim_data_center == NULL || !sim_data_center_is_ready(sim_data_center))
    {
        printf("Cabin SHARED: SimDataCenter unavailable.\n");
        return -1;
    }
    return cabin_main_run_internal(sim_data_center, NULL, CABIN_RUN_SHARED_SIM_CENTER);
}

int cabin_main_run_with_shared_runtime(XPlaneSharedRuntime *runtime)
{
    SimDataCenter *sim_data_center = xplane_shared_runtime_data_center(runtime);

    if (!xplane_shared_runtime_initialized(runtime) ||
        sim_data_center == NULL ||
        !sim_data_center_is_ready(sim_data_center))
    {
        printf("Cabin SHARED: runtime unavailable.\n");
        return -1;
    }
    return cabin_main_run_internal(sim_data_center, runtime, CABIN_RUN_SHARED_RUNTIME);
}
