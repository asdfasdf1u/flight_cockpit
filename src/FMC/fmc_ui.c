#include "fmc_ui.h"

#include <stdarg.h>
#include <stdio.h>

typedef struct FMC_ButtonDef
{
    SDL_Rect rect;
    const char *label;
    FMC_Page page;
} FMC_ButtonDef;

static const SDL_Color COLOR_BG = {4, 6, 9, 255};
static const SDL_Color COLOR_SHELL = {35, 39, 42, 255};
static const SDL_Color COLOR_SHELL_EDGE = {92, 99, 104, 255};
static const SDL_Color COLOR_SCREEN = {6, 18, 13, 255};
static const SDL_Color COLOR_SCREEN_EDGE = {80, 230, 150, 255};
static const SDL_Color COLOR_KEY = {26, 31, 34, 255};
static const SDL_Color COLOR_KEY_EDGE = {132, 140, 145, 255};
static const SDL_Color COLOR_TEXT = {214, 255, 218, 255};
static const SDL_Color COLOR_DIM = {110, 155, 125, 255};
static const SDL_Color COLOR_CYAN = {80, 220, 255, 255};
static const SDL_Color COLOR_WHITE = {238, 244, 240, 255};
static const SDL_Color COLOR_AMBER = {255, 190, 65, 255};

#define FMC_BUTTON_COUNT 5

static const SDL_Rect FMC_SHELL_RECT = {30, 20, 640, 850};
static const SDL_Rect FMC_SCREEN_RECT = {75, 55, 550, 520};
static const SDL_Rect FMC_SCRATCHPAD_RECT = {95, 524, 510, 34};
static const SDL_Rect FMC_CLR_BUTTON_RECT = {515, 665, 110, 62};

static const FMC_ButtonDef FMC_PAGE_BUTTONS[FMC_BUTTON_COUNT] = {
    {{75, 610, 110, 62}, "INDEX", FMC_PAGE_INDEX},
    {{185, 610, 110, 62}, "RTE", FMC_PAGE_ROUTE},
    {{295, 610, 110, 62}, "DEP/ARR", FMC_PAGE_DEP_ARR},
    {{75, 690, 110, 62}, "PERF", FMC_PAGE_PERF},
    {{185, 690, 110, 62}, "LEGS", FMC_PAGE_LEGS},
};

static int point_in_rect(int x, int y, const SDL_Rect *rect)
{
    return rect != NULL &&
           x >= rect->x &&
           x < rect->x + rect->w &&
           y >= rect->y &&
           y < rect->y + rect->h;
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

static void draw_text(SDL_Renderer *renderer, TTF_Font *font, SDL_Color color, int x, int y, const char *format, ...)
{
    if (renderer == NULL || font == NULL || format == NULL)
    {
        return;
    }

    char text[180];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

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

static void draw_centered_text(SDL_Renderer *renderer, TTF_Font *font, SDL_Color color, const SDL_Rect *rect, const char *format, ...)
{
    if (renderer == NULL || font == NULL || rect == NULL || format == NULL)
    {
        return;
    }

    char text[180];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    int text_w = 0;
    int text_h = 0;
    if (TTF_SizeUTF8(font, text, &text_w, &text_h) != 0)
    {
        return;
    }

    draw_text(renderer, font, color, rect->x + (rect->w - text_w) / 2, rect->y + (rect->h - text_h) / 2, "%s", text);
}

static const char *page_title(FMC_Page page)
{
    switch (page)
    {
    case FMC_PAGE_ROUTE:
        return "RTE 1/1";
    case FMC_PAGE_DEP_ARR:
        return "DEP/ARR";
    case FMC_PAGE_PERF:
        return "PERF INIT";
    case FMC_PAGE_LEGS:
        return "LEGS";
    case FMC_PAGE_INDEX:
    default:
        return "FMC INDEX";
    }
}

static void draw_button(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const char *label, int active)
{
    fill_rect(renderer, rect, active ? (SDL_Color){40, 54, 48, 255} : COLOR_KEY);
    draw_rect(renderer, rect, active ? COLOR_SCREEN_EDGE : COLOR_KEY_EDGE);
    draw_centered_text(renderer, font, active ? COLOR_SCREEN_EDGE : COLOR_WHITE, rect, "%s", label);
}

static void draw_fmc_shell(SDL_Renderer *renderer, TTF_Font *font)
{
    fill_rect(renderer, &FMC_SHELL_RECT, COLOR_SHELL);
    draw_rect(renderer, &FMC_SHELL_RECT, COLOR_SHELL_EDGE);
    draw_rect(renderer, &(SDL_Rect){FMC_SHELL_RECT.x + 4, FMC_SHELL_RECT.y + 4, FMC_SHELL_RECT.w - 8, FMC_SHELL_RECT.h - 8}, COLOR_KEY_EDGE);

    draw_centered_text(renderer, font, COLOR_WHITE, &(SDL_Rect){FMC_SHELL_RECT.x, FMC_SHELL_RECT.y + 8, FMC_SHELL_RECT.w, 30},
                       "FMC - Flight Management Computer");
}

static void draw_fmc_screen(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data)
{
    fill_rect(renderer, &FMC_SCREEN_RECT, COLOR_SCREEN);
    draw_rect(renderer, &FMC_SCREEN_RECT, COLOR_SCREEN_EDGE);
    draw_rect(renderer, &(SDL_Rect){FMC_SCREEN_RECT.x + 5, FMC_SCREEN_RECT.y + 5, FMC_SCREEN_RECT.w - 10, FMC_SCREEN_RECT.h - 10}, COLOR_DIM);
    draw_centered_text(renderer, font, COLOR_CYAN, &(SDL_Rect){FMC_SCREEN_RECT.x, FMC_SCREEN_RECT.y + 14, FMC_SCREEN_RECT.w, 30},
                       "%s", page_title(data->current_page));
}

static void draw_line_select_labels(SDL_Renderer *renderer, TTF_Font *font)
{
    for (int i = 0; i < 6; ++i)
    {
        const int y = FMC_SCREEN_RECT.y + 82 + i * 58;
        draw_text(renderer, font, COLOR_DIM, FMC_SCREEN_RECT.x + 14, y, "<");
        draw_text(renderer, font, COLOR_DIM, FMC_SCREEN_RECT.x + FMC_SCREEN_RECT.w - 28, y, ">");
    }
}

static void draw_index_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data)
{
    const int x = FMC_SCREEN_RECT.x + 52;
    int y = FMC_SCREEN_RECT.y + 80;

    draw_text(renderer, font, COLOR_DIM, x, y, "FLIGHT NO");
    draw_text(renderer, font, COLOR_TEXT, x + 250, y, "%s", data->flight_no);

    y += 72;
    draw_text(renderer, font, COLOR_TEXT, x, y, "< ROUTE");
    draw_text(renderer, font, COLOR_TEXT, x + 280, y, "DEP/ARR >");

    y += 58;
    draw_text(renderer, font, COLOR_TEXT, x, y, "< PERF INIT");
    draw_text(renderer, font, COLOR_TEXT, x + 300, y, "LEGS >");

    y += 86;
    draw_text(renderer, font, COLOR_DIM, x, y, "USE BUTTONS OR F1-F5 TO SELECT PAGE");
}

static void draw_route_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data)
{
    const int x = FMC_SCREEN_RECT.x + 52;
    int y = FMC_SCREEN_RECT.y + 78;

    draw_text(renderer, font, COLOR_DIM, x, y, "ORIGIN");
    draw_text(renderer, font, COLOR_TEXT, x + 180, y, "%s", data->origin);
    draw_text(renderer, font, COLOR_DIM, x + 310, y, "DEST");
    draw_text(renderer, font, COLOR_TEXT, x + 390, y, "%s", data->destination);

    y += 58;
    draw_text(renderer, font, COLOR_CYAN, x, y, "ROUTE POINTS");
    for (int i = 0; i < data->route_count && i < 6; ++i)
    {
        y += 42;
        draw_text(renderer, font, COLOR_DIM, x, y, "%02d", i + 1);
        draw_text(renderer, font, COLOR_TEXT, x + 60, y, "%s", data->route_points[i]);
    }
}

static void draw_dep_arr_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data)
{
    const int x = FMC_SCREEN_RECT.x + 52;
    int y = FMC_SCREEN_RECT.y + 88;

    draw_text(renderer, font, COLOR_DIM, x, y, "DEPARTURE AIRPORT");
    draw_text(renderer, font, COLOR_TEXT, x + 280, y, "%s", data->origin);

    y += 58;
    draw_text(renderer, font, COLOR_DIM, x, y, "DEP RUNWAY");
    draw_text(renderer, font, COLOR_TEXT, x + 280, y, "%s", data->departure_runway);

    y += 82;
    draw_text(renderer, font, COLOR_DIM, x, y, "ARRIVAL AIRPORT");
    draw_text(renderer, font, COLOR_TEXT, x + 280, y, "%s", data->destination);

    y += 58;
    draw_text(renderer, font, COLOR_DIM, x, y, "ARR RUNWAY");
    draw_text(renderer, font, COLOR_TEXT, x + 280, y, "%s", data->arrival_runway);
}

static void draw_perf_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data)
{
    const int x = FMC_SCREEN_RECT.x + 52;
    int y = FMC_SCREEN_RECT.y + 88;

    draw_text(renderer, font, COLOR_DIM, x, y, "CRZ ALT");
    draw_text(renderer, font, COLOR_TEXT, x + 260, y, "FL%03d", data->cruise_altitude / 100);

    y += 64;
    draw_text(renderer, font, COLOR_DIM, x, y, "TARGET SPD");
    draw_text(renderer, font, COLOR_TEXT, x + 260, y, "%d KT", data->target_speed);

    y += 64;
    draw_text(renderer, font, COLOR_DIM, x, y, "COST INDEX");
    draw_text(renderer, font, COLOR_TEXT, x + 260, y, "%.0f", data->cost_index);

    y += 88;
    draw_text(renderer, font, COLOR_AMBER, x, y, "PERFORMANCE DATA IS DEMO ONLY");
}

static void draw_legs_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data)
{
    const int x = FMC_SCREEN_RECT.x + 52;
    int y = FMC_SCREEN_RECT.y + 78;

    if (data->route_count >= 2)
    {
        draw_text(renderer, font, COLOR_CYAN, x, y, "ACTIVE LEG");
        draw_text(renderer, font, COLOR_TEXT, x + 180, y, "%s -> %s", data->route_points[0], data->route_points[1]);
    }

    y += 58;
    draw_text(renderer, font, COLOR_DIM, x, y, "SEQ");
    draw_text(renderer, font, COLOR_DIM, x + 72, y, "WAYPOINT");

    for (int i = 0; i < data->route_count && i < 7; ++i)
    {
        y += 38;
        draw_text(renderer, font, i == 1 ? COLOR_AMBER : COLOR_TEXT, x, y, "%02d", i + 1);
        draw_text(renderer, font, i == 1 ? COLOR_AMBER : COLOR_TEXT, x + 72, y, "%s", data->route_points[i]);
    }
}

static void draw_page_content(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data)
{
    draw_line_select_labels(renderer, font);

    switch (data->current_page)
    {
    case FMC_PAGE_ROUTE:
        draw_route_page(renderer, font, data);
        break;
    case FMC_PAGE_DEP_ARR:
        draw_dep_arr_page(renderer, font, data);
        break;
    case FMC_PAGE_PERF:
        draw_perf_page(renderer, font, data);
        break;
    case FMC_PAGE_LEGS:
        draw_legs_page(renderer, font, data);
        break;
    case FMC_PAGE_INDEX:
    default:
        draw_index_page(renderer, font, data);
        break;
    }
}

static void draw_scratchpad(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data)
{
    fill_rect(renderer, &FMC_SCRATCHPAD_RECT, (SDL_Color){2, 10, 7, 255});
    draw_rect(renderer, &FMC_SCRATCHPAD_RECT, COLOR_SCREEN_EDGE);

    if (data->scratchpad_len > 0)
    {
        draw_text(renderer, font, COLOR_WHITE, FMC_SCRATCHPAD_RECT.x + 12, FMC_SCRATCHPAD_RECT.y + 6, "%s", data->scratchpad);
    }
    else
    {
        draw_text(renderer, font, COLOR_DIM, FMC_SCRATCHPAD_RECT.x + 12, FMC_SCRATCHPAD_RECT.y + 6, "SCRATCHPAD");
    }
}

static void draw_keypad(SDL_Renderer *renderer, TTF_Font *font, FMC_Page current_page)
{
    for (int i = 0; i < FMC_BUTTON_COUNT; ++i)
    {
        draw_button(renderer, font, &FMC_PAGE_BUTTONS[i].rect, FMC_PAGE_BUTTONS[i].label, current_page == FMC_PAGE_BUTTONS[i].page);
    }

    draw_button(renderer, font, &FMC_CLR_BUTTON_RECT, "CLR", 0);

    const char *keys[] = {
        "A", "B", "C", "1",
        "D", "E", "F", "2",
        "G", "H", "I", "3",
    };

    int key_index = 0;
    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            SDL_Rect rect = {335 + col * 72, 745 + row * 50, 58, 38};
            draw_button(renderer, font, &rect, keys[key_index++], 0);
        }
    }

    draw_text(renderer, font, COLOR_DIM, 82, 790, "TEXT INPUT: KEYBOARD");
    draw_text(renderer, font, COLOR_DIM, 82, 820, "F1-F5 CHANGE PAGES");
}

void fmc_ui_render(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data)
{
    if (renderer == NULL || font == NULL || data == NULL)
    {
        return;
    }

    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    if (width <= 0 || height <= 0)
    {
        width = 700;
        height = 900;
    }

    fill_rect(renderer, &(SDL_Rect){0, 0, width, height}, COLOR_BG);
    draw_fmc_shell(renderer, font);
    draw_fmc_screen(renderer, font, data);
    draw_page_content(renderer, font, data);
    draw_scratchpad(renderer, font, data);
    draw_keypad(renderer, font, data->current_page);
}

FMC_Page fmc_ui_hit_test_page_button(int x, int y, int *hit)
{
    if (hit != NULL)
    {
        *hit = 0;
    }

    for (int i = 0; i < FMC_BUTTON_COUNT; ++i)
    {
        if (point_in_rect(x, y, &FMC_PAGE_BUTTONS[i].rect))
        {
            if (hit != NULL)
            {
                *hit = 1;
            }
            return FMC_PAGE_BUTTONS[i].page;
        }
    }

    return FMC_PAGE_INDEX;
}

int fmc_ui_hit_test_clear_button(int x, int y)
{
    return point_in_rect(x, y, &FMC_CLR_BUTTON_RECT);
}
