//页面开发主流程
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "fmc_ui_adapter.h"
#include "fmc_display.h"
#include "fmc_event.h"
#include "fmc_connect.h"
#include "../Util/SDL_Util.h"

#define LOGIC_WIDTH 638
#define LOGIC_HEIGHT 998 // FMC 是竖长形的!
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

static void close_shared_fonts(void)
{
    if (font8 != NULL)
    {
        TTF_CloseFont(font8);
        font8 = NULL;
    }
    if (font12 != NULL)
    {
        TTF_CloseFont(font12);
        font12 = NULL;
    }
    if (font16 != NULL)
    {
        TTF_CloseFont(font16);
        font16 = NULL;
    }
    if (font20 != NULL)
    {
        TTF_CloseFont(font20);
        font20 = NULL;
    }
    if (font24 != NULL)
    {
        TTF_CloseFont(font24);
        font24 = NULL;
    }
}

static void cleanup_initialized_sdl(SDL_Renderer *renderer, SDL_Window *window)
{
    close_shared_fonts();
    TTF_Quit();
    IMG_Quit();
    if (renderer != NULL)
    {
        SDL_DestroyRenderer(renderer);
    }
    if (window != NULL)
    {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
}

int fmc_main_run(void)
{
    if (initSDL() != 0)
    {
        return -1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "FMC",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        LOGIC_WIDTH,
        LOGIC_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window == NULL)
    {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        cleanup_initialized_sdl(NULL, NULL);
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
        cleanup_initialized_sdl(NULL, window);
        return -1;
    }

    TTF_Font *font = open_fmc_font();
    if (font == NULL)
    {
        printf("TTF_OpenFont failed: %s\n", TTF_GetError());
        destroySDL(renderer, window);
        return -1;
    }

    const char *image_path = "assets/fmc.png";
    SDL_Texture *texture = IMG_LoadTexture(renderer, image_path);
    if (!texture)
    {
        printf("图片加载失败: %s\n", IMG_GetError());
    }

    FMC_Display_Assets ui_assets = {texture};

    FMC_Data data;
    fmc_data_init(&data);

    FMC_Event_State ui_state;
    fmc_event_state_init(&ui_state);

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
            else if (event.type == SDL_MOUSEMOTION)
            {
                fmc_event_update_hover(renderer, &ui_state, event.motion.x, event.motion.y);
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
            {
                fmc_event_handle_mouse_button(renderer, &ui_state, &data, event.button.x, event.button.y);
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
                    fmc_xplane_send_command("sim/FMS/clear");
                }
                else if ((event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) &&
                         data.current_page == FMC_PAGE_ROUTE)
                {
                    fmc_data_exec_route_selection(&data);
                }
                else if (event.key.keysym.sym == SDLK_F1)
                {
                    fmc_data_set_page(&data, FMC_PAGE_INDEX);
                    fmc_xplane_send_command("sim/FMS/init");
                }
                else if (event.key.keysym.sym == SDLK_F2)
                {
                    fmc_data_set_page(&data, FMC_PAGE_ROUTE);
                    fmc_xplane_send_command("sim/FMS/fpln");
                }
                else if (event.key.keysym.sym == SDLK_F3)
                {
                    fmc_data_set_page(&data, FMC_PAGE_DEP_ARR);
                    fmc_xplane_send_command("sim/FMS/dep_arr");
                }
                else if (event.key.keysym.sym == SDLK_F4)
                {
                    fmc_data_set_page(&data, FMC_PAGE_CLIMB);
                    fmc_xplane_send_command("sim/FMS/clb");
                }
                else if (event.key.keysym.sym == SDLK_F5)
                {
                    fmc_data_set_page(&data, FMC_PAGE_CRUISE);
                    fmc_xplane_send_command("sim/FMS/crz");
                }
                else if (event.key.keysym.sym == SDLK_F6)
                {
                    fmc_data_set_page(&data, FMC_PAGE_DESCENT);
                    fmc_xplane_send_command("sim/FMS/des");
                }
                else if (event.key.keysym.sym == SDLK_F7)
                {
                    fmc_data_set_page(&data, FMC_PAGE_LEGS);
                    fmc_xplane_send_command("sim/FMS/legs");
                }
                else if (event.key.keysym.sym == SDLK_F8)
                {
                    fmc_data_set_page(&data, FMC_PAGE_STATUS);
                    fmc_xplane_send_command("sim/FMS/prog");
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
        fmc_display_render(renderer, font, &ui_assets, &ui_state, &data);
        SDL_RenderPresent(renderer);

        const Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FMC_TARGET_FRAME_MS)
        {
            SDL_Delay(FMC_TARGET_FRAME_MS - frame_time);
        }
    }

    SDL_StopTextInput();
    fmc_data_destroy(&data);
    fmc_display_assets_destroy(&ui_assets);
    TTF_CloseFont(font);
    destroySDL(renderer, window);

    return 0;
}
