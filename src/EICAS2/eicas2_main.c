#include "eicas2_main.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "../Systems/aircraft_systems_data.h"
#include "eicas2_ui.h"

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

        aircraft_systems_data_update_mock(&data, delta_time);

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

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}

