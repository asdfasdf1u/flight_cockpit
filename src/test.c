/* Development-only single-module switches; keep Launcher as the default entry. */
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
#include <SDL2/SDL2_gfxPrimitives.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

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
#include "Data/sim_data_center.h"
#include "Util/xplane_live_data.h"
#endif

#ifdef TEST_MODULE_LAUNCHER
#define LAUNCHER_WIDTH 1672
#define LAUNCHER_HEIGHT 894
#define LAUNCHER_CORNER_RADIUS 10

typedef enum LauncherChoice
{
    LAUNCHER_CHOICE_QUIT,
    LAUNCHER_CHOICE_COCKPIT,
    LAUNCHER_CHOICE_CABIN
} LauncherChoice;

typedef struct LauncherCanvas
{
    float scale;
    float offset_x;
    float offset_y;
    int window_width;
    int window_height;
    int output_width;
    int output_height;
} LauncherCanvas;

typedef struct LauncherFonts
{
    TTF_Font *page_title;
    TTF_Font *page_subtitle;
    TTF_Font *card_number;
    TTF_Font *card_title;
    TTF_Font *card_hint;
    TTF_Font *panel_title;
    TTF_Font *panel_label;
    TTF_Font *panel_text;
    TTF_Font *keycap;
} LauncherFonts;

typedef struct LauncherLayout
{
    SDL_Rect cockpit_card;
    SDL_Rect cabin_card;
    SDL_Rect shortcuts_panel;
} LauncherLayout;

static const SDL_Color LAUNCHER_BACKGROUND_TOP = {5, 24, 47, 255};
static const SDL_Color LAUNCHER_BACKGROUND_BOTTOM = {3, 18, 35, 255};
static const SDL_Color LAUNCHER_CARD = {19, 45, 78, 238};
static const SDL_Color LAUNCHER_CARD_HOVER = {24, 55, 91, 244};
static const SDL_Color LAUNCHER_PANEL = {13, 34, 61, 70};
static const SDL_Color LAUNCHER_TEXT = {231, 237, 245, 255};
static const SDL_Color LAUNCHER_MUTED_TEXT = {157, 175, 198, 255};
static const SDL_Color LAUNCHER_BORDER = {61, 91, 124, 255};
static const SDL_Color LAUNCHER_DIVIDER = {44, 67, 94, 255};
static const SDL_Color LAUNCHER_KEY_FILL = {18, 39, 66, 255};
static const SDL_Color LAUNCHER_KEY_BORDER = {84, 119, 154, 255};
static const SDL_Color LAUNCHER_BLUE = {39, 148, 246, 255};
static const SDL_Color LAUNCHER_CYAN = {55, 204, 215, 255};

static void launcher_runtime_log(int truncate, const char *format, ...)
{
    char log_path[MAX_PATH];
    DWORD path_length = GetModuleFileNameA(NULL, log_path, (DWORD)sizeof(log_path));
    FILE *log_file = NULL;

    if (path_length > 0 && path_length < sizeof(log_path))
    {
        char *file_name = strrchr(log_path, '\\');
        if (file_name != NULL)
        {
            snprintf(file_name + 1, sizeof(log_path) - (size_t)(file_name + 1 - log_path), "launcher_runtime.log");
            log_file = fopen(log_path, truncate ? "w" : "a");
        }
    }

    if (log_file == NULL)
    {
        log_file = fopen("launcher_runtime.log", truncate ? "w" : "a");
    }
    if (log_file == NULL)
    {
        return;
    }

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fputc('\n', log_file);
    fclose(log_file);
}

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

static void draw_line(SDL_Renderer *renderer, int x1, int y1, int x2, int y2, SDL_Color color)
{
    set_color(renderer, color);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

static void fill_rounded_rect(SDL_Renderer *renderer, const SDL_Rect *rect, int radius, SDL_Color color)
{
    if (renderer == NULL || rect == NULL)
    {
        return;
    }

    roundedBoxRGBA(renderer, rect->x, rect->y, rect->x + rect->w - 1, rect->y + rect->h - 1,
                   radius, color.r, color.g, color.b, color.a);
}

static void draw_rounded_rect(SDL_Renderer *renderer, const SDL_Rect *rect, int radius, SDL_Color color)
{
    if (renderer == NULL || rect == NULL)
    {
        return;
    }

    roundedRectangleRGBA(renderer, rect->x, rect->y, rect->x + rect->w - 1, rect->y + rect->h - 1,
                         radius, color.r, color.g, color.b, color.a);
}

static void draw_vertical_gradient(SDL_Renderer *renderer, const SDL_Rect *rect, SDL_Color top, SDL_Color bottom)
{
    int y;
    if (renderer == NULL || rect == NULL || rect->h <= 0)
    {
        return;
    }

    for (y = 0; y < rect->h; ++y)
    {
        float ratio = (float)y / (float)(rect->h - 1);
        SDL_Color color = {
            (Uint8)(top.r + (bottom.r - top.r) * ratio),
            (Uint8)(top.g + (bottom.g - top.g) * ratio),
            (Uint8)(top.b + (bottom.b - top.b) * ratio),
            255};
        draw_line(renderer, rect->x, rect->y + y, rect->x + rect->w - 1, rect->y + y, color);
    }
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

static void launcher_canvas_update(SDL_Window *window, SDL_Renderer *renderer, LauncherCanvas *canvas)
{
    float scale_x;
    float scale_y;

    if (window == NULL || renderer == NULL || canvas == NULL)
    {
        return;
    }

    SDL_GetWindowSize(window, &canvas->window_width, &canvas->window_height);
    SDL_GetRendererOutputSize(renderer, &canvas->output_width, &canvas->output_height);
    if (canvas->window_width <= 0 || canvas->window_height <= 0 ||
        canvas->output_width <= 0 || canvas->output_height <= 0)
    {
        return;
    }

    scale_x = (float)canvas->window_width / (float)LAUNCHER_WIDTH;
    scale_y = (float)canvas->window_height / (float)LAUNCHER_HEIGHT;
    canvas->scale = scale_x < scale_y ? scale_x : scale_y;
    canvas->offset_x = ((float)canvas->window_width - (float)LAUNCHER_WIDTH * canvas->scale) * 0.5f;
    canvas->offset_y = ((float)canvas->window_height - (float)LAUNCHER_HEIGHT * canvas->scale) * 0.5f;
}

static void launcher_canvas_apply(SDL_Renderer *renderer, const LauncherCanvas *canvas)
{
    SDL_Rect viewport;
    float output_scale_x;
    float output_scale_y;

    if (renderer == NULL || canvas == NULL || canvas->window_width <= 0 || canvas->window_height <= 0)
    {
        return;
    }

    output_scale_x = (float)canvas->output_width / (float)canvas->window_width;
    output_scale_y = (float)canvas->output_height / (float)canvas->window_height;
    viewport.x = (int)(canvas->offset_x * output_scale_x);
    viewport.y = (int)(canvas->offset_y * output_scale_y);
    viewport.w = (int)((float)LAUNCHER_WIDTH * canvas->scale * output_scale_x);
    viewport.h = (int)((float)LAUNCHER_HEIGHT * canvas->scale * output_scale_y);

    SDL_RenderSetViewport(renderer, &viewport);
    SDL_RenderSetScale(renderer, canvas->scale * output_scale_x, canvas->scale * output_scale_y);
}

static void launcher_window_to_canvas(const LauncherCanvas *canvas, int window_x, int window_y, int *canvas_x, int *canvas_y)
{
    if (canvas == NULL || canvas->scale <= 0.0f || canvas_x == NULL || canvas_y == NULL)
    {
        return;
    }

    *canvas_x = (int)(((float)window_x - canvas->offset_x) / canvas->scale);
    *canvas_y = (int)(((float)window_y - canvas->offset_y) / canvas->scale);
}

static LauncherLayout launcher_layout(void)
{
    LauncherLayout layout;
    layout.cockpit_card = (SDL_Rect){284, 250, 510, 318};
    layout.cabin_card = (SDL_Rect){878, 250, 510, 318};
    layout.shortcuts_panel = (SDL_Rect){220, 614, 1232, 217};
    return layout;
}

static void draw_keycap(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const char *label)
{
    fill_rounded_rect(renderer, rect, 6, LAUNCHER_KEY_FILL);
    draw_rounded_rect(renderer, rect, 6, LAUNCHER_KEY_BORDER);
    draw_centered_text(renderer, font, LAUNCHER_TEXT, rect, label);
}

static void draw_launcher_card(SDL_Renderer *renderer, const LauncherFonts *fonts, const SDL_Rect *rect,
                               const char *number, const char *title, const char *hint, SDL_Color accent, int hovered)
{
    SDL_Rect shadow = {rect->x + 3, rect->y + 6, rect->w, rect->h};
    SDL_Rect accent_bar = {rect->x + LAUNCHER_CORNER_RADIUS, rect->y, rect->w - LAUNCHER_CORNER_RADIUS * 2, 8};
    SDL_Color border = hovered ? accent : LAUNCHER_BORDER;
    SDL_Color card = hovered ? LAUNCHER_CARD_HOVER : LAUNCHER_CARD;

    fill_rounded_rect(renderer, &shadow, LAUNCHER_CORNER_RADIUS, (SDL_Color){0, 9, 22, 105});
    fill_rounded_rect(renderer, rect, LAUNCHER_CORNER_RADIUS, card);
    fill_rect(renderer, &accent_bar, accent);
    draw_rounded_rect(renderer, rect, LAUNCHER_CORNER_RADIUS, border);
    draw_line(renderer, rect->x + 150, rect->y + 75, rect->x + 214, rect->y + 75, (SDL_Color){accent.r, accent.g, accent.b, 135});
    draw_line(renderer, rect->x + 296, rect->y + 75, rect->x + 360, rect->y + 75, (SDL_Color){accent.r, accent.g, accent.b, 135});
    draw_centered_text(renderer, fonts->card_number, accent, &(SDL_Rect){rect->x, rect->y + 51, rect->w, 52}, number);
    draw_centered_text(renderer, fonts->card_title, LAUNCHER_TEXT, &(SDL_Rect){rect->x, rect->y + 119, rect->w, 70}, title);
    draw_centered_text(renderer, fonts->card_hint, LAUNCHER_MUTED_TEXT, &(SDL_Rect){rect->x, rect->y + 199, rect->w, 46}, hint);
}

static void draw_shortcuts_panel(SDL_Renderer *renderer, const LauncherFonts *fonts, const SDL_Rect *panel)
{
    const int row1_y = panel->y + 64;
    const int row2_y = panel->y + 154;
    const int key_h = 40;

    fill_rounded_rect(renderer, panel, LAUNCHER_CORNER_RADIUS, LAUNCHER_PANEL);
    draw_rounded_rect(renderer, panel, LAUNCHER_CORNER_RADIUS, LAUNCHER_BORDER);
    draw_line(renderer, 698, panel->y + 30, 758, panel->y + 30, LAUNCHER_MUTED_TEXT);
    draw_line(renderer, 914, panel->y + 30, 974, panel->y + 30, LAUNCHER_MUTED_TEXT);
    draw_centered_text(renderer, fonts->panel_title, LAUNCHER_TEXT, &(SDL_Rect){760, panel->y + 15, 152, 34}, "快捷操作");
    draw_line(renderer, panel->x + 17, panel->y + 130, panel->x + panel->w - 18, panel->y + 130, LAUNCHER_DIVIDER);

    draw_text(renderer, fonts->panel_label, LAUNCHER_TEXT, 264, row1_y + 9, "ND界面");
    draw_keycap(renderer, fonts->keycap, &(SDL_Rect){377, row1_y, 47, key_h}, "1");
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 441, row1_y + 11, "WPT");
    draw_keycap(renderer, fonts->keycap, &(SDL_Rect){515, row1_y, 47, key_h}, "2");
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 578, row1_y + 11, "ARPT");
    draw_keycap(renderer, fonts->keycap, &(SDL_Rect){656, row1_y, 47, key_h}, "3");
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 720, row1_y + 11, "STA");
    draw_keycap(renderer, fonts->keycap, &(SDL_Rect){792, row1_y, 47, key_h}, "L");
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 856, row1_y + 11, "文字");
    draw_line(renderer, 946, row1_y, 946, row1_y + key_h, LAUNCHER_DIVIDER);
    draw_text(renderer, fonts->panel_label, LAUNCHER_TEXT, 990, row1_y + 9, "客舱");
    draw_keycap(renderer, fonts->keycap, &(SDL_Rect){1065, row1_y, 47, key_h}, "C");
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 1129, row1_y + 11, "简洁模式");
    draw_line(renderer, 1250, row1_y, 1250, row1_y + key_h, LAUNCHER_DIVIDER);
    draw_keycap(renderer, fonts->keycap, &(SDL_Rect){1290, row1_y, 60, key_h}, "ESC");
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 1367, row1_y + 11, "退出");

    draw_text(renderer, fonts->panel_label, LAUNCHER_TEXT, 258, row2_y + 9, "警报演示");
    draw_line(renderer, 371, row2_y, 371, row2_y + key_h, LAUNCHER_DIVIDER);
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 403, row2_y + 11, "坠机");
    draw_keycap(renderer, fonts->keycap, &(SDL_Rect){454, row2_y, 47, key_h}, "Y");
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 516, row2_y + 11, "开启");
    draw_keycap(renderer, fonts->keycap, &(SDL_Rect){577, row2_y, 47, key_h}, "R");
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 641, row2_y + 11, "关闭");
    draw_line(renderer, 716, row2_y, 716, row2_y + key_h, LAUNCHER_DIVIDER);
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 757, row2_y + 11, "火警");
    draw_keycap(renderer, fonts->keycap, &(SDL_Rect){816, row2_y, 47, key_h}, "G");
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 879, row2_y + 11, "开启");
    draw_keycap(renderer, fonts->keycap, &(SDL_Rect){943, row2_y, 47, key_h}, "H");
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 1007, row2_y + 11, "关闭");
    draw_line(renderer, 1088, row2_y, 1088, row2_y + key_h, LAUNCHER_DIVIDER);
    draw_text(renderer, fonts->panel_label, LAUNCHER_TEXT, 1129, row2_y + 9, "系统");
    draw_keycap(renderer, fonts->keycap, &(SDL_Rect){1188, row2_y, 47, key_h}, "P");
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 1251, row2_y + 11, "开启");
    draw_keycap(renderer, fonts->keycap, &(SDL_Rect){1314, row2_y, 47, key_h}, "O");
    draw_text(renderer, fonts->panel_text, LAUNCHER_MUTED_TEXT, 1377, row2_y + 11, "关闭");
}

static void draw_launcher_home(SDL_Renderer *renderer, const LauncherFonts *fonts, const LauncherLayout *layout,
                               int cockpit_hovered, int cabin_hovered)
{
    draw_vertical_gradient(renderer, &(SDL_Rect){0, 0, LAUNCHER_WIDTH, LAUNCHER_HEIGHT}, LAUNCHER_BACKGROUND_TOP, LAUNCHER_BACKGROUND_BOTTOM);
    draw_centered_text(renderer, fonts->page_title, LAUNCHER_TEXT, &(SDL_Rect){0, 73, LAUNCHER_WIDTH, 70}, "航空模拟座舱显示系统");
    draw_centered_text(renderer, fonts->page_subtitle, LAUNCHER_MUTED_TEXT, &(SDL_Rect){0, 158, LAUNCHER_WIDTH, 42}, "点击卡片或按数字键切换显示界面");
    draw_launcher_card(renderer, fonts, &layout->cockpit_card, "01", "主驾驶舱", "PFD / ND / EICAS / FMC", LAUNCHER_BLUE, cockpit_hovered);
    draw_launcher_card(renderer, fonts, &layout->cabin_card, "02", "客舱显示屏", "地图 / 天气 / 航班信息", LAUNCHER_CYAN, cabin_hovered);
    draw_shortcuts_panel(renderer, fonts, &layout->shortcuts_panel);
}

static LauncherChoice run_launcher_window(void)
{
    launcher_runtime_log(0, "Creating launcher window.");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Launcher: SDL_Init failed: %s\n", SDL_GetError());
        launcher_runtime_log(0, "FAILED: SDL_Init: %s", SDL_GetError());
        return LAUNCHER_CHOICE_QUIT;
    }

    if (TTF_Init() != 0)
    {
        printf("Launcher: TTF_Init failed: %s\n", TTF_GetError());
        launcher_runtime_log(0, "FAILED: TTF_Init: %s", TTF_GetError());
        SDL_Quit();
        return LAUNCHER_CHOICE_QUIT;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Flight Cockpit Launcher",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        LAUNCHER_WIDTH,
        LAUNCHER_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == NULL)
    {
        printf("Launcher: SDL_CreateWindow failed: %s\n", SDL_GetError());
        launcher_runtime_log(0, "FAILED: SDL_CreateWindow: %s", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return LAUNCHER_CHOICE_QUIT;
    }

    SDL_SetWindowMinimumSize(window, LAUNCHER_WIDTH / 2, LAUNCHER_HEIGHT / 2);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL)
    {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == NULL)
    {
        printf("Launcher: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        launcher_runtime_log(0, "FAILED: SDL_CreateRenderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return LAUNCHER_CHOICE_QUIT;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    LauncherFonts fonts = {
        open_launcher_font(53),
        open_launcher_font(26),
        open_launcher_font(35),
        open_launcher_font(45),
        open_launcher_font(25),
        open_launcher_font(23),
        open_launcher_font(19),
        open_launcher_font(17),
        open_launcher_font(18)};
    if (fonts.page_title != NULL)
    {
        TTF_SetFontStyle(fonts.page_title, TTF_STYLE_BOLD);
    }
    if (fonts.card_title != NULL)
    {
        TTF_SetFontStyle(fonts.card_title, TTF_STYLE_BOLD);
    }
    if (fonts.page_title == NULL || fonts.page_subtitle == NULL || fonts.card_number == NULL ||
        fonts.card_title == NULL || fonts.card_hint == NULL || fonts.panel_title == NULL ||
        fonts.panel_label == NULL || fonts.panel_text == NULL || fonts.keycap == NULL)
    {
        printf("Launcher: font load failed: %s\n", TTF_GetError());
        launcher_runtime_log(0, "WARNING: launcher font load: %s", TTF_GetError());
    }
    launcher_runtime_log(0, "Launcher event loop entered.");

    LauncherLayout layout = launcher_layout();
    LauncherCanvas canvas = {0};
    launcher_canvas_update(window, renderer, &canvas);

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
            else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                launcher_canvas_update(window, renderer, &canvas);
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
                int canvas_x = 0;
                int canvas_y = 0;
                launcher_window_to_canvas(&canvas, event.button.x, event.button.y, &canvas_x, &canvas_y);
                if (point_in_rect(canvas_x, canvas_y, &layout.cockpit_card))
                {
                    running = 0;
                    choice = LAUNCHER_CHOICE_COCKPIT;
                    launcher_runtime_log(0, "Cockpit button selected.");
                }
                else if (point_in_rect(canvas_x, canvas_y, &layout.cabin_card))
                {
                    running = 0;
                    choice = LAUNCHER_CHOICE_CABIN;
                    launcher_runtime_log(0, "Cabin button selected.");
                }
            }
        }

        int mouse_x = 0;
        int mouse_y = 0;
        int canvas_mouse_x = 0;
        int canvas_mouse_y = 0;
        SDL_GetMouseState(&mouse_x, &mouse_y);
        launcher_canvas_update(window, renderer, &canvas);
        launcher_window_to_canvas(&canvas, mouse_x, mouse_y, &canvas_mouse_x, &canvas_mouse_y);

        SDL_RenderSetScale(renderer, 1.0f, 1.0f);
        SDL_RenderSetViewport(renderer, NULL);
        set_color(renderer, LAUNCHER_BACKGROUND_BOTTOM);
        SDL_RenderClear(renderer);
        launcher_canvas_apply(renderer, &canvas);
        draw_launcher_home(renderer, &fonts, &layout,
                           point_in_rect(canvas_mouse_x, canvas_mouse_y, &layout.cockpit_card),
                           point_in_rect(canvas_mouse_x, canvas_mouse_y, &layout.cabin_card));

        SDL_RenderPresent(renderer);
    }

    TTF_Font *font_list[] = {
        fonts.page_title, fonts.page_subtitle, fonts.card_number, fonts.card_title, fonts.card_hint,
        fonts.panel_title, fonts.panel_label, fonts.panel_text, fonts.keycap};
    size_t font_index;
    for (font_index = 0; font_index < sizeof(font_list) / sizeof(font_list[0]); ++font_index)
    {
        if (font_list[font_index] != NULL)
        {
            TTF_CloseFont(font_list[font_index]);
        }
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    launcher_runtime_log(0, "Launcher window released; choice=%d.", choice);

    return choice;
}

static int initialize_shared_runtime(SimDataCenter *shared_sim_data_center, XPlaneSharedRuntime *shared_runtime)
{
    if (shared_sim_data_center == NULL || shared_runtime == NULL)
    {
        return 0;
    }

    launcher_runtime_log(0, "Initializing shared runtime after a module was selected.");
    if (!sim_data_center_init(shared_sim_data_center))
    {
        launcher_runtime_log(0, "FAILED: shared SimDataCenter initialization.");
        return 0;
    }
    launcher_runtime_log(0, "Shared SimDataCenter=%p planned_route=%p revision=%d.",
                         (void *)shared_sim_data_center,
                         (void *)&shared_sim_data_center->planned_route,
                         sim_data_center_route_revision(shared_sim_data_center));

    xplane_shared_runtime_init(shared_runtime, shared_sim_data_center, NULL, 0);
    if (!xplane_shared_runtime_initialized(shared_runtime))
    {
        launcher_runtime_log(0, "FAILED: shared XPlane runtime initialization.");
        xplane_shared_runtime_shutdown(shared_runtime);
        sim_data_center_destroy(shared_sim_data_center);
        return 0;
    }

    launcher_runtime_log(0, "Shared runtime initialized with SimDataCenter=%p.", (void *)shared_sim_data_center);
    return 1;
}

static int run_launcher(void)
{
    int exit_code = 0;
    int shared_runtime_ready = 0;
    SimDataCenter *shared_sim_data_center = (SimDataCenter *)malloc(sizeof(*shared_sim_data_center));
    XPlaneSharedRuntime *shared_runtime = (XPlaneSharedRuntime *)malloc(sizeof(*shared_runtime));

    launcher_runtime_log(1, "Launcher process started.");
    if (shared_sim_data_center == NULL)
    {
        launcher_runtime_log(0, "FAILED: shared SimDataCenter allocation.");
        return -1;
    }
    if (shared_runtime == NULL)
    {
        launcher_runtime_log(0, "FAILED: shared XPlane runtime allocation.");
        free(shared_sim_data_center);
        return -1;
    }

    for (;;)
    {
        LauncherChoice choice = run_launcher_window();
        if (choice == LAUNCHER_CHOICE_QUIT)
        {
            launcher_runtime_log(0, "Launcher exit selected.");
            break;
        }

        if (!shared_runtime_ready)
        {
            if (!initialize_shared_runtime(shared_sim_data_center, shared_runtime))
            {
                exit_code = -1;
                break;
            }
            shared_runtime_ready = 1;
        }

        if (choice == LAUNCHER_CHOICE_COCKPIT)
        {
            launcher_runtime_log(0, "Calling cockpit_main_run_with_shared_runtime(%p).", (void *)shared_runtime);
            exit_code = cockpit_main_run_with_shared_runtime(shared_runtime);
            launcher_runtime_log(0, "Cockpit returned %d; shared revision=%d.", exit_code,
                                 sim_data_center_route_revision(shared_sim_data_center));
        }
        else if (choice == LAUNCHER_CHOICE_CABIN)
        {
            launcher_runtime_log(0, "Calling cabin_main_run_with_shared_runtime(%p).", (void *)shared_runtime);
            exit_code = cabin_main_run_with_shared_runtime(shared_runtime);
            launcher_runtime_log(0, "Cabin returned %d; shared revision=%d origin=%s destination=%s points=%d.",
                                 exit_code,
                                 sim_data_center_route_revision(shared_sim_data_center),
                                 shared_sim_data_center->planned_route.origin,
                                 shared_sim_data_center->planned_route.destination,
                                 shared_sim_data_center->planned_route.point_count);
        }

        if (exit_code != 0)
        {
            break;
        }
    }

    if (shared_runtime_ready)
    {
        launcher_runtime_log(0, "Destroying shared SimDataCenter=%p revision=%d.",
                             (void *)shared_sim_data_center,
                             sim_data_center_route_revision(shared_sim_data_center));
        xplane_shared_runtime_shutdown(shared_runtime);
        sim_data_center_destroy(shared_sim_data_center);
    }
    free(shared_runtime);
    free(shared_sim_data_center);
    launcher_runtime_log(0, "Launcher process exiting with code=%d.", exit_code);
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
    return cockpit_main_run_with_args(argc, argv);
#endif

#ifdef TEST_MODULE_CABIN
    return cabin_main_run();
#endif

#ifdef TEST_MODULE_LAUNCHER
    return run_launcher();
#endif

    return 0;
}
