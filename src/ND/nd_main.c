#include "nd_main.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "nd_data.h"
#include "nd_ui.h"
#include "../Data/sim_data_center.h"

#define ND_WINDOW_WIDTH 752
#define ND_WINDOW_HEIGHT 752
#define ND_TARGET_FRAME_MS 16

static void handle_nd_layer_key(ND_Data *data, SDL_Keycode key)
{
    switch (key)
    {
    case SDLK_1:
    case SDLK_KP_1:
        nd_data_toggle_map_layer_visible(data, ND_MAP_LAYER_WPT);
        break;
    case SDLK_2:
    case SDLK_KP_2:
        nd_data_toggle_map_layer_visible(data, ND_MAP_LAYER_ARPT);
        break;
    case SDLK_3:
    case SDLK_KP_3:
        nd_data_toggle_map_layer_visible(data, ND_MAP_LAYER_STA);
        break;
    case SDLK_l:
        nd_data_toggle_map_labels_visible(data);
        break;
    default:
        break;
    }
}

static TTF_Font *open_nd_font(void)
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

static void apply_sim_snapshot_to_nd(ND_Data *data, const SimSnapshot *snapshot)
{
    if (data == NULL || snapshot == NULL || !snapshot->has_nd)
    {
        return;
    }

    data->latitude = snapshot->latitude;
    data->longitude = snapshot->longitude;
    data->heading = snapshot->heading;
    data->track = snapshot->track;
    data->ground_speed = snapshot->ground_speed;
    data->true_air_speed = snapshot->true_air_speed;
    data->simulation_time = snapshot->sim_time;
    data->data_frame_index = snapshot->nd_frame_index;
    data->data_frame_elapsed = snapshot->sim_time;
    nd_data_recalculate_nav_points(data);
}

int nd_main_run(void)
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
        "ND",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        ND_WINDOW_WIDTH,
        ND_WINDOW_HEIGHT,
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

    TTF_Font *font = open_nd_font();
    if (font == NULL)
    {
        printf("TTF_OpenFont failed: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    ND_Data data;
    nd_data_init(&data);
    SimDataCenter sim_data_center;
    const int use_sim_data_center = sim_data_center_init(&sim_data_center) &&
                                    sim_data_center_has_nd_data(&sim_data_center);
    if (use_sim_data_center)
    {
        apply_sim_snapshot_to_nd(&data, sim_data_center_snapshot(&sim_data_center));
    }
    else
    {
        printf("ND: SimDataCenter unavailable for ND, using legacy nd.dat/mock path.\n");
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
            else if (event.type == SDL_KEYDOWN && event.key.repeat == 0)
            {
                handle_nd_layer_key(&data, event.key.keysym.sym);
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
            apply_sim_snapshot_to_nd(&data, sim_data_center_snapshot(&sim_data_center));
        }
        else
        {
            nd_data_update_mock(&data, delta_time);
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        nd_ui_render(renderer, font, &data);
        SDL_RenderPresent(renderer);

        const Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < ND_TARGET_FRAME_MS)
        {
            SDL_Delay(ND_TARGET_FRAME_MS - frame_time);
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
