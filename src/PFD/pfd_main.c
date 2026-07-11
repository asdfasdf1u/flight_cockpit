#include "pfd_main.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "pfd_data.h"
#include "pfd_ui.h"
#include "../Data/sim_data_center.h"

#define PFD_WINDOW_WIDTH 900
#define PFD_WINDOW_HEIGHT 800
#define PFD_TARGET_FRAME_MS 33

static TTF_Font *open_pfd_font(void)
{
    TTF_Font *font = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 20);
    if (font != NULL)
    {
        return font;
    }

    font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 20);
    if (font != NULL)
    {
        return font;
    }

    font = TTF_OpenFont("C:/Windows/Fonts/simhei.ttf", 20);
    return font;
}

static void apply_sim_snapshot_to_pfd(PFD_Data *data, const SimSnapshot *snapshot)
{
    if (data == NULL || snapshot == NULL || !snapshot->has_pfd)
    {
        return;
    }

    data->pitch = snapshot->pitch;
    data->roll = snapshot->roll;
    data->yaw = snapshot->yaw;
    data->altitude = snapshot->altitude;
    data->agl_altitude = snapshot->agl_altitude;
    data->throttle = snapshot->throttle;
    data->airspeed_current = snapshot->airspeed;
    data->airspeed_target = snapshot->airspeed_target;
    data->vertical_speed = snapshot->vertical_speed;
    data->heading = snapshot->heading;
    data->heading_target = snapshot->heading_target;
    data->altitude_target = snapshot->altitude_target;
    data->autopilot_on = 1;
    data->simulation_time = snapshot->sim_time;
    data->using_file_data = 1;
    data->file_sample_index = snapshot->pfd_frame_index;
    snprintf(data->flight_mode, sizeof(data->flight_mode), "%s", "SIM DATA");
}

int pfd_main_run(void)
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
        "PFD - Primary Flight Display",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        PFD_WINDOW_WIDTH,
        PFD_WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN);
    if (window == NULL)
    {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
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
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    TTF_Font *font = open_pfd_font();
    if (font == NULL)
    {
        printf("TTF_OpenFont failed: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    PFDData data;
    pfd_data_init(&data);
    SimDataCenter sim_data_center;
    const int use_sim_data_center = sim_data_center_init(&sim_data_center) &&
                                    sim_data_center_has_pfd_data(&sim_data_center);
    if (use_sim_data_center)
    {
        apply_sim_snapshot_to_pfd(&data, sim_data_center_snapshot(&sim_data_center));
    }
    else
    {
        printf("PFD: SimDataCenter unavailable for PFD, using legacy pfd.dat/mock path.\n");
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
            apply_sim_snapshot_to_pfd(&data, sim_data_center_snapshot(&sim_data_center));
        }
        else
        {
            pfd_data_update_mock(&data, delta_time);
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        pfd_ui_render(renderer, font, &data);
        SDL_RenderPresent(renderer);

        const Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < PFD_TARGET_FRAME_MS)
        {
            SDL_Delay(PFD_TARGET_FRAME_MS - frame_time);
        }
    }

    sim_data_center_destroy(&sim_data_center);
    pfd_ui_clear_text_cache(renderer);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
