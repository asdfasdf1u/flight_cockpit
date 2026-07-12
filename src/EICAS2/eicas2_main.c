#include "eicas2_main.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "eicas_data.h"
#include "../Systems/aircraft_systems_data.h"
#include "eicas2_ui.h"
#include "../Data/sim_data_center.h"

#define EICAS2_WINDOW_WIDTH 768
#define EICAS2_WINDOW_HEIGHT 768
#define EICAS2_WINDOW_MIN_WIDTH 384
#define EICAS2_WINDOW_MIN_HEIGHT 384
#define EICAS2_TARGET_FRAME_MS 16

static TTF_Font *open_eicas2_font(void)
{
    TTF_Font *font = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 18);
    if (font != NULL)
    {
        return font;
    }

    font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 18);
    if (font != NULL)
    {
        return font;
    }

    return TTF_OpenFont("C:/Windows/Fonts/simhei.ttf", 18);
}

static AircraftSystems_WarningLevel sim_warning_level_to_aircraft(SimWarningLevel level)
{
    switch (level)
    {
    case SIM_WARNING_WARNING:
        return AIRCRAFT_SYSTEMS_WARNING_WARNING;
    case SIM_WARNING_CAUTION:
        return AIRCRAFT_SYSTEMS_WARNING_CAUTION;
    case SIM_WARNING_INFO:
    default:
        return AIRCRAFT_SYSTEMS_WARNING_INFO;
    }
}

static void apply_sim_snapshot_to_aircraft_systems(AircraftSystems_Data *data, const SimSnapshot *snapshot)
{
    if (data == NULL || snapshot == NULL || !snapshot->has_eicas_lower)
    {
        return;
    }

    data->engine_left.n1 = snapshot->n1_left;
    data->engine_left.n2 = snapshot->n2_left;
    data->engine_left.egt = snapshot->egt_left;
    data->engine_left.fuel_flow = snapshot->lower_fuel_flow_left;
    data->engine_left.oil_pressure = snapshot->oil_pressure_left;
    data->engine_left.oil_temp = snapshot->oil_temperature_left;
    data->engine_left.oil_quantity = snapshot->oil_quantity_left;
    data->engine_left.vibration = snapshot->vibration_left;
    data->engine_left.running = snapshot->n1_left > 20.0f || snapshot->n2_left > 20.0f;

    data->engine_right.n1 = snapshot->n1_right;
    data->engine_right.n2 = snapshot->n2_right;
    data->engine_right.egt = snapshot->egt_right;
    data->engine_right.fuel_flow = snapshot->lower_fuel_flow_right;
    data->engine_right.oil_pressure = snapshot->oil_pressure_right;
    data->engine_right.oil_temp = snapshot->oil_temperature_right;
    data->engine_right.oil_quantity = snapshot->oil_quantity_right;
    data->engine_right.vibration = snapshot->vibration_right;
    data->engine_right.running = snapshot->n1_right > 20.0f || snapshot->n2_right > 20.0f;

    data->total_air_temperature = snapshot->total_air_temperature;
    data->fuel_quantity = snapshot->fuel_quantity;
    data->hydraulic_pressure = snapshot->hydraulic_pressure;
    data->cabin_pressure = snapshot->cabin_pressure;
    data->battery_voltage = snapshot->battery_voltage;
    data->gear_down = snapshot->gear_down;
    data->flaps_level = snapshot->flaps_level;
    data->parking_brake_on = snapshot->parking_brake_on;
    data->simulation_time = snapshot->sim_time;

    data->warning_count = 0;
    for (int i = 0; i < snapshot->warning_count && i < AIRCRAFT_SYSTEMS_MAX_WARNINGS; ++i)
    {
        snprintf(data->warnings[i].text, sizeof(data->warnings[i].text), "%s", snapshot->warnings[i].text);
        data->warnings[i].level = sim_warning_level_to_aircraft(snapshot->warnings[i].level);
        data->warnings[i].active = snapshot->warnings[i].active;
        ++data->warning_count;
    }
}

int eicas2_main_run(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    if (TTF_Init() != 0)
    {
        printf("TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "EICAS2",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        EICAS2_WINDOW_WIDTH,
        EICAS2_WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window == NULL)
    {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return -1;
    }
    SDL_SetWindowMinimumSize(window, EICAS2_WINDOW_MIN_WIDTH, EICAS2_WINDOW_MIN_HEIGHT);

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
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    TTF_Font *font = open_eicas2_font();
    if (font == NULL)
    {
        printf("TTF_OpenFont failed: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    AircraftSystems_Data data;
    aircraft_systems_data_init(&data);

    EICAS_Data eicas_data;
    eicas_data_init(&eicas_data);
    const int eicas1_data_loaded = eicas_data_load_upper_file(&eicas_data, "assets/eicas1.dat");
    const int eicas2_data_loaded = eicas_data_load_lower_file(&eicas_data, "assets/eicas2.dat");
    (void)eicas1_data_loaded;
    if (eicas2_data_loaded)
    {
        eicas_data_apply_lower_to_aircraft_systems(&eicas_data, &data);
    }
    else
    {
        printf("EICAS2: using mock fallback data for Lower display.\n");
        fflush(stdout);
    }
    SimDataCenter sim_data_center;
    const int use_sim_data_center = sim_data_center_init(&sim_data_center) &&
                                    sim_data_center_has_eicas_lower_data(&sim_data_center);
    if (use_sim_data_center)
    {
        apply_sim_snapshot_to_aircraft_systems(&data, sim_data_center_snapshot(&sim_data_center));
    }
    else
    {
        printf("EICAS2: SimDataCenter unavailable for Lower display, using legacy eicas2.dat/mock path.\n");
        fflush(stdout);
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
        }

        const Uint32 current_ticks = SDL_GetTicks();
        float delta_time = (float)(current_ticks - last_ticks) / 1000.0f;
        last_ticks = current_ticks;
        if (delta_time > 0.1f)
        {
            delta_time = 0.1f;
        }

        if (use_sim_data_center)
        {
            sim_data_center_update(&sim_data_center, delta_time);
            apply_sim_snapshot_to_aircraft_systems(&data, sim_data_center_snapshot(&sim_data_center));
        }
        else if (eicas2_data_loaded)
        {
            eicas_data_update(&eicas_data, delta_time);
            eicas_data_apply_lower_to_aircraft_systems(&eicas_data, &data);
        }
        else
        {
            aircraft_systems_data_update_mock(&data, delta_time);
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        eicas2_ui_render(renderer, font, &data);
        SDL_RenderPresent(renderer);

        const Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < EICAS2_TARGET_FRAME_MS)
        {
            SDL_Delay(EICAS2_TARGET_FRAME_MS - frame_time);
        }
    }

    sim_data_center_destroy(&sim_data_center);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}

