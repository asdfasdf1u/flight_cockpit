#include "nd_ui.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#define ND_DEG_TO_RAD 0.01745329251994329577f

typedef struct ND_Layout
{
    int width;
    int height;
    int center_x;
    int aircraft_y;
    int arc_center_y;
    int arc_radius;
    int arc_top_y;
} ND_Layout;

static const SDL_Color COLOR_BLACK = {0, 0, 0, 255};
static const SDL_Color COLOR_WHITE = {236, 238, 232, 255};
static const SDL_Color COLOR_GRAY = {170, 176, 172, 255};
static const SDL_Color COLOR_GREEN = {30, 230, 45, 255};
static const SDL_Color COLOR_BLUE = {0, 70, 255, 255};
static const SDL_Color COLOR_MAGENTA = {238, 46, 210, 255};

static void set_color(SDL_Renderer *renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static void fill_rect(SDL_Renderer *renderer, const SDL_Rect *rect, SDL_Color color)
{
    set_color(renderer, color);
    SDL_RenderFillRect(renderer, rect);
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

static float normalize_signed_degrees(float degrees)
{
    while (degrees > 180.0f)
    {
        degrees -= 360.0f;
    }

    while (degrees < -180.0f)
    {
        degrees += 360.0f;
    }

    return degrees;
}

static ND_Layout build_layout(int width, int height)
{
    ND_Layout layout;
    layout.width = width;
    layout.height = height;
    layout.center_x = width / 2;
    layout.aircraft_y = height - 92;
    layout.arc_center_y = height + 8;
    layout.arc_radius = (int)((float)height * 0.86f);

    const int max_radius = (int)((float)width * 0.92f);
    if (layout.arc_radius > max_radius)
    {
        layout.arc_radius = max_radius;
    }

    if (layout.arc_radius < 280)
    {
        layout.arc_radius = 280;
    }

    if (layout.arc_center_y - layout.arc_radius < 112)
    {
        layout.arc_radius = layout.arc_center_y - 112;
    }

    layout.arc_top_y = layout.arc_center_y - layout.arc_radius;
    return layout;
}

static void arc_point(const ND_Layout *layout, float relative_degrees, int radius, int *x, int *y)
{
    const float radians = relative_degrees * ND_DEG_TO_RAD;
    *x = layout->center_x + (int)(sinf(radians) * (float)radius);
    *y = layout->arc_center_y - (int)(cosf(radians) * (float)radius);
}

static void draw_filled_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color)
{
    set_color(renderer, color);

    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            if (x * x + y * y <= radius * radius)
            {
                SDL_RenderDrawPoint(renderer, cx + x, cy + y);
            }
        }
    }
}

static void draw_circle_dot(SDL_Renderer *renderer, int cx, int cy, int circle_radius, float angle_degrees, int dot_radius)
{
    const float radians = angle_degrees * ND_DEG_TO_RAD;
    const int x = cx + (int)(sinf(radians) * (float)circle_radius);
    const int y = cy - (int)(cosf(radians) * (float)circle_radius);

    draw_filled_circle(renderer, x, y, dot_radius, COLOR_WHITE);
}

static void draw_nd_background(SDL_Renderer *renderer, const ND_Layout *layout)
{
    fill_rect(renderer, &(SDL_Rect){0, 0, layout->width, layout->height}, COLOR_BLACK);
}

static void draw_active_waypoint_info(SDL_Renderer *renderer, TTF_Font *font, const ND_Layout *layout, const ND_Data *data)
{
    char distance_text[32];
    snprintf(distance_text, sizeof(distance_text), "%04.1fNM", data->active_waypoint_distance_nm);

    const SDL_Rect name_rect = {layout->width - 180, 16, 150, 24};
    const SDL_Rect min_rect = {layout->width - 180, 42, 150, 22};
    const SDL_Rect distance_rect = {layout->width - 180, 63, 150, 22};

    draw_centered_text(renderer, font, COLOR_MAGENTA, &name_rect, "%s", data->active_waypoint_name);
    draw_centered_text(renderer, font, COLOR_WHITE, &min_rect, "MIN");
    draw_centered_text(renderer, font, COLOR_WHITE, &distance_rect, "%s", distance_text);
}

static void draw_top_status(SDL_Renderer *renderer, TTF_Font *font, const ND_Layout *layout, const ND_Data *data)
{
    draw_text(renderer, font, COLOR_WHITE, 14, 16, "GS: %03.0f TAS: %03.0f", data->ground_speed, data->true_air_speed);
    draw_text(renderer, font, COLOR_WHITE, 20, 39, "---\302\260/---");

    const int center_x = layout->center_x;
    const SDL_Rect track_box = {center_x - 48, 46, 96, 30};

    draw_text(renderer, font, COLOR_GREEN, center_x - 125, 52, "TRK");
    draw_text(renderer, font, COLOR_GREEN, center_x + 65, 52, "MAG");

    set_color(renderer, COLOR_WHITE);
    SDL_RenderDrawLine(renderer, track_box.x, track_box.y, track_box.x, track_box.y + track_box.h);
    SDL_RenderDrawLine(renderer, track_box.x, track_box.y + track_box.h, track_box.x + track_box.w, track_box.y + track_box.h);
    SDL_RenderDrawLine(renderer, track_box.x + track_box.w, track_box.y, track_box.x + track_box.w, track_box.y + track_box.h);

    draw_centered_text(renderer, font, COLOR_WHITE, &track_box, "%03d", ((int)(data->track + 0.5f)) % 360);
}

static void draw_heading_arc(SDL_Renderer *renderer, TTF_Font *font, const ND_Layout *layout, const ND_Data *data)
{
    int prev_x = 0;
    int prev_y = 0;
    arc_point(layout, -88.0f, layout->arc_radius, &prev_x, &prev_y);

    set_color(renderer, COLOR_WHITE);
    for (int degree = -87; degree <= 88; ++degree)
    {
        int x = 0;
        int y = 0;
        arc_point(layout, (float)degree, layout->arc_radius, &x, &y);
        SDL_RenderDrawLine(renderer, prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;
    }

    for (int mark = 0; mark < 360; mark += 5)
    {
        const float relative = normalize_signed_degrees((float)mark - data->track);
        if (relative < -76.0f || relative > 76.0f)
        {
            continue;
        }

        const int label_mark = mark % 10 == 0;
        const int major_mark = mark % 30 == 0;
        const int tick_length = major_mark ? 22 : (label_mark ? 16 : 9);
        int x1 = 0;
        int y1 = 0;
        int x2 = 0;
        int y2 = 0;
        arc_point(layout, relative, layout->arc_radius - tick_length, &x1, &y1);
        arc_point(layout, relative, layout->arc_radius - 2, &x2, &y2);

        set_color(renderer, COLOR_WHITE);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);

        if (label_mark)
        {
            int label_x = 0;
            int label_y = 0;
            arc_point(layout, relative, layout->arc_radius - 48, &label_x, &label_y);
            draw_centered_text(renderer, font, COLOR_WHITE, &(SDL_Rect){label_x - 20, label_y - 13, 40, 24}, "%d", mark / 10);
        }
    }

    const int pointer_y = layout->arc_top_y + 7;
    set_color(renderer, COLOR_WHITE);
    SDL_RenderDrawLine(renderer, layout->center_x - 8, pointer_y - 12, layout->center_x, pointer_y + 4);
    SDL_RenderDrawLine(renderer, layout->center_x + 8, pointer_y - 12, layout->center_x, pointer_y + 4);
}

static void draw_track_line(SDL_Renderer *renderer, const ND_Layout *layout)
{
    const int top_y = layout->arc_top_y - 22;
    const int bottom_y = layout->aircraft_y - 34;

    set_color(renderer, COLOR_GRAY);
    SDL_RenderDrawLine(renderer, layout->center_x, top_y, layout->center_x, bottom_y);

    for (int y = bottom_y - 70; y > top_y + 42; y -= 72)
    {
        SDL_RenderDrawLine(renderer, layout->center_x - 5, y, layout->center_x + 5, y);
    }
}

static void waypoint_to_screen(const ND_Layout *layout, const ND_Waypoint *waypoint, int *x, int *y)
{
    const float lateral_scale = (float)layout->arc_radius * 0.54f;
    const float forward_scale = (float)layout->arc_radius * 0.78f;

    *x = layout->center_x + (int)(waypoint->rel_x * lateral_scale);
    *y = layout->aircraft_y - (int)(waypoint->rel_y * forward_scale);
}

static void draw_waypoint_triangle(SDL_Renderer *renderer, int x, int y, SDL_Color color)
{
    set_color(renderer, color);
    SDL_RenderDrawLine(renderer, x, y - 7, x - 6, y + 6);
    SDL_RenderDrawLine(renderer, x - 6, y + 6, x + 6, y + 6);
    SDL_RenderDrawLine(renderer, x + 6, y + 6, x, y - 7);
}

static void draw_active_waypoint_symbol(SDL_Renderer *renderer, int x, int y)
{
    set_color(renderer, COLOR_MAGENTA);
    SDL_RenderDrawLine(renderer, x, y - 9, x + 9, y);
    SDL_RenderDrawLine(renderer, x + 9, y, x, y + 9);
    SDL_RenderDrawLine(renderer, x, y + 9, x - 9, y);
    SDL_RenderDrawLine(renderer, x - 9, y, x, y - 9);
}

static void draw_waypoints(SDL_Renderer *renderer, TTF_Font *font, const ND_Layout *layout, const ND_Data *data)
{
    for (int i = 0; i < data->waypoint_count; ++i)
    {
        const ND_Waypoint *waypoint = &data->waypoints[i];
        const int active = i == data->active_waypoint_index;
        int x = 0;
        int y = 0;
        waypoint_to_screen(layout, waypoint, &x, &y);

        if (active)
        {
            draw_active_waypoint_symbol(renderer, x, y);
            draw_text(renderer, font, COLOR_MAGENTA, x + 13, y - 13, "%s", waypoint->name);
        }
        else
        {
            draw_waypoint_triangle(renderer, x, y, COLOR_BLUE);
            draw_text(renderer, font, COLOR_WHITE, x + 11, y - 12, "%s", waypoint->name);
        }
    }
}

static void draw_aircraft_symbol(SDL_Renderer *renderer, const ND_Layout *layout)
{
    const int cx = layout->center_x;
    const int cy = layout->aircraft_y;
    const int center_y = cy - 34;
    const int base_y = center_y + 78;
    const int base_half_width = 34;
    const int circle_radius = 56;

    set_color(renderer, COLOR_WHITE);
    SDL_RenderDrawLine(renderer, cx, center_y, cx - base_half_width, base_y);
    SDL_RenderDrawLine(renderer, cx, center_y, cx + base_half_width, base_y);
    SDL_RenderDrawLine(renderer, cx - base_half_width, base_y, cx + base_half_width, base_y);
    SDL_RenderDrawLine(renderer, cx, center_y + 28, cx - 18, base_y - 6);
    SDL_RenderDrawLine(renderer, cx - 18, base_y - 6, cx + 18, base_y - 6);
    SDL_RenderDrawLine(renderer, cx + 18, base_y - 6, cx, center_y + 28);

    draw_circle_dot(renderer, cx, center_y, circle_radius, 0.0f, 5);
    draw_circle_dot(renderer, cx, center_y, circle_radius, 90.0f, 5);
    draw_circle_dot(renderer, cx, center_y, circle_radius, 180.0f, 5);
    draw_circle_dot(renderer, cx, center_y, circle_radius, 270.0f, 5);

    draw_circle_dot(renderer, cx, center_y, circle_radius, -58.0f, 2);
    draw_circle_dot(renderer, cx, center_y, circle_radius, -35.0f, 2);
    draw_circle_dot(renderer, cx, center_y, circle_radius, 35.0f, 2);
    draw_circle_dot(renderer, cx, center_y, circle_radius, 58.0f, 2);
    draw_circle_dot(renderer, cx, center_y, circle_radius, 122.0f, 2);
    draw_circle_dot(renderer, cx, center_y, circle_radius, 145.0f, 2);
    draw_circle_dot(renderer, cx, center_y, circle_radius, -145.0f, 2);
    draw_circle_dot(renderer, cx, center_y, circle_radius, -122.0f, 2);
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
        height = 752;
    }

    const ND_Layout layout = build_layout(width, height);

    draw_nd_background(renderer, &layout);
    draw_heading_arc(renderer, font, &layout, data);
    draw_waypoints(renderer, font, &layout, data);
    draw_track_line(renderer, &layout);
    draw_aircraft_symbol(renderer, &layout);
    draw_top_status(renderer, font, &layout, data);
    draw_active_waypoint_info(renderer, font, &layout, data);
}
