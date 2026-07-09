#include "cabin_main.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "cabin_api.h"
#include "cabin_data.h"
#include "cabin_ui.h"

#define CABIN_WINDOW_WIDTH 1600
#define CABIN_WINDOW_HEIGHT 900
#define CABIN_TARGET_FRAME_MS 16

#define CABIN_MAP_PATH "assets/20260303110928.png"
#define CABIN_PLANE_PATH "assets/plane.png"
#define CABIN_ADD_PATH "assets/add.png"
#define CABIN_SUB_PATH "assets/sub.png"
#define CABIN_FONT_PATH "assets/ALIBABAPUHUITI-2-45-LIGHT.TTF"

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
    SDL_Texture *texture = NULL;

    printf("Cabin Map: static map API disabled for this build, using local map background.\n");

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
    cabin_api_update_weather(&data);

    Cabin_Assets assets;
    assets.map_texture = load_cabin_map_texture(renderer, &data);
    assets.plane_texture = load_texture(renderer, CABIN_PLANE_PATH, "plane icon");
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
        }

        const Uint32 now = SDL_GetTicks();
        float delta_time = (float)(now - last_ticks) / 1000.0f;
        last_ticks = now;
        cabin_data_update_mock(&data, delta_time);

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
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
