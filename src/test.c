/* #define TEST_MODULE_PFD */
/* #define TEST_MODULE_ND */
/* #define TEST_MODULE_EICAS1 */
/* #define TEST_MODULE_EICAS2 */
/* #define TEST_MODULE_FMC */
/* #define TEST_MODULE_COCKPIT */
/* #define TEST_MODULE_CABIN */
#define TEST_MODULE_LAUNCHER

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#ifdef TEST_MODULE_PFD
#include "PFD/pfd_main.h"
#endif

#ifdef TEST_MODULE_ND
#include "ND/nd_main.h"
#endif

#ifdef TEST_MODULE_EICAS1
#include "EICAS1/eicas1_main.h"
#endif

#ifdef TEST_MODULE_EICAS2
#include "EICAS2/eicas2_main.h"
#endif

#ifdef TEST_MODULE_FMC
int fmc_main_run(void);
#endif

#if defined(TEST_MODULE_COCKPIT) || defined(TEST_MODULE_LAUNCHER)
#include "Cockpit/cockpit_main.h"
#endif

#if defined(TEST_MODULE_CABIN) || defined(TEST_MODULE_LAUNCHER)
#include "Cabin/cabin_main.h"
#endif

#ifdef TEST_MODULE_LAUNCHER
#define LAUNCHER_WIDTH 900
#define LAUNCHER_HEIGHT 520

typedef enum LauncherChoice
{
    LAUNCHER_CHOICE_QUIT,
    LAUNCHER_CHOICE_COCKPIT,
    LAUNCHER_CHOICE_CABIN
} LauncherChoice;

static TTF_Font *open_launcher_font(int size)
{
    TTF_Font *font = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", size);
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

static void set_color(SDL_Renderer *renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static void fill_rect(SDL_Renderer *renderer, const SDL_Rect *rect, SDL_Color color)
{
    set_color(renderer, color);
    SDL_RenderFillRect(renderer, rect);
}

static void draw_rect(SDL_Renderer *renderer, const SDL_Rect *rect, SDL_Color color)
{
    set_color(renderer, color);
    SDL_RenderDrawRect(renderer, rect);
}

static void draw_text(SDL_Renderer *renderer, TTF_Font *font, SDL_Color color, int x, int y, const char *text)
{
    if (renderer == NULL || font == NULL || text == NULL)
    {
        return;
    }

    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (surface == NULL)
    {
        return;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL)
    {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dest = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

static void draw_centered_text(SDL_Renderer *renderer, TTF_Font *font, SDL_Color color, const SDL_Rect *rect, const char *text)
{
    int text_w = 0;
    int text_h = 0;
    if (font == NULL || rect == NULL || TTF_SizeUTF8(font, text, &text_w, &text_h) != 0)
    {
        return;
    }

    draw_text(renderer, font, color, rect->x + (rect->w - text_w) / 2, rect->y + (rect->h - text_h) / 2, text);
}

static int point_in_rect(int x, int y, const SDL_Rect *rect)
{
    return rect != NULL &&
           x >= rect->x &&
           x < rect->x + rect->w &&
           y >= rect->y &&
           y < rect->y + rect->h;
}

static void draw_launcher_button(
    SDL_Renderer *renderer,
    TTF_Font *title_font,
    TTF_Font *font,
    const SDL_Rect *rect,
    const char *title,
    const char *hint,
    SDL_Color accent)
{
    const SDL_Color card = {34, 46, 62, 255};
    const SDL_Color border = {94, 129, 166, 255};
    const SDL_Color white = {240, 245, 250, 255};
    const SDL_Color muted = {176, 194, 210, 255};
    const SDL_Rect accent_rect = {rect->x, rect->y, rect->w, 8};

    fill_rect(renderer, rect, card);
    fill_rect(renderer, &accent_rect, accent);
    draw_rect(renderer, rect, border);

    draw_centered_text(renderer, title_font, white, &(SDL_Rect){rect->x, rect->y + 54, rect->w, 42}, title);
    draw_centered_text(renderer, font, muted, &(SDL_Rect){rect->x, rect->y + 114, rect->w, 32}, hint);
}

static LauncherChoice run_launcher_window(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Launcher: SDL_Init failed: %s\n", SDL_GetError());
        return LAUNCHER_CHOICE_QUIT;
    }

    if (TTF_Init() != 0)
    {
        printf("Launcher: TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return LAUNCHER_CHOICE_QUIT;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Flight Cockpit Launcher",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        LAUNCHER_WIDTH,
        LAUNCHER_HEIGHT,
        SDL_WINDOW_SHOWN);
    if (window == NULL)
    {
        printf("Launcher: SDL_CreateWindow failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return LAUNCHER_CHOICE_QUIT;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL)
    {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == NULL)
    {
        printf("Launcher: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return LAUNCHER_CHOICE_QUIT;
    }

    TTF_Font *title_font = open_launcher_font(30);
    TTF_Font *font = open_launcher_font(20);
    TTF_Font *small_font = open_launcher_font(17);
    if (title_font == NULL || font == NULL || small_font == NULL)
    {
        printf("Launcher: font load failed: %s\n", TTF_GetError());
    }

    const SDL_Color bg = {18, 28, 42, 255};
    const SDL_Color title = {240, 246, 252, 255};
    const SDL_Color muted = {158, 178, 196, 255};
    const SDL_Color blue = {31, 142, 237, 255};
    const SDL_Color cyan = {48, 206, 225, 255};
    const SDL_Rect cockpit_rect = {110, 190, 300, 190};
    const SDL_Rect cabin_rect = {490, 190, 300, 190};

    LauncherChoice choice = LAUNCHER_CHOICE_QUIT;
    int running = 1;
    SDL_Event event;

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = 0;
                choice = LAUNCHER_CHOICE_QUIT;
            }
            else if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    running = 0;
                    choice = LAUNCHER_CHOICE_QUIT;
                }
                else if (event.key.keysym.sym == SDLK_1)
                {
                    running = 0;
                    choice = LAUNCHER_CHOICE_COCKPIT;
                }
                else if (event.key.keysym.sym == SDLK_2)
                {
                    running = 0;
                    choice = LAUNCHER_CHOICE_CABIN;
                }
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
            {
                if (point_in_rect(event.button.x, event.button.y, &cockpit_rect))
                {
                    running = 0;
                    choice = LAUNCHER_CHOICE_COCKPIT;
                }
                else if (point_in_rect(event.button.x, event.button.y, &cabin_rect))
                {
                    running = 0;
                    choice = LAUNCHER_CHOICE_CABIN;
                }
            }
        }

        fill_rect(renderer, &(SDL_Rect){0, 0, LAUNCHER_WIDTH, LAUNCHER_HEIGHT}, bg);
        draw_centered_text(renderer, title_font, title, &(SDL_Rect){0, 64, LAUNCHER_WIDTH, 44}, "航空模拟系统显示选择");
        draw_centered_text(renderer, font, muted, &(SDL_Rect){0, 116, LAUNCHER_WIDTH, 30}, "点击卡片或按数字键切换显示界面");

        draw_launcher_button(renderer, title_font, font, &cockpit_rect, "1  主驾驶舱", "PFD / ND / EICAS / FMC", blue);
        draw_launcher_button(renderer, title_font, font, &cabin_rect, "2  客舱显示屏", "地图 / 天气 / 航班信息", cyan);
        draw_centered_text(renderer, small_font, muted, &(SDL_Rect){0, 438, LAUNCHER_WIDTH, 28}, "关闭当前界面后会回到这里，ESC 退出");

        SDL_RenderPresent(renderer);
    }

    if (title_font != NULL)
    {
        TTF_CloseFont(title_font);
    }
    if (font != NULL)
    {
        TTF_CloseFont(font);
    }
    if (small_font != NULL)
    {
        TTF_CloseFont(small_font);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return choice;
}

static int run_launcher(void)
{
    int exit_code = 0;

    for (;;)
    {
        LauncherChoice choice = run_launcher_window();
        if (choice == LAUNCHER_CHOICE_QUIT)
        {
            break;
        }

        if (choice == LAUNCHER_CHOICE_COCKPIT)
        {
            exit_code = cockpit_main_run();
        }
        else if (choice == LAUNCHER_CHOICE_CABIN)
        {
            exit_code = cabin_main_run();
        }

        if (exit_code != 0)
        {
            break;
        }
    }

    return exit_code;
}
#endif

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

#ifdef TEST_MODULE_PFD
    return pfd_main_run();
#endif

#ifdef TEST_MODULE_ND
    return nd_main_run();
#endif

#ifdef TEST_MODULE_EICAS1
    return eicas1_main_run();
#endif

#ifdef TEST_MODULE_EICAS2
    return eicas2_main_run();
#endif

#ifdef TEST_MODULE_FMC
    return fmc_main_run();
#endif

#ifdef TEST_MODULE_COCKPIT
    return cockpit_main_run();
#endif

#ifdef TEST_MODULE_CABIN
    return cabin_main_run();
#endif

#ifdef TEST_MODULE_LAUNCHER
    return run_launcher();
#endif

    return 0;
}
