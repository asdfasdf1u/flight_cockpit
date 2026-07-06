#include "cockpit_main.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "cockpit_ui.h"

#include "../PFD/pfd_data.h"
#include "../PFD/pfd_ui.h"

#include "../ND/nd_data.h"
#include "../ND/nd_ui.h"

#include "../EICAS/eicas_data.h"
#include "../EICAS/eicas_ui.h"

#include "../FMC/fmc_data.h"
#include "../FMC/fmc_ui.h"

#define COCKPIT_WINDOW_WIDTH 1400
#define COCKPIT_WINDOW_HEIGHT 900
#define COCKPIT_TARGET_FRAME_MS 16

static TTF_Font *open_cockpit_font(void)
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

static int cockpit_key_to_page(SDL_Keycode key, Cockpit_Page *page)
{
    if (page == NULL)
    {
        return 0;
    }

    if (key == SDLK_0 || key == SDLK_KP_0)
    {
        *page = COCKPIT_PAGE_OVERVIEW;
        return 1;
    }
    if (key == SDLK_1 || key == SDLK_KP_1)
    {
        *page = COCKPIT_PAGE_PFD;
        return 1;
    }
    if (key == SDLK_2 || key == SDLK_KP_2)
    {
        *page = COCKPIT_PAGE_ND;
        return 1;
    }
    if (key == SDLK_3 || key == SDLK_KP_3)
    {
        *page = COCKPIT_PAGE_EICAS;
        return 1;
    }
    if (key == SDLK_4 || key == SDLK_KP_4)
    {
        *page = COCKPIT_PAGE_FMC;
        return 1;
    }

    return 0;
}

static void handle_fmc_text_input(FMC_Data *data, const char *text)
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

static void handle_fmc_keydown(FMC_Data *data, SDL_Keycode key)
{
    if (data == NULL)
    {
        return;
    }

    if (key == SDLK_BACKSPACE)
    {
        fmc_data_backspace(data);
    }
    else if (key == SDLK_F1)
    {
        fmc_data_set_page(data, FMC_PAGE_INDEX);
    }
    else if (key == SDLK_F2)
    {
        fmc_data_set_page(data, FMC_PAGE_ROUTE);
    }
    else if (key == SDLK_F3)
    {
        fmc_data_set_page(data, FMC_PAGE_DEP_ARR);
    }
    else if (key == SDLK_F4)
    {
        fmc_data_set_page(data, FMC_PAGE_PERF);
    }
    else if (key == SDLK_F5)
    {
        fmc_data_set_page(data, FMC_PAGE_LEGS);
    }
}

static void render_current_page(
    SDL_Renderer *renderer,
    TTF_Font *font,
    Cockpit_Page current_page,
    const PFD_Data *pfd_data,
    const ND_Data *nd_data,
    const EICAS_Data *eicas_data,
    const FMC_Data *fmc_data)
{
    if (current_page == COCKPIT_PAGE_PFD)
    {
        pfd_ui_render(renderer, font, pfd_data);
    }
    else if (current_page == COCKPIT_PAGE_ND)
    {
        nd_ui_render(renderer, font, nd_data);
    }
    else if (current_page == COCKPIT_PAGE_EICAS)
    {
        eicas_ui_render(renderer, font, eicas_data);
    }
    else if (current_page == COCKPIT_PAGE_FMC)
    {
        fmc_ui_render(renderer, font, fmc_data);
    }
    else
    {
        const char *active_waypoint = "--";
        if (nd_data->active_waypoint_index >= 0 && nd_data->active_waypoint_index < nd_data->waypoint_count)
        {
            active_waypoint = nd_data->waypoints[nd_data->active_waypoint_index].name;
        }

        cockpit_ui_render_overview(
            renderer,
            font,
            pfd_data->airspeed,
            pfd_data->altitude,
            pfd_data->heading,
            eicas_data->fuel_quantity,
            active_waypoint);
    }
}

int cockpit_main_run(void)
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
        "Cockpit - Integrated Flight Deck",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        COCKPIT_WINDOW_WIDTH,
        COCKPIT_WINDOW_HEIGHT,
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

    TTF_Font *font = open_cockpit_font();
    if (font == NULL)
    {
        printf("TTF_OpenFont failed: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    PFD_Data pfd_data;
    ND_Data nd_data;
    EICAS_Data eicas_data;
    FMC_Data fmc_data;
    pfd_data_init(&pfd_data);
    nd_data_init(&nd_data);
    eicas_data_init(&eicas_data);
    fmc_data_init(&fmc_data);

    Cockpit_Page current_page = COCKPIT_PAGE_OVERVIEW;
    int suppress_next_text_input = 0;

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
                if (current_page == COCKPIT_PAGE_FMC)
                {
                    int hit = 0;
                    FMC_Page page = fmc_ui_hit_test_page_button(event.button.x, event.button.y, &hit);
                    if (hit)
                    {
                        fmc_data_set_page(&fmc_data, page);
                    }
                    else if (fmc_ui_hit_test_clear_button(event.button.x, event.button.y))
                    {
                        fmc_data_clear_scratchpad(&fmc_data);
                    }
                }
            }
            else if (event.type == SDL_TEXTINPUT)
            {
                if (suppress_next_text_input)
                {
                    suppress_next_text_input = 0;
                }
                else if (current_page == COCKPIT_PAGE_FMC)
                {
                    handle_fmc_text_input(&fmc_data, event.text.text);
                }
            }
            else if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    running = 0;
                }
                else
                {
                    Cockpit_Page requested_page = current_page;
                    if (cockpit_key_to_page(event.key.keysym.sym, &requested_page))
                    {
                        current_page = requested_page;
                        suppress_next_text_input = 1;
                    }
                    else if (current_page == COCKPIT_PAGE_FMC)
                    {
                        handle_fmc_keydown(&fmc_data, event.key.keysym.sym);
                    }
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

        pfd_data_update_mock(&pfd_data, delta_time);
        nd_data_update_mock(&nd_data, delta_time);
        eicas_data_update_mock(&eicas_data, delta_time);
        fmc_data_update_mock(&fmc_data, delta_time);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        render_current_page(renderer, font, current_page, &pfd_data, &nd_data, &eicas_data, &fmc_data);
        SDL_RenderPresent(renderer);

        const Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < COCKPIT_TARGET_FRAME_MS)
        {
            SDL_Delay(COCKPIT_TARGET_FRAME_MS - frame_time);
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
