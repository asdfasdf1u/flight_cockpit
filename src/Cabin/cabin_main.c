#include "cabin_main.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "cabin_api.h"
#include "cabin_data.h"
#include "cabin_ui.h"
#include "../Data/sim_data_center.h"
#include "../FMC/fmc_data.h"

#define CABIN_WINDOW_WIDTH 1600
#define CABIN_WINDOW_HEIGHT 900
#define CABIN_TARGET_FRAME_MS 16

#define CABIN_MAP_PATH "assets/20260303110928.png"
#define CABIN_PLANE_PATH "assets/plane.png"
#define CABIN_FULLSCREEN_PATH "assets/full_screen.png"
#define CABIN_ADD_PATH "assets/add.png"
#define CABIN_SUB_PATH "assets/sub.png"
#define CABIN_FONT_PATH "assets/ALIBABAPUHUITI-2-45-LIGHT.TTF"

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

    if (strcmp(data->current_city, "北京") == 0 || strcmp(data->current_city, "北京市") == 0)
    {
        snprintf(city, city_size, "%s", "北京");
        snprintf(adcode, adcode_size, "%s", "110000");
    }
    else if (strcmp(data->current_city, "成都") == 0 || strcmp(data->current_city, "成都市") == 0)
    {
        snprintf(city, city_size, "%s", "成都");
        snprintf(adcode, adcode_size, "%s", "510100");
    }
    else
    {
        snprintf(city, city_size, "%s", "飞行途中");
        snprintf(adcode, adcode_size, "%s", "");
    }
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
        texture = load_texture(renderer, api_map_path, "cached/API route map background");
        if (texture != NULL)
        {
            printf("Cabin Map: final map source=%s path=%s.\n", data->map_source, api_map_path);
            return texture;
        }

        printf("Cabin Map: cached/API map failed to load as SDL texture.\n");
    }

    if (data != NULL && data->planned_route_from_fmc)
    {
        snprintf(data->map_source, sizeof(data->map_source), "%s", "FALLBACK");
        printf("Cabin Map: FMC route has no usable API/cache map; keep route bounds and use drawn fallback background instead of mismatched local Beijing-Chengdu map.\n");
        return NULL;
    }

    printf("Cabin Map: using local fallback map for mock Beijing-Chengdu route.\n");
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

static void print_fmc_route_summary(const SimPlannedRoute *route)
{
    if (route == NULL)
    {
        return;
    }

    printf("Cabin FMC Route: %s -> %s, points=%d, coordinates=%s.\n",
           route->origin[0] != '\0' ? route->origin : "----",
           route->destination[0] != '\0' ? route->destination : "----",
           route->point_count,
           route->has_coordinates ? "yes" : "partial/missing");
    printf("Cabin FMC Route: sequence=");
    for (int i = 0; i < route->point_count; ++i)
    {
        printf("%s%s", i == 0 ? "" : " -> ", route->points[i].ident);
    }
    printf("\n");
    fflush(stdout);
}

static int load_fmc_planned_route_for_cabin(Cabin_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    FMC_Data fmc_data;
    SimPlannedRoute route;
    fmc_data_init(&fmc_data);
    const int exported = fmc_data_export_planned_route(&fmc_data, &route);
    if (exported)
    {
        print_fmc_route_summary(&route);
    }

    const int applied = exported && cabin_data_apply_planned_route(data, &route);
    if (!applied)
    {
        printf("Cabin Route: using existing mock planned_route fallback.\n");
        fflush(stdout);
    }

    fmc_data_destroy(&fmc_data);
    return applied;
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

    Cabin_Data data;
    cabin_data_init(&data);
    SimDataCenter sim_data_center;
    const int use_sim_data_center = sim_data_center_init(&sim_data_center);
    const int route_from_fmc = load_fmc_planned_route_for_cabin(&data);
    if (use_sim_data_center)
    {
        if (route_from_fmc &&
            data.planned_route_count > 0 &&
            !sim_data_center_has_nd_position_data(&sim_data_center))
        {
            sim_data_center_set_position(&sim_data_center, data.origin_lat, data.origin_lon);
            printf("Cabin SimData: nd.dat has no latitude/longitude, seed Cabin DataCenter position from FMC route origin.\n");
            fflush(stdout);
        }
        cabin_data_apply_sim_snapshot(&data, sim_data_center_snapshot(&sim_data_center), 0.0f);
        printf("Cabin SimData: using SimDataCenter for current position, altitude, speed and heading.\n");
        fflush(stdout);
    }
    else
    {
        printf("Cabin SimData: SimDataCenter unavailable, using Cabin mock position fallback.\n");
        fflush(stdout);
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
    cabin_data_print_route_map_summary(&data);
    assets.plane_texture = load_texture(renderer, CABIN_PLANE_PATH, "plane icon");
    assets.fullscreen_texture = load_texture(renderer, CABIN_FULLSCREEN_PATH, "fullscreen control");
    assets.add_texture = load_texture(renderer, CABIN_ADD_PATH, "zoom plus");
    assets.sub_texture = load_texture(renderer, CABIN_SUB_PATH, "zoom minus");
    assets.title_font = open_font(24);
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
            else
            {
                cabin_ui_handle_event(window, &event);
            }
        }

        const Uint32 now = SDL_GetTicks();
        float delta_time = (float)(now - last_ticks) / 1000.0f;
        last_ticks = now;
        if (use_sim_data_center)
        {
            sim_data_center_update(&sim_data_center, delta_time);
            cabin_data_apply_sim_snapshot(&data, sim_data_center_snapshot(&sim_data_center), delta_time);
        }
        else
        {
            cabin_data_update_mock(&data, delta_time);
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

    destroy_assets(&assets);
    sim_data_center_destroy(&sim_data_center);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
