#include "nd_ui.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#define ND_DEG_TO_RAD 0.01745329251994329577f

static const SDL_Color COLOR_BG = {5, 10, 14, 255};
static const SDL_Color COLOR_PANEL = {11, 23, 28, 255};
static const SDL_Color COLOR_GRID = {24, 100, 82, 255};
static const SDL_Color COLOR_TEXT = {225, 242, 238, 255};
static const SDL_Color COLOR_GREEN = {90, 255, 135, 255};
static const SDL_Color COLOR_CYAN = {70, 210, 255, 255};
static const SDL_Color COLOR_MAGENTA = {255, 95, 230, 255};
static const SDL_Color COLOR_AMBER = {255, 185, 65, 255};
static const SDL_Color COLOR_DIM = {95, 120, 118, 255};

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

static void draw_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color)
{
    set_color(renderer, color);
    int prev_x = cx + radius;
    int prev_y = cy;

    for (int degree = 1; degree <= 360; ++degree)
    {
        const float radians = (float)degree * ND_DEG_TO_RAD;
        const int x = cx + (int)(cosf(radians) * (float)radius);
        const int y = cy + (int)(sinf(radians) * (float)radius);
        SDL_RenderDrawLine(renderer, prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;
    }
}

static void draw_dashed_line(SDL_Renderer *renderer, int x1, int y1, int x2, int y2, SDL_Color color)
{
    set_color(renderer, color);

    const int segments = 18;
    for (int i = 0; i < segments; i += 2)
    {
        const float a0 = (float)i / (float)segments;
        const float a1 = (float)(i + 1) / (float)segments;
        const int sx = x1 + (int)((float)(x2 - x1) * a0);
        const int sy = y1 + (int)((float)(y2 - y1) * a0);
        const int ex = x1 + (int)((float)(x2 - x1) * a1);
        const int ey = y1 + (int)((float)(y2 - y1) * a1);
        SDL_RenderDrawLine(renderer, sx, sy, ex, ey);
    }
}

static int waypoint_screen_x(const SDL_Rect *map_rect, int radius, const ND_Waypoint *waypoint)
{
    return map_rect->x + map_rect->w / 2 + (int)(waypoint->rel_x * (float)radius);
}

static int waypoint_screen_y(const SDL_Rect *map_rect, int radius, const ND_Waypoint *waypoint)
{
    return map_rect->y + map_rect->h / 2 + (int)(waypoint->rel_y * (float)radius);
}

static void draw_nd_background(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *map_rect)
{
    fill_rect(renderer, map_rect, COLOR_PANEL);
    draw_rect(renderer, map_rect, COLOR_CYAN);
    draw_text(renderer, font, COLOR_CYAN, map_rect->x + 18, map_rect->y + 14, "ND - Navigation Display");
}

static void draw_range_rings(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *map_rect, int radius)
{
    const int cx = map_rect->x + map_rect->w / 2;
    const int cy = map_rect->y + map_rect->h / 2;

    draw_circle(renderer, cx, cy, radius, COLOR_GRID);
    draw_circle(renderer, cx, cy, radius * 2 / 3, COLOR_GRID);
    draw_circle(renderer, cx, cy, radius / 3, COLOR_GRID);

    set_color(renderer, COLOR_GRID);
    SDL_RenderDrawLine(renderer, cx - radius, cy, cx + radius, cy);
    SDL_RenderDrawLine(renderer, cx, cy - radius, cx, cy + radius);

    draw_text(renderer, font, COLOR_DIM, cx + radius / 3 + 8, cy - 18, "20");
    draw_text(renderer, font, COLOR_DIM, cx + radius * 2 / 3 + 8, cy - 18, "40");
    draw_text(renderer, font, COLOR_DIM, cx + radius + 8, cy - 18, "60 NM");
}

static void draw_aircraft_symbol(SDL_Renderer *renderer, const SDL_Rect *map_rect)
{
    const int cx = map_rect->x + map_rect->w / 2;
    const int cy = map_rect->y + map_rect->h / 2;

    set_color(renderer, COLOR_TEXT);
    SDL_RenderDrawLine(renderer, cx, cy - 24, cx - 12, cy + 18);
    SDL_RenderDrawLine(renderer, cx, cy - 24, cx + 12, cy + 18);
    SDL_RenderDrawLine(renderer, cx - 12, cy + 18, cx + 12, cy + 18);
    SDL_RenderDrawLine(renderer, cx - 36, cy + 2, cx - 10, cy + 2);
    SDL_RenderDrawLine(renderer, cx + 10, cy + 2, cx + 36, cy + 2);
    SDL_RenderDrawLine(renderer, cx, cy + 18, cx, cy + 34);
}

static void draw_route(SDL_Renderer *renderer, const SDL_Rect *map_rect, int radius, const ND_Data *data)
{
    const int cx = map_rect->x + map_rect->w / 2;
    const int cy = map_rect->y + map_rect->h / 2;

    if (data->waypoint_count <= 0)
    {
        return;
    }

    int previous_x = cx;
    int previous_y = cy;
    for (int i = 0; i < data->waypoint_count; ++i)
    {
        const ND_Waypoint *waypoint = &data->waypoints[i];
        const int x = waypoint_screen_x(map_rect, radius, waypoint);
        const int y = waypoint_screen_y(map_rect, radius, waypoint);
        draw_dashed_line(renderer, previous_x, previous_y, x, y, i == data->active_waypoint_index ? COLOR_MAGENTA : COLOR_GREEN);
        previous_x = x;
        previous_y = y;
    }
}

static void draw_waypoints(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *map_rect, int radius, const ND_Data *data)
{
    for (int i = 0; i < data->waypoint_count; ++i)
    {
        const ND_Waypoint *waypoint = &data->waypoints[i];
        const int x = waypoint_screen_x(map_rect, radius, waypoint);
        const int y = waypoint_screen_y(map_rect, radius, waypoint);
        const int active = i == data->active_waypoint_index;

        SDL_Rect box = {x - 6, y - 6, 12, 12};
        draw_rect(renderer, &box, active ? COLOR_MAGENTA : COLOR_GREEN);
        if (active)
        {
            SDL_Rect outer = {x - 10, y - 10, 20, 20};
            draw_rect(renderer, &outer, COLOR_MAGENTA);
        }

        set_color(renderer, active ? COLOR_MAGENTA : COLOR_GREEN);
        SDL_RenderDrawLine(renderer, x - 12, y, x - 6, y);
        SDL_RenderDrawLine(renderer, x + 6, y, x + 12, y);
        SDL_RenderDrawLine(renderer, x, y - 12, x, y - 6);
        SDL_RenderDrawLine(renderer, x, y + 6, x, y + 12);

        draw_text(renderer, font, active ? COLOR_MAGENTA : COLOR_GREEN, x + 14, y - 15, "%s", waypoint->name);
        draw_text(renderer, font, COLOR_DIM, x + 14, y + 8, "%.0fNM %03.0f", waypoint->distance_nm, waypoint->bearing_deg);
    }
}

static void draw_compass_marks(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *map_rect, int radius, const ND_Data *data)
{
    const int cx = map_rect->x + map_rect->w / 2;
    const int cy = map_rect->y + map_rect->h / 2;

    for (int mark = 0; mark < 360; mark += 30)
    {
        const float relative = ((float)mark - data->heading) * ND_DEG_TO_RAD - 1.5707963f;
        const int x1 = cx + (int)(cosf(relative) * (float)(radius - 16));
        const int y1 = cy + (int)(sinf(relative) * (float)(radius - 16));
        const int x2 = cx + (int)(cosf(relative) * (float)(radius - 2));
        const int y2 = cy + (int)(sinf(relative) * (float)(radius - 2));

        set_color(renderer, COLOR_TEXT);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);

        const int tx = cx + (int)(cosf(relative) * (float)(radius - 38));
        const int ty = cy + (int)(sinf(relative) * (float)(radius - 38));
        draw_text(renderer, font, COLOR_TEXT, tx - 15, ty - 12, "%02d", mark / 10);
    }
}

static void draw_status_bar(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const ND_Data *data)
{
    fill_rect(renderer, rect, COLOR_PANEL);
    draw_rect(renderer, rect, COLOR_CYAN);

    draw_text(renderer, font, COLOR_CYAN, rect->x + 20, rect->y + 15, "HDG");
    draw_text(renderer, font, COLOR_GREEN, rect->x + 72, rect->y + 15, "%03.0f", data->heading);

    draw_text(renderer, font, COLOR_CYAN, rect->x + 165, rect->y + 15, "TRK");
    draw_text(renderer, font, COLOR_GREEN, rect->x + 217, rect->y + 15, "%03.0f", data->track);

    draw_text(renderer, font, COLOR_CYAN, rect->x + 310, rect->y + 15, "GS");
    draw_text(renderer, font, COLOR_GREEN, rect->x + 352, rect->y + 15, "%03.0f KT", data->ground_speed);

    draw_text(renderer, font, COLOR_CYAN, rect->x + 500, rect->y + 15, "LAT");
    draw_text(renderer, font, COLOR_TEXT, rect->x + 550, rect->y + 15, "%.5f", data->latitude);

    draw_text(renderer, font, COLOR_CYAN, rect->x + 710, rect->y + 15, "LON");
    draw_text(renderer, font, COLOR_TEXT, rect->x + 760, rect->y + 15, "%.5f", data->longitude);

    if (data->active_waypoint_index >= 0 && data->active_waypoint_index < data->waypoint_count)
    {
        const ND_Waypoint *active = &data->waypoints[data->active_waypoint_index];
        draw_text(renderer, font, COLOR_AMBER, rect->x + 20, rect->y + 48, "ACTIVE");
        draw_text(renderer, font, COLOR_MAGENTA, rect->x + 100, rect->y + 48, "%s", active->name);
        draw_text(renderer, font, COLOR_TEXT, rect->x + 190, rect->y + 48, "%.1f NM / %03.0f DEG", active->distance_nm, active->bearing_deg);
    }
}

void nd_ui_render(SDL_Renderer *renderer, TTF_Font *font, const ND_Data *data)
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
        width = 1000;
        height = 700;
    }

    fill_rect(renderer, &(SDL_Rect){0, 0, width, height}, COLOR_BG);

    const SDL_Rect map_rect = {80, 30, 840, 545};
    const SDL_Rect status_rect = {80, 595, 840, 82};
    const int radius = 248;

    draw_nd_background(renderer, font, &map_rect);
    draw_range_rings(renderer, font, &map_rect, radius);
    draw_compass_marks(renderer, font, &map_rect, radius, data);
    draw_route(renderer, &map_rect, radius, data);
    draw_waypoints(renderer, font, &map_rect, radius, data);
    draw_aircraft_symbol(renderer, &map_rect);
    draw_centered_text(renderer, font, COLOR_AMBER, &(SDL_Rect){map_rect.x + map_rect.w / 2 - 60, map_rect.y + 45, 120, 34}, "HDG %03.0f", data->heading);
    draw_status_bar(renderer, font, &status_rect, data);
}
