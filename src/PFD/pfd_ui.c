#include "pfd_ui.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#define PFD_DEG_TO_RAD 0.01745329251994329577f

static const SDL_Color COLOR_WHITE = {235, 245, 250, 255};
static const SDL_Color COLOR_CYAN = {70, 210, 255, 255};
static const SDL_Color COLOR_GREEN = {90, 255, 120, 255};
static const SDL_Color COLOR_AMBER = {255, 190, 60, 255};
static const SDL_Color COLOR_GRAY = {120, 135, 145, 255};
static const SDL_Color COLOR_DARK = {8, 14, 20, 255};
static const SDL_Color COLOR_PANEL = {14, 25, 35, 255};
static const SDL_Color COLOR_SKY = {40, 95, 155, 255};
static const SDL_Color COLOR_GROUND = {118, 70, 38, 255};

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    if (value > max_value)
    {
        return max_value;
    }

    return value;
}

static float clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    if (value > max_value)
    {
        return max_value;
    }

    return value;
}

static float normalize_heading(float heading)
{
    while (heading >= 360.0f)
    {
        heading -= 360.0f;
    }

    while (heading < 0.0f)
    {
        heading += 360.0f;
    }

    return heading;
}

static float shortest_heading_delta(float target, float current)
{
    float delta = normalize_heading(target) - normalize_heading(current);

    while (delta > 180.0f)
    {
        delta -= 360.0f;
    }

    while (delta < -180.0f)
    {
        delta += 360.0f;
    }

    return delta;
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

    char text[128];
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

    char text[128];
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

static void draw_panel(SDL_Renderer *renderer, const SDL_Rect *rect)
{
    fill_rect(renderer, rect, COLOR_PANEL);
    draw_rect(renderer, rect, COLOR_CYAN);
}

static void draw_speed_tape(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    draw_panel(renderer, &rect);
    draw_text(renderer, font, COLOR_CYAN, rect.x + 12, rect.y + 10, "SPEED");
    draw_text(renderer, font, COLOR_GRAY, rect.x + 104, rect.y + 10, "KTS");

    const int center_y = rect.y + rect.h / 2;
    const float pixels_per_knot = 4.0f;
    const int base_speed = ((int)(data->airspeed / 10.0f)) * 10;

    set_color(renderer, COLOR_GRAY);
    SDL_RenderDrawLine(renderer, rect.x + rect.w - 50, rect.y + 42, rect.x + rect.w - 50, rect.y + rect.h - 18);

    for (int i = -6; i <= 6; ++i)
    {
        const int value = base_speed + i * 10;
        const int y = center_y - (int)((value - data->airspeed) * pixels_per_knot);
        if (y < rect.y + 50 || y > rect.y + rect.h - 20)
        {
            continue;
        }

        set_color(renderer, COLOR_WHITE);
        SDL_RenderDrawLine(renderer, rect.x + rect.w - 60, y, rect.x + rect.w - 25, y);
        draw_text(renderer, font, COLOR_WHITE, rect.x + 18, y - 11, "%03d", value);
    }

    SDL_Rect value_box = {rect.x + 42, center_y - 24, rect.w - 54, 48};
    fill_rect(renderer, &value_box, COLOR_DARK);
    draw_rect(renderer, &value_box, COLOR_WHITE);
    draw_centered_text(renderer, font, COLOR_GREEN, &value_box, "%03.0f", data->airspeed);

    set_color(renderer, COLOR_GREEN);
    SDL_RenderDrawLine(renderer, rect.x + rect.w - 21, center_y, rect.x + rect.w - 4, center_y - 10);
    SDL_RenderDrawLine(renderer, rect.x + rect.w - 21, center_y, rect.x + rect.w - 4, center_y + 10);
}

static void draw_altitude_tape(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    draw_panel(renderer, &rect);
    draw_text(renderer, font, COLOR_CYAN, rect.x + 14, rect.y + 10, "ALT");
    draw_text(renderer, font, COLOR_GRAY, rect.x + rect.w - 36, rect.y + 10, "FT");

    const int center_y = rect.y + rect.h / 2;
    const float pixels_per_foot = 0.08f;
    const int base_altitude = ((int)(data->altitude / 500.0f)) * 500;

    set_color(renderer, COLOR_GRAY);
    SDL_RenderDrawLine(renderer, rect.x + 22, rect.y + 42, rect.x + 22, rect.y + rect.h - 18);

    for (int i = -6; i <= 6; ++i)
    {
        const int value = base_altitude + i * 500;
        const int y = center_y - (int)((value - data->altitude) * pixels_per_foot);
        if (y < rect.y + 50 || y > rect.y + rect.h - 20)
        {
            continue;
        }

        set_color(renderer, COLOR_WHITE);
        SDL_RenderDrawLine(renderer, rect.x + 18, y, rect.x + 52, y);
        draw_text(renderer, font, COLOR_WHITE, rect.x + 62, y - 11, "%05d", value);
    }

    SDL_Rect value_box = {rect.x + 10, center_y - 24, rect.w - 20, 48};
    fill_rect(renderer, &value_box, COLOR_DARK);
    draw_rect(renderer, &value_box, COLOR_WHITE);
    draw_centered_text(renderer, font, COLOR_GREEN, &value_box, "%05.0f", data->altitude);
}

static void draw_attitude_indicator(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    fill_rect(renderer, &rect, COLOR_DARK);
    SDL_RenderSetClipRect(renderer, &rect);

    const int cx = rect.x + rect.w / 2;
    const int cy = rect.y + rect.h / 2;
    const float roll_rad = data->roll * PFD_DEG_TO_RAD;
    const float cos_roll = cosf(roll_rad);
    const float sin_roll = sinf(roll_rad);
    const float slope = sin_roll / (cos_roll == 0.0f ? 0.001f : cos_roll);
    const float pitch_offset = data->pitch * 7.0f;

    for (int x = rect.x; x < rect.x + rect.w; ++x)
    {
        const float horizon = (float)cy + pitch_offset + slope * (float)(x - cx);
        const int sky_end = clamp_int((int)horizon, rect.y, rect.y + rect.h);

        set_color(renderer, COLOR_SKY);
        SDL_RenderDrawLine(renderer, x, rect.y, x, sky_end);
        set_color(renderer, COLOR_GROUND);
        SDL_RenderDrawLine(renderer, x, sky_end, x, rect.y + rect.h);
    }

    const int x1 = rect.x - rect.w;
    const int x2 = rect.x + rect.w * 2;
    const int y1 = (int)((float)cy + pitch_offset + slope * (float)(x1 - cx));
    const int y2 = (int)((float)cy + pitch_offset + slope * (float)(x2 - cx));
    set_color(renderer, COLOR_WHITE);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);

    for (int pitch_mark = -15; pitch_mark <= 15; pitch_mark += 5)
    {
        if (pitch_mark == 0)
        {
            continue;
        }

        const float mark_center_x = (float)cx - sin_roll * (float)pitch_mark * 7.0f;
        const float mark_center_y = (float)cy + pitch_offset - cos_roll * (float)pitch_mark * 7.0f;
        const float half_length = (pitch_mark % 10 == 0) ? 46.0f : 28.0f;
        const int lx = (int)(mark_center_x - cos_roll * half_length);
        const int ly = (int)(mark_center_y - sin_roll * half_length);
        const int rx = (int)(mark_center_x + cos_roll * half_length);
        const int ry = (int)(mark_center_y + sin_roll * half_length);

        set_color(renderer, COLOR_WHITE);
        SDL_RenderDrawLine(renderer, lx, ly, rx, ry);
    }

    SDL_RenderSetClipRect(renderer, NULL);
    draw_rect(renderer, &rect, COLOR_CYAN);

    set_color(renderer, COLOR_AMBER);
    SDL_RenderDrawLine(renderer, cx - 82, cy, cx - 24, cy);
    SDL_RenderDrawLine(renderer, cx + 24, cy, cx + 82, cy);
    SDL_RenderDrawLine(renderer, cx - 24, cy, cx, cy + 16);
    SDL_RenderDrawLine(renderer, cx + 24, cy, cx, cy + 16);
    SDL_RenderDrawLine(renderer, cx, cy - 8, cx, cy + 8);
    SDL_RenderDrawLine(renderer, cx - 8, cy, cx + 8, cy);

    const int arc_y = rect.y + 28;
    set_color(renderer, COLOR_WHITE);
    SDL_RenderDrawLine(renderer, cx, rect.y + 10, cx - 8, rect.y + 24);
    SDL_RenderDrawLine(renderer, cx, rect.y + 10, cx + 8, rect.y + 24);
    for (int mark = -30; mark <= 30; mark += 10)
    {
        const int mark_x = cx + mark * 3;
        const int tick_h = (mark % 30 == 0) ? 16 : 10;
        SDL_RenderDrawLine(renderer, mark_x, arc_y, mark_x, arc_y + tick_h);
    }

    draw_text(renderer, font, COLOR_WHITE, rect.x + 18, rect.y + rect.h - 64, "PITCH %+04.1f", data->pitch);
    draw_text(renderer, font, COLOR_WHITE, rect.x + 18, rect.y + rect.h - 34, "ROLL  %+04.1f", data->roll);
}

static void draw_vertical_speed_indicator(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    draw_panel(renderer, &rect);
    draw_text(renderer, font, COLOR_CYAN, rect.x + 12, rect.y + 10, "VS");

    const int center_y = rect.y + rect.h / 2 + 10;
    const int scale_x = rect.x + rect.w - 42;
    const int scale_top = rect.y + 42;
    const int scale_bottom = rect.y + rect.h - 20;
    const int scale_half = (scale_bottom - scale_top) / 2;
    const float clamped_vs = clamp_float(data->vertical_speed, -2000.0f, 2000.0f);
    const int pointer_y = center_y - (int)(clamped_vs / 2000.0f * (float)scale_half);

    set_color(renderer, COLOR_GRAY);
    SDL_RenderDrawLine(renderer, scale_x, scale_top, scale_x, scale_bottom);
    SDL_RenderDrawLine(renderer, scale_x - 18, center_y, scale_x + 18, center_y);
    SDL_RenderDrawLine(renderer, scale_x - 12, scale_top, scale_x + 12, scale_top);
    SDL_RenderDrawLine(renderer, scale_x - 12, scale_bottom, scale_x + 12, scale_bottom);

    set_color(renderer, COLOR_GREEN);
    SDL_RenderDrawLine(renderer, scale_x - 32, pointer_y, scale_x + 24, pointer_y);
    SDL_RenderDrawLine(renderer, scale_x - 32, pointer_y, scale_x - 18, pointer_y - 8);
    SDL_RenderDrawLine(renderer, scale_x - 32, pointer_y, scale_x - 18, pointer_y + 8);

    draw_text(renderer, font, COLOR_GREEN, rect.x + 12, rect.y + 44, "%+05.0f", data->vertical_speed);
    draw_text(renderer, font, COLOR_GRAY, rect.x + 12, rect.y + 74, "FPM");
}

static void draw_heading_indicator(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    draw_panel(renderer, &rect);
    draw_text(renderer, font, COLOR_CYAN, rect.x + 16, rect.y + 12, "HDG");

    const int tape_y = rect.y + 60;
    const int center_x = rect.x + rect.w / 2;
    const float pixels_per_degree = 6.0f;
    const int base_heading = ((int)(data->heading / 10.0f)) * 10;

    set_color(renderer, COLOR_GRAY);
    SDL_RenderDrawLine(renderer, rect.x + 24, tape_y, rect.x + rect.w - 24, tape_y);

    for (int offset = -60; offset <= 60; offset += 10)
    {
        const float heading_value = (float)(base_heading + offset);
        const float delta = shortest_heading_delta(heading_value, data->heading);
        const int x = center_x + (int)(delta * pixels_per_degree);

        if (x < rect.x + 18 || x > rect.x + rect.w - 18)
        {
            continue;
        }

        set_color(renderer, COLOR_WHITE);
        SDL_RenderDrawLine(renderer, x, tape_y - 20, x, tape_y + 18);
        draw_text(renderer, font, COLOR_WHITE, x - 14, tape_y + 22, "%03d", (int)normalize_heading(heading_value));
    }

    set_color(renderer, COLOR_GREEN);
    SDL_RenderDrawLine(renderer, center_x, tape_y - 34, center_x, tape_y + 28);
    SDL_RenderDrawLine(renderer, center_x, tape_y - 34, center_x - 8, tape_y - 22);
    SDL_RenderDrawLine(renderer, center_x, tape_y - 34, center_x + 8, tape_y - 22);

    SDL_Rect value_box = {center_x - 58, rect.y + 8, 116, 36};
    fill_rect(renderer, &value_box, COLOR_DARK);
    draw_rect(renderer, &value_box, COLOR_WHITE);
    draw_centered_text(renderer, font, COLOR_GREEN, &value_box, "%03.0f", data->heading);
}

static void draw_throttle_indicator(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    draw_panel(renderer, &rect);
    draw_text(renderer, font, COLOR_CYAN, rect.x + 12, rect.y + 10, "THR");

    const int bar_x = rect.x + 24;
    const int bar_y = rect.y + 46;
    const int bar_w = 30;
    const int bar_h = rect.h - 68;
    const float throttle = clamp_float(data->throttle, 0.0f, 100.0f);
    const int fill_h = (int)((throttle / 100.0f) * (float)bar_h);

    SDL_Rect bar = {bar_x, bar_y, bar_w, bar_h};
    SDL_Rect fill = {bar_x + 3, bar_y + bar_h - fill_h + 3, bar_w - 6, fill_h - 6};
    if (fill.h < 0)
    {
        fill.h = 0;
    }

    fill_rect(renderer, &bar, COLOR_DARK);
    fill_rect(renderer, &fill, throttle > 82.0f ? COLOR_AMBER : COLOR_GREEN);
    draw_rect(renderer, &bar, COLOR_WHITE);

    set_color(renderer, COLOR_GRAY);
    SDL_RenderDrawLine(renderer, bar_x + bar_w + 8, bar_y, bar_x + bar_w + 24, bar_y);
    SDL_RenderDrawLine(renderer, bar_x + bar_w + 8, bar_y + bar_h / 2, bar_x + bar_w + 22, bar_y + bar_h / 2);
    SDL_RenderDrawLine(renderer, bar_x + bar_w + 8, bar_y + bar_h, bar_x + bar_w + 24, bar_y + bar_h);

    draw_text(renderer, font, COLOR_GREEN, rect.x + 74, rect.y + 48, "%03.0f%%", throttle);
    draw_text(renderer, font, COLOR_GRAY, rect.x + 74, rect.y + 78, "N1 CMD");
}

static void draw_flight_mode_annunciator(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    draw_panel(renderer, &rect);

    const int third = rect.w / 3;
    set_color(renderer, COLOR_CYAN);
    SDL_RenderDrawLine(renderer, rect.x + third, rect.y, rect.x + third, rect.y + rect.h);
    SDL_RenderDrawLine(renderer, rect.x + third * 2, rect.y, rect.x + third * 2, rect.y + rect.h);

    draw_text(renderer, font, COLOR_CYAN, rect.x + 18, rect.y + 12, "FMA");
    draw_centered_text(renderer, font, COLOR_GREEN, &(SDL_Rect){rect.x + third, rect.y, third, rect.h}, "%s", data->flight_mode);
    draw_centered_text(renderer, font, data->autopilot_on ? COLOR_GREEN : COLOR_AMBER,
                       &(SDL_Rect){rect.x + third * 2, rect.y, third, rect.h},
                       "%s", data->autopilot_on ? "AP ON" : "AP OFF");
}

void pfd_ui_render(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data)
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

    fill_rect(renderer, &(SDL_Rect){0, 0, width, height}, COLOR_DARK);

    const SDL_Rect fma_rect = {30, 20, 940, 58};
    const SDL_Rect speed_rect = {30, 105, 165, 405};
    const SDL_Rect attitude_rect = {220, 95, 560, 430};
    const SDL_Rect altitude_rect = {805, 105, 165, 405};
    const SDL_Rect throttle_rect = {30, 540, 165, 120};
    const SDL_Rect heading_rect = {220, 545, 560, 115};
    const SDL_Rect vertical_speed_rect = {805, 540, 165, 120};

    draw_flight_mode_annunciator(renderer, font, data, fma_rect);
    draw_speed_tape(renderer, font, data, speed_rect);
    draw_attitude_indicator(renderer, font, data, attitude_rect);
    draw_altitude_tape(renderer, font, data, altitude_rect);
    draw_throttle_indicator(renderer, font, data, throttle_rect);
    draw_heading_indicator(renderer, font, data, heading_rect);
    draw_vertical_speed_indicator(renderer, font, data, vertical_speed_rect);
}
