#include "fmc_main.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "fmc_data.h"
#include "fmc_ui.h"

#define FMC_WINDOW_WIDTH 700
#define FMC_WINDOW_HEIGHT 900
#define FMC_TARGET_FRAME_MS 16

static TTF_Font *open_fmc_font(void)
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

static void handle_text_input(FMC_Data *data, const char *text)
{
    if (data == NULL || text == NULL)
    {
        return;
    }

    for (int i = 0; text[i] != '\0'; ++i)
    {
        fmc_data_append_char(data, text[i]);
    }
}

int fmc_main_run(void)
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
        "FMC - Flight Management Computer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        FMC_WINDOW_WIDTH,
        FMC_WINDOW_HEIGHT,
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

    TTF_Font *font = open_fmc_font();
    if (font == NULL)
    {
        printf("TTF_OpenFont failed: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    FMC_Data data;
    fmc_data_init(&data);

    SDL_StartTextInput();

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
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
            {
                int hit = 0;
                FMC_Page page = fmc_ui_hit_test_page_button(event.button.x, event.button.y, &hit);
                if (hit)
                {
                    fmc_data_set_page(&data, page);
                }
                else if (fmc_ui_hit_test_clear_button(event.button.x, event.button.y))
                {
                    fmc_data_clear_scratchpad(&data);
                }
            }
            else if (event.type == SDL_TEXTINPUT)
            {
                handle_text_input(&data, event.text.text);
            }
            else if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    running = 0;
                }
                else if (event.key.keysym.sym == SDLK_BACKSPACE)
                {
                    fmc_data_backspace(&data);
                }
                else if (event.key.keysym.sym == SDLK_F1)
                {
                    fmc_data_set_page(&data, FMC_PAGE_INDEX);
                }
                else if (event.key.keysym.sym == SDLK_F2)
                {
                    fmc_data_set_page(&data, FMC_PAGE_ROUTE);
                }
                else if (event.key.keysym.sym == SDLK_F3)
                {
                    fmc_data_set_page(&data, FMC_PAGE_DEP_ARR);
                }
                else if (event.key.keysym.sym == SDLK_F4)
                {
                    fmc_data_set_page(&data, FMC_PAGE_PERF);
                }
                else if (event.key.keysym.sym == SDLK_F5)
                {
                    fmc_data_set_page(&data, FMC_PAGE_LEGS);
                }
            }
        }

        const Uint32 current_ticks = SDL_GetTicks();
        float delta_time = (float)(current_ticks - last_ticks) / 1000.0f;
        last_ticks = current_ticks;
        if (delta_time > 0.1f)
        {
            delta_time = 0.1f;
        }

        fmc_data_update_mock(&data, delta_time);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        fmc_ui_render(renderer, font, &data);
        SDL_RenderPresent(renderer);

        const Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FMC_TARGET_FRAME_MS)
        {
            SDL_Delay(FMC_TARGET_FRAME_MS - frame_time);
        }
    }

    SDL_StopTextInput();
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
