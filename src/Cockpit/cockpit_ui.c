#include "cockpit_ui.h"

#include <stdarg.h>
#include <stdio.h>

static const SDL_Color COLOR_BG = {5, 8, 12, 255};
static const SDL_Color COLOR_PANEL = {14, 24, 31, 255};
static const SDL_Color COLOR_CARD = {20, 34, 42, 255};
static const SDL_Color COLOR_TEXT = {232, 242, 240, 255};
static const SDL_Color COLOR_DIM = {120, 144, 145, 255};
static const SDL_Color COLOR_CYAN = {70, 210, 255, 255};
static const SDL_Color COLOR_GREEN = {90, 255, 135, 255};
static const SDL_Color COLOR_AMBER = {255, 190, 65, 255};

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

    char text[160];
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

    char text[160];
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

static void draw_panel(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const char *title)
{
    fill_rect(renderer, rect, COLOR_PANEL);
    draw_rect(renderer, rect, COLOR_CYAN);
    draw_text(renderer, font, COLOR_CYAN, rect->x + 18, rect->y + 14, "%s", title);
}

static void draw_module_card(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const char *module_name)
{
    fill_rect(renderer, rect, COLOR_CARD);
    draw_rect(renderer, rect, COLOR_GREEN);
    draw_text(renderer, font, COLOR_CYAN, rect->x + 20, rect->y + 18, "%s", module_name);
    draw_text(renderer, font, COLOR_GREEN, rect->x + 20, rect->y + 56, "READY");
}

static void draw_shortcuts(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect)
{
    draw_panel(renderer, font, rect, "SHORTCUTS");

    const char *items[] = {
        "0  OVERVIEW",
        "1  PFD",
        "2  ND",
        "3  EICAS",
        "4  FMC",
        "ESC EXIT",
    };

    for (int i = 0; i < 6; ++i)
    {
        draw_text(renderer, font, i == 5 ? COLOR_AMBER : COLOR_TEXT, rect->x + 28, rect->y + 58 + i * 38, "%s", items[i]);
    }
}

static void draw_flight_summary(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const SDL_Rect *rect,
    float speed,
    float altitude,
    float heading,
    float fuel,
    const char *active_waypoint)
{
    draw_panel(renderer, font, rect, "FLIGHT SUMMARY");

    const char *waypoint = active_waypoint != NULL ? active_waypoint : "--";
    const int label_x = rect->x + 34;
    const int value_x = rect->x + 205;
    int y = rect->y + 64;

    draw_text(renderer, font, COLOR_DIM, label_x, y, "SPD");
    draw_text(renderer, font, COLOR_GREEN, value_x, y, "%03.0f KT", speed);

    y += 48;
    draw_text(renderer, font, COLOR_DIM, label_x, y, "ALT");
    draw_text(renderer, font, COLOR_GREEN, value_x, y, "%05.0f FT", altitude);

    y += 48;
    draw_text(renderer, font, COLOR_DIM, label_x, y, "HDG");
    draw_text(renderer, font, COLOR_GREEN, value_x, y, "%03.0f DEG", heading);

    y += 48;
    draw_text(renderer, font, COLOR_DIM, label_x, y, "FUEL");
    draw_text(renderer, font, fuel < 20.0f ? COLOR_AMBER : COLOR_GREEN, value_x, y, "%05.1f%%", fuel);

    y += 48;
    draw_text(renderer, font, COLOR_DIM, label_x, y, "ACTIVE WPT");
    draw_text(renderer, font, COLOR_GREEN, value_x, y, "%s", waypoint);
}

void cockpit_ui_render_overview(
    SDL_Renderer *renderer,
    TTF_Font *font,
    float speed,
    float altitude,
    float heading,
    float fuel,
    const char *active_waypoint)
{
    if (renderer == NULL || font == NULL)
    {
        return;
    }

    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    if (width <= 0 || height <= 0)
    {
        width = 1400;
        height = 900;
    }

    fill_rect(renderer, &(SDL_Rect){0, 0, width, height}, COLOR_BG);

    const SDL_Rect title_rect = {60, 38, width - 120, 78};
    fill_rect(renderer, &title_rect, COLOR_PANEL);
    draw_rect(renderer, &title_rect, COLOR_CYAN);
    draw_centered_text(renderer, font, COLOR_CYAN, &title_rect, "Integrated Flight Deck");

    const int card_y = 158;
    const int card_w = (width - 180) / 4;
    draw_module_card(renderer, font, &(SDL_Rect){60, card_y, card_w, 112}, "PFD");
    draw_module_card(renderer, font, &(SDL_Rect){80 + card_w, card_y, card_w, 112}, "ND");
    draw_module_card(renderer, font, &(SDL_Rect){100 + card_w * 2, card_y, card_w, 112}, "EICAS");
    draw_module_card(renderer, font, &(SDL_Rect){120 + card_w * 3, card_y, card_w, 112}, "FMC");

    draw_shortcuts(renderer, font, &(SDL_Rect){60, 325, 420, 330});
    draw_flight_summary(renderer, font, &(SDL_Rect){520, 325, width - 580, 330}, speed, altitude, heading, fuel, active_waypoint);

    const SDL_Rect footer_rect = {60, height - 105, width - 120, 54};
    fill_rect(renderer, &footer_rect, COLOR_PANEL);
    draw_rect(renderer, &footer_rect, COLOR_DIM);
    draw_centered_text(renderer, font, COLOR_DIM, &footer_rect,
                       "Use number keys 0-4 to switch pages. FMC page keeps mouse, text input, Backspace, CLR, and F1-F5 controls.");
}
