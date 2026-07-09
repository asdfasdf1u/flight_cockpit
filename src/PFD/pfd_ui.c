#include "pfd_ui.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PFD_DEG_TO_RAD 0.01745329251994329577f
#define TEXT_CACHE_SIZE 192

static const SDL_Color COLOR_BLACK = {0, 0, 0, 255};
static const SDL_Color COLOR_WHITE = {236, 244, 248, 255};
static const SDL_Color COLOR_GREEN = {75, 255, 105, 255};
static const SDL_Color COLOR_MAGENTA = {230, 55, 220, 255};
static const SDL_Color COLOR_AMBER = {255, 190, 55, 255};
static const SDL_Color COLOR_GRAY = {120, 130, 138, 255};
static const SDL_Color COLOR_DARK = {3, 5, 7, 255};
static const SDL_Color COLOR_SKY = {36, 103, 174, 255};
static const SDL_Color COLOR_GROUND = {119, 73, 35, 255};

typedef struct
{
    SDL_Renderer *renderer;
    TTF_Font *font;
    SDL_Color color;
    char text[96];
    SDL_Texture *texture;
    int w;
    int h;
    unsigned int age;
} TextCacheEntry;

static TextCacheEntry text_cache[TEXT_CACHE_SIZE];
static unsigned int text_cache_clock = 1;

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

static int same_color(SDL_Color a, SDL_Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static void set_color(SDL_Renderer *renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static void fill_rect(SDL_Renderer *renderer, SDL_Rect rect, SDL_Color color)
{
    set_color(renderer, color);
    SDL_RenderFillRect(renderer, &rect);
}

static void draw_rect(SDL_Renderer *renderer, SDL_Rect rect, SDL_Color color)
{
    set_color(renderer, color);
    SDL_RenderDrawRect(renderer, &rect);
}

static void fill_rounded_rect(SDL_Renderer *renderer, SDL_Rect rect, int radius, SDL_Color color)
{
    radius = radius < 0 ? 0 : radius;
    if (radius * 2 > rect.w)
    {
        radius = rect.w / 2;
    }
    if (radius * 2 > rect.h)
    {
        radius = rect.h / 2;
    }

    set_color(renderer, color);
    SDL_RenderFillRect(renderer, &(SDL_Rect){rect.x + radius, rect.y, rect.w - radius * 2, rect.h});
    SDL_RenderFillRect(renderer, &(SDL_Rect){rect.x, rect.y + radius, rect.w, rect.h - radius * 2});

    for (int y = 0; y < radius; ++y)
    {
        const int dx = (int)sqrtf((float)(radius * radius - y * y));
        SDL_RenderDrawLine(renderer, rect.x + radius - dx, rect.y + radius - y, rect.x + rect.w - radius + dx, rect.y + radius - y);
        SDL_RenderDrawLine(renderer, rect.x + radius - dx, rect.y + rect.h - radius + y, rect.x + rect.w - radius + dx, rect.y + rect.h - radius + y);
    }
}

static void draw_rounded_rect(SDL_Renderer *renderer, SDL_Rect rect, int radius, SDL_Color color)
{
    radius = radius < 0 ? 0 : radius;
    set_color(renderer, color);
    SDL_RenderDrawLine(renderer, rect.x + radius, rect.y, rect.x + rect.w - radius, rect.y);
    SDL_RenderDrawLine(renderer, rect.x + radius, rect.y + rect.h, rect.x + rect.w - radius, rect.y + rect.h);
    SDL_RenderDrawLine(renderer, rect.x, rect.y + radius, rect.x, rect.y + rect.h - radius);
    SDL_RenderDrawLine(renderer, rect.x + rect.w, rect.y + radius, rect.x + rect.w, rect.y + rect.h - radius);

    for (int a = 0; a <= 90; a += 4)
    {
        const float rad = (float)a * PFD_DEG_TO_RAD;
        const int dx = (int)lrintf(cosf(rad) * (float)radius);
        const int dy = (int)lrintf(sinf(rad) * (float)radius);
        SDL_RenderDrawPoint(renderer, rect.x + radius - dx, rect.y + radius - dy);
        SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + dx, rect.y + radius - dy);
        SDL_RenderDrawPoint(renderer, rect.x + radius - dx, rect.y + rect.h - radius + dy);
        SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + dx, rect.y + rect.h - radius + dy);
    }
}

static void mask_rounded_corners(SDL_Renderer *renderer, SDL_Rect rect, int radius, SDL_Color mask_color)
{
    set_color(renderer, mask_color);
    for (int y = 0; y < radius; ++y)
    {
        for (int x = 0; x < radius; ++x)
        {
            const int dx = radius - x;
            const int dy = radius - y;
            if (dx * dx + dy * dy > radius * radius)
            {
                SDL_RenderDrawPoint(renderer, rect.x + x, rect.y + y);
                SDL_RenderDrawPoint(renderer, rect.x + rect.w - 1 - x, rect.y + y);
                SDL_RenderDrawPoint(renderer, rect.x + x, rect.y + rect.h - 1 - y);
                SDL_RenderDrawPoint(renderer, rect.x + rect.w - 1 - x, rect.y + rect.h - 1 - y);
            }
        }
    }
}

static void draw_line(SDL_Renderer *renderer, int x1, int y1, int x2, int y2, SDL_Color color)
{
    set_color(renderer, color);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

static void draw_thick_line(SDL_Renderer *renderer, int x1, int y1, int x2, int y2, int thickness, SDL_Color color)
{
    const float dx = (float)(x2 - x1);
    const float dy = (float)(y2 - y1);
    const float length = sqrtf(dx * dx + dy * dy);
    if (length < 0.5f)
    {
        return;
    }

    const float nx = -dy / length;
    const float ny = dx / length;
    const int half = thickness / 2;
    set_color(renderer, color);
    for (int i = 0; i < thickness; ++i)
    {
        const float offset = (float)(i - half);
        const int ox = (int)lrintf(nx * offset);
        const int oy = (int)lrintf(ny * offset);
        SDL_RenderDrawLine(renderer, x1 + ox, y1 + oy, x2 + ox, y2 + oy);
    }
}

static void plot_aa(SDL_Renderer *renderer, int x, int y, SDL_Color color, float coverage)
{
    if (coverage <= 0.0f)
    {
        return;
    }
    if (coverage > 1.0f)
    {
        coverage = 1.0f;
    }

    SDL_Color c = color;
    c.a = (Uint8)lrintf((float)color.a * coverage);
    set_color(renderer, c);
    SDL_RenderDrawPoint(renderer, x, y);
}

static float frac_part(float value)
{
    return value - floorf(value);
}

static void draw_aa_line(SDL_Renderer *renderer, float x1, float y1, float x2, float y2, SDL_Color color)
{
    const int steep = fabsf(y2 - y1) > fabsf(x2 - x1);
    if (steep)
    {
        float temp = x1;
        x1 = y1;
        y1 = temp;
        temp = x2;
        x2 = y2;
        y2 = temp;
    }

    if (x1 > x2)
    {
        float temp = x1;
        x1 = x2;
        x2 = temp;
        temp = y1;
        y1 = y2;
        y2 = temp;
    }

    const float dx = x2 - x1;
    const float dy = y2 - y1;
    if (fabsf(dx) < 0.001f)
    {
        draw_line(renderer, (int)lrintf(steep ? y1 : x1), (int)lrintf(steep ? x1 : y1),
                  (int)lrintf(steep ? y2 : x2), (int)lrintf(steep ? x2 : y2), color);
        return;
    }

    const float gradient = dy / dx;
    float y = y1 + gradient * (ceilf(x1) - x1);
    const int start_x = (int)ceilf(x1);
    const int end_x = (int)floorf(x2);

    for (int x = start_x; x <= end_x; ++x)
    {
        const int yi = (int)floorf(y);
        const float frac = frac_part(y);
        if (steep)
        {
            plot_aa(renderer, yi, x, color, 1.0f - frac);
            plot_aa(renderer, yi + 1, x, color, frac);
        }
        else
        {
            plot_aa(renderer, x, yi, color, 1.0f - frac);
            plot_aa(renderer, x, yi + 1, color, frac);
        }
        y += gradient;
    }
}

static void draw_smooth_line(SDL_Renderer *renderer, int x1, int y1, int x2, int y2, int thickness, SDL_Color color)
{
    const float dx = (float)(x2 - x1);
    const float dy = (float)(y2 - y1);
    const float length = sqrtf(dx * dx + dy * dy);
    if (length < 0.5f)
    {
        return;
    }

    if (abs(y2 - y1) <= 1 || abs(x2 - x1) <= 1)
    {
        draw_thick_line(renderer, x1, y1, x2, y2, thickness, color);
        return;
    }

    const float nx = -dy / length;
    const float ny = dx / length;
    const int half = thickness / 2;
    for (int i = 0; i < thickness; ++i)
    {
        const float offset = (float)(i - half);
        const float ox = nx * offset;
        const float oy = ny * offset;
        draw_aa_line(renderer, (float)x1 + ox, (float)y1 + oy, (float)x2 + ox, (float)y2 + oy, color);
    }
}

static void draw_arc(SDL_Renderer *renderer, int cx, int cy, int radius, int start_deg, int end_deg, SDL_Color color)
{
    float last_x = (float)cx + cosf((float)start_deg * PFD_DEG_TO_RAD) * (float)radius;
    float last_y = (float)cy + sinf((float)start_deg * PFD_DEG_TO_RAD) * (float)radius;
    for (float deg = (float)start_deg + 0.35f; deg <= (float)end_deg + 0.01f; deg += 0.35f)
    {
        const float x = (float)cx + cosf(deg * PFD_DEG_TO_RAD) * (float)radius;
        const float y = (float)cy + sinf(deg * PFD_DEG_TO_RAD) * (float)radius;
        draw_aa_line(renderer, last_x, last_y, x, y, color);
        last_x = x;
        last_y = y;
    }
}

static void fill_polygon(SDL_Renderer *renderer, const SDL_Point *points, int count, SDL_Color color)
{
    if (points == NULL || count < 3)
    {
        return;
    }

    int min_y = points[0].y;
    int max_y = points[0].y;
    for (int i = 1; i < count; ++i)
    {
        min_y = points[i].y < min_y ? points[i].y : min_y;
        max_y = points[i].y > max_y ? points[i].y : max_y;
    }

    set_color(renderer, color);
    for (int y = min_y; y <= max_y; ++y)
    {
        float intersections[12];
        int n = 0;
        for (int i = 0; i < count; ++i)
        {
            const SDL_Point a = points[i];
            const SDL_Point b = points[(i + 1) % count];
            if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y))
            {
                intersections[n++] = (float)a.x + (float)(y - a.y) * (float)(b.x - a.x) / (float)(b.y - a.y);
                if (n >= 12)
                {
                    break;
                }
            }
        }

        for (int i = 0; i + 1 < n; i += 2)
        {
            if (intersections[i] > intersections[i + 1])
            {
                const float temp = intersections[i];
                intersections[i] = intersections[i + 1];
                intersections[i + 1] = temp;
            }
            SDL_RenderDrawLine(renderer, (int)floorf(intersections[i]), y, (int)ceilf(intersections[i + 1]), y);
        }
    }
}

static void draw_smooth_polygon_outline(SDL_Renderer *renderer, const SDL_Point *points, int count, int thickness, SDL_Color color)
{
    if (points == NULL || count < 2)
    {
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        const SDL_Point a = points[i];
        const SDL_Point b = points[(i + 1) % count];
        draw_smooth_line(renderer, a.x, a.y, b.x, b.y, thickness, color);
    }
}

static SDL_Texture *get_text_texture(SDL_Renderer *renderer, TTF_Font *font, SDL_Color color, const char *text, int *w, int *h)
{
    int oldest = 0;
    for (int i = 0; i < TEXT_CACHE_SIZE; ++i)
    {
        if (text_cache[i].texture != NULL &&
            text_cache[i].renderer == renderer &&
            text_cache[i].font == font &&
            same_color(text_cache[i].color, color) &&
            strcmp(text_cache[i].text, text) == 0)
        {
            text_cache[i].age = text_cache_clock++;
            *w = text_cache[i].w;
            *h = text_cache[i].h;
            return text_cache[i].texture;
        }

        if (text_cache[i].texture == NULL || text_cache[i].age < text_cache[oldest].age)
        {
            oldest = i;
        }
    }

    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (surface == NULL)
    {
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL)
    {
        SDL_FreeSurface(surface);
        return NULL;
    }

    if (text_cache[oldest].texture != NULL)
    {
        SDL_DestroyTexture(text_cache[oldest].texture);
    }

    text_cache[oldest].renderer = renderer;
    text_cache[oldest].font = font;
    text_cache[oldest].color = color;
    snprintf(text_cache[oldest].text, sizeof(text_cache[oldest].text), "%s", text);
    text_cache[oldest].texture = texture;
    text_cache[oldest].w = surface->w;
    text_cache[oldest].h = surface->h;
    text_cache[oldest].age = text_cache_clock++;

    *w = surface->w;
    *h = surface->h;
    SDL_FreeSurface(surface);
    return texture;
}

static void draw_text_at(SDL_Renderer *renderer, TTF_Font *font, SDL_Color color, int x, int y, const char *format, ...)
{
    char text[96];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    int w = 0;
    int h = 0;
    SDL_Texture *texture = get_text_texture(renderer, font, color, text, &w, &h);
    if (texture == NULL)
    {
        return;
    }

    SDL_Rect dest = {x, y, w, h};
    SDL_RenderCopy(renderer, texture, NULL, &dest);
}

static void draw_text_center(SDL_Renderer *renderer, TTF_Font *font, SDL_Color color, SDL_Rect rect, const char *format, ...)
{
    char text[96];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    int w = 0;
    int h = 0;
    SDL_Texture *texture = get_text_texture(renderer, font, color, text, &w, &h);
    if (texture == NULL)
    {
        return;
    }

    SDL_Rect dest = {rect.x + (rect.w - w) / 2, rect.y + (rect.h - h) / 2, w, h};
    SDL_RenderCopy(renderer, texture, NULL, &dest);
}

static void draw_panel(SDL_Renderer *renderer, SDL_Rect rect)
{
    fill_rect(renderer, rect, COLOR_BLACK);
}

static void draw_speed_bug(SDL_Renderer *renderer, int x, int y)
{
    SDL_Point bug[5] = {
        {x, y},
        {x + 12, y - 8},
        {x + 28, y - 8},
        {x + 28, y + 8},
        {x + 12, y + 8},
    };
    fill_polygon(renderer, bug, 5, COLOR_MAGENTA);
}

static void draw_left_pointer(SDL_Renderer *renderer, TTF_Font *font, SDL_Rect rect, int center_y, float value)
{
    const int x = rect.x + rect.w - 88;
    const int y = center_y - 22;
    SDL_Point outer[7] = {
        {x, y},
        {x + 62, y},
        {x + 62, y + 11},
        {x + 78, y + 22},
        {x + 62, y + 33},
        {x + 62, y + 44},
        {x, y + 44},
    };
    SDL_Point inner[7] = {
        {x + 3, y + 3},
        {x + 60, y + 3},
        {x + 60, y + 13},
        {x + 73, y + 22},
        {x + 60, y + 31},
        {x + 60, y + 41},
        {x + 3, y + 41},
    };
    fill_polygon(renderer, outer, 7, COLOR_WHITE);
    fill_polygon(renderer, inner, 7, COLOR_BLACK);
    draw_smooth_polygon_outline(renderer, outer, 7, 1, COLOR_WHITE);
    draw_smooth_polygon_outline(renderer, inner, 7, 1, COLOR_BLACK);
    draw_text_center(renderer, font, COLOR_WHITE, (SDL_Rect){x + 2, y + 3, 62, 38}, "%03.0f", value);
}

static void draw_right_pointer(SDL_Renderer *renderer, TTF_Font *font, SDL_Rect rect, int center_y, float value)
{
    const int x = rect.x + 10;
    const int y = center_y - 22;
    SDL_Point outer[7] = {
        {x, y + 22},
        {x + 16, y + 11},
        {x + 16, y},
        {x + 98, y},
        {x + 98, y + 44},
        {x + 16, y + 44},
        {x + 16, y + 33},
    };
    SDL_Point inner[7] = {
        {x + 5, y + 22},
        {x + 18, y + 13},
        {x + 18, y + 3},
        {x + 95, y + 3},
        {x + 95, y + 41},
        {x + 18, y + 41},
        {x + 18, y + 31},
    };
    fill_polygon(renderer, outer, 7, COLOR_WHITE);
    fill_polygon(renderer, inner, 7, COLOR_BLACK);
    draw_smooth_polygon_outline(renderer, outer, 7, 1, COLOR_WHITE);
    draw_smooth_polygon_outline(renderer, inner, 7, 1, COLOR_BLACK);
    draw_text_center(renderer, font, COLOR_WHITE, (SDL_Rect){x + 18, y + 3, 78, 38}, "%04.0f", value);
}

static void draw_speed_tape(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    draw_panel(renderer, rect);
    const int center_y = rect.y + rect.h / 2;
    const float pixels_per_knot = 4.4f;
    const int base = ((int)data->airspeed_current / 10) * 10;
    const float target_speed = data->airspeed_target;

    SDL_RenderSetClipRect(renderer, &rect);

    for (int value = base - 80; value <= base + 80; value += 5)
    {
        if (value < 30)
        {
            continue;
        }
        const int y = center_y - (int)lrintf((value - data->airspeed_current) * pixels_per_knot);
        if (y < rect.y + 24 || y > rect.y + rect.h - 24)
        {
            continue;
        }

        const int major = (value % 10) == 0;
        draw_line(renderer, rect.x + rect.w - (major ? 58 : 42), y, rect.x + rect.w - 22, y, COLOR_WHITE);
        if (major)
        {
            draw_text_at(renderer, font, COLOR_WHITE, rect.x + 18, y - 13, "%03d", value);
        }
    }

    const int bug_y = center_y - (int)lrintf((target_speed - data->airspeed_current) * pixels_per_knot);
    if (bug_y > rect.y + 28 && bug_y < rect.y + rect.h - 24)
    {
        draw_speed_bug(renderer, rect.x + rect.w - 24, bug_y);
    }
    SDL_RenderSetClipRect(renderer, NULL);

    draw_text_at(renderer, font, COLOR_MAGENTA, rect.x + 14, rect.y - 28, "%03.0f", target_speed);
    draw_left_pointer(renderer, font, rect, center_y, data->airspeed_current);
}

static void draw_altitude_tape(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    (void)data;
    draw_panel(renderer, rect);
    SDL_RenderSetClipRect(renderer, &rect);

    const int labels[] = {3000, 2800, 2600, 2400};
    const int label_y[] = {40, 170, 300, 430};
    for (int i = 0; i < 4; ++i)
    {
        const int y = rect.y + label_y[i];
        draw_line(renderer, rect.x + 8, y, rect.x + 42, y, COLOR_WHITE);
        draw_text_at(renderer, font, COLOR_WHITE, rect.x + 46, y - 14, "%d", labels[i]);

        if (i < 3)
        {
            const int minor_y = y + 65;
            draw_line(renderer, rect.x + 8, minor_y, rect.x + 34, minor_y, COLOR_GRAY);
        }
    }

    SDL_RenderSetClipRect(renderer, NULL);

    draw_text_at(renderer, font, COLOR_MAGENTA, rect.x + rect.w - 70, rect.y - 28, "01100");
    draw_right_pointer(renderer, font, rect, rect.y + 264, 2656.0f);
}

static SDL_Point attitude_point(SDL_Rect rect, float roll_rad, float pitch_offset, float pitch_deg, float x)
{
    const float cx = (float)(rect.x + rect.w / 2);
    const float cy = (float)(rect.y + rect.h / 2);
    const float cos_roll = cosf(roll_rad);
    const float sin_roll = sinf(roll_rad);
    const float local_y = pitch_offset - pitch_deg * 7.2f;

    SDL_Point p;
    p.x = (int)lrintf(cx + x * cos_roll - local_y * sin_roll);
    p.y = (int)lrintf(cy + x * sin_roll + local_y * cos_roll);
    return p;
}

static void draw_attitude(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    const int radius = 16;
    fill_rounded_rect(renderer, rect, radius, COLOR_DARK);
    SDL_RenderSetClipRect(renderer, &rect);

    const int cx = rect.x + rect.w / 2;
    const int cy = rect.y + rect.h / 2;
    const float roll_rad = data->roll * PFD_DEG_TO_RAD;
    const float pitch_offset = data->pitch * 7.2f;
    const float span = (float)(rect.w + rect.h);

    SDL_Point left_h = attitude_point(rect, roll_rad, pitch_offset, 0.0f, -span);
    SDL_Point right_h = attitude_point(rect, roll_rad, pitch_offset, 0.0f, span);
    SDL_Point sky[4] = {{rect.x - rect.w, rect.y - rect.h}, {rect.x + rect.w * 2, rect.y - rect.h}, right_h, left_h};
    SDL_Point ground[4] = {left_h, right_h, {rect.x + rect.w * 2, rect.y + rect.h * 2}, {rect.x - rect.w, rect.y + rect.h * 2}};
    fill_polygon(renderer, sky, 4, COLOR_SKY);
    fill_polygon(renderer, ground, 4, COLOR_GROUND);

    draw_smooth_line(renderer, left_h.x, left_h.y, right_h.x, right_h.y, 3, COLOR_WHITE);

    for (int pitch_step = -120; pitch_step <= 120; pitch_step += 10)
    {
        if (pitch_step == 0)
        {
            continue;
        }

        const float pitch = (float)pitch_step / 4.0f;
        const int major = (pitch_step % 40) == 0;
        const int medium = (pitch_step % 20) == 0;
        const float half = major ? 68.0f : (medium ? 48.0f : 28.0f);
        SDL_Point a = attitude_point(rect, roll_rad, pitch_offset, pitch, -half);
        SDL_Point b = attitude_point(rect, roll_rad, pitch_offset, pitch, half);
        draw_smooth_line(renderer, a.x, a.y, b.x, b.y, major ? 2 : 1, COLOR_WHITE);
        if (major)
        {
            SDL_Point t1 = attitude_point(rect, roll_rad, pitch_offset, pitch, -half - 30.0f);
            SDL_Point t2 = attitude_point(rect, roll_rad, pitch_offset, pitch, half + 8.0f);
            draw_text_at(renderer, font, COLOR_WHITE, t1.x - 12, t1.y - 12, "%.0f", fabsf(pitch));
            draw_text_at(renderer, font, COLOR_WHITE, t2.x, t2.y - 12, "%.0f", fabsf(pitch));
        }
    }

    SDL_RenderSetClipRect(renderer, NULL);

    mask_rounded_corners(renderer, rect, radius, COLOR_DARK);
    draw_rounded_rect(renderer, rect, radius, (SDL_Color){8, 8, 8, 255});

    const int wing_outer = rect.w / 2 - 70;
    const int wing_inner = wing_outer / 3;
    SDL_Rect left_wing = {cx - wing_outer, cy - 4, wing_outer - wing_inner, 8};
    SDL_Rect left_drop = {cx - wing_inner - 8, cy - 4, 8, 22};
    SDL_Rect right_wing = {cx + wing_inner, cy - 4, wing_outer - wing_inner, 8};
    SDL_Rect right_drop = {cx + wing_inner, cy - 4, 8, 22};
    fill_rect(renderer, left_wing, COLOR_BLACK);
    fill_rect(renderer, left_drop, COLOR_BLACK);
    fill_rect(renderer, right_wing, COLOR_BLACK);
    fill_rect(renderer, right_drop, COLOR_BLACK);
    fill_rect(renderer, (SDL_Rect){cx - 5, cy - 5, 10, 10}, COLOR_BLACK);

    SDL_RenderSetClipRect(renderer, &rect);
    for (int mark = -60; mark <= 60; mark += 10)
    {
        if (mark == 0)
        {
            continue;
        }
        const float angle = (float)mark * PFD_DEG_TO_RAD;
        const int tick_len = (mark % 30 == 0) ? 20 : 12;
        const float roll_span = (float)(rect.w / 2 - 18);
        const int x1 = cx + (int)lrintf(sinf(angle) * roll_span);
        const int y1 = rect.y + 20 + (int)lrintf((1.0f - cosf(angle)) * 74.0f);
        const int x2 = cx + (int)lrintf(sinf(angle) * (roll_span - (float)tick_len));
        const int y2 = rect.y + 20 + tick_len + (int)lrintf((1.0f - cosf(angle)) * 74.0f);
        draw_smooth_line(renderer, x1, y1, x2, y2, 2, COLOR_WHITE);

    }

    draw_line(renderer, cx, rect.y + 2, cx, rect.y + 12, COLOR_WHITE);
    SDL_Point roll_pointer[3] = {{cx, rect.y + 24}, {cx - 7, rect.y + 12}, {cx + 7, rect.y + 12}};
    fill_polygon(renderer, roll_pointer, 3, COLOR_WHITE);
    SDL_RenderSetClipRect(renderer, NULL);
}

static void draw_vertical_speed(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    (void)data;
    fill_rect(renderer, rect, COLOR_BLACK);
    const int left = rect.x + 12;
    const int top = rect.y + 30;
    const int bottom = rect.y + rect.h - 20;
    const int center_y = (top + bottom) / 2;

    SDL_Point body[8] = {
        {left, top},
        {left + 30, top},
        {left + 48, top + 82},
        {left + 48, center_y - 18},
        {left + 35, center_y},
        {left + 48, center_y + 18},
        {left + 30, bottom},
        {left, bottom},
    };
    fill_polygon(renderer, body, 8, (SDL_Color){58, 60, 62, 255});
    draw_line(renderer, left, top, left, bottom, COLOR_GRAY);
    draw_text_at(renderer, font, COLOR_WHITE, rect.x + 2, rect.y + 2, "600");

    const int label_values[] = {6, 2, 1, 1, 2, 6};
    const int label_y[] = {top + 28, top + 82, top + 132, center_y + 38, center_y + 90, bottom - 30};
    for (int i = 0; i < 6; ++i)
    {
        draw_line(renderer, left + 12, label_y[i], left + 26, label_y[i], COLOR_WHITE);
        draw_text_at(renderer, font, COLOR_WHITE, left + 6, label_y[i] - 15, "%d", label_values[i]);
    }

    for (int y = top + 56; y < bottom - 20; y += 28)
    {
        draw_line(renderer, left + 14, y, left + 23, y, COLOR_GRAY);
    }

    draw_smooth_line(renderer, left + 12, center_y - 10, left + 50, center_y + 12, 1, COLOR_WHITE);
}

static void draw_heading(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    (void)data;
    fill_rect(renderer, rect, COLOR_BLACK);
    const int center_x = rect.x + rect.w / 2;
    const int center_y = rect.y + 198;
    const int radius = rect.w / 2 - 54;
    SDL_RenderSetClipRect(renderer, &rect);
    for (int offset = -60; offset <= 60; offset += 10)
    {
        const float angle = (270.0f + (float)offset) * PFD_DEG_TO_RAD;
        const int major = (offset == -40 || offset == 0 || offset == 40);
        const int r1 = radius;
        const int r2 = radius - 12;
        const int x1 = center_x + (int)lrintf(cosf(angle) * (float)r1);
        const int y1 = center_y + (int)lrintf(sinf(angle) * (float)r1);
        const int x2 = center_x + (int)lrintf(cosf(angle) * (float)r2);
        const int y2 = center_y + (int)lrintf(sinf(angle) * (float)r2);
        if (major)
        {
            if (offset != 0)
            {
                draw_smooth_line(renderer, x1, y1, x2, y2, 1, COLOR_GRAY);
            }
            if (offset == -40)
            {
                draw_text_at(renderer, font, COLOR_GRAY, x2 - 14, y2 + 8, "18");
            }
            else if (offset == 0)
            {
                draw_text_at(renderer, font, COLOR_GRAY, x2 - 12, y2 + 8, "21");
            }
            else if (offset == 40)
            {
                draw_text_at(renderer, font, COLOR_GRAY, x2 - 12, y2 + 8, "24");
            }
        }
        else
        {
            draw_aa_line(renderer, (float)x1, (float)y1, (float)x2, (float)y2, COLOR_GRAY);
        }
    }
    SDL_RenderSetClipRect(renderer, NULL);

    const int pointer_top = center_y - (radius - 12) - 5;
    SDL_Point heading_pointer[3] = {
        {center_x, pointer_top + 8},
        {center_x - 5, pointer_top},
        {center_x + 5, pointer_top},
    };
    fill_polygon(renderer, heading_pointer, 3, COLOR_GRAY);
    draw_thick_line(renderer, center_x, pointer_top + 8, center_x, rect.y + rect.h - 42, 1, COLOR_WHITE);
    fill_rect(renderer, (SDL_Rect){center_x - 95, rect.y + rect.h - 36, 190, 34}, COLOR_BLACK);
    draw_text_center(renderer, font, COLOR_MAGENTA, (SDL_Rect){center_x - 92, rect.y + rect.h - 34, 82, 30}, "20H");
    draw_text_center(renderer, font, COLOR_GREEN, (SDL_Rect){center_x + 10, rect.y + rect.h - 34, 82, 30}, "MAG");
}

static void draw_fma(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    fill_rect(renderer, rect, COLOR_BLACK);
    const int col = rect.w / 2;
    draw_line(renderer, rect.x + col, rect.y, rect.x + col, rect.y + rect.h, (SDL_Color){45, 48, 50, 255});

    (void)data;
    draw_text_center(renderer, font, COLOR_AMBER, (SDL_Rect){rect.x, rect.y + rect.h + 2, col, 28}, "CWSR");
    draw_text_center(renderer, font, COLOR_AMBER, (SDL_Rect){rect.x + col, rect.y + rect.h + 2, col, 28}, "CWSP");
}

static void draw_thrust_indicator(SDL_Renderer *renderer, TTF_Font *font, const PFD_Data *data, SDL_Rect rect)
{
    (void)data;
    fill_rect(renderer, rect, COLOR_BLACK);

    const int cx = rect.x + rect.w / 2;
    const int cy = rect.y + rect.h / 2 + 4;
    const int radius = rect.w / 2 - 10;

    draw_arc(renderer, cx, cy, radius, 225, 450, COLOR_GRAY);
    for (int mark = 0; mark <= 100; mark += 20)
    {
        const float angle = (450.0f - (float)mark * 2.25f) * PFD_DEG_TO_RAD;
        const int r1 = radius;
        const int r2 = radius - 8;
        const int x1 = cx + (int)lrintf(cosf(angle) * (float)r1);
        const int y1 = cy + (int)lrintf(sinf(angle) * (float)r1);
        const int x2 = cx + (int)lrintf(cosf(angle) * (float)r2);
        const int y2 = cy + (int)lrintf(sinf(angle) * (float)r2);
        draw_line(renderer, x1, y1, x2, y2, COLOR_GRAY);
    }

    const float pointer_angle = 450.0f * PFD_DEG_TO_RAD;
    const int pointer_x = cx + (int)lrintf(cosf(pointer_angle) * (float)(radius - 8));
    const int pointer_y = cy + (int)lrintf(sinf(pointer_angle) * (float)(radius - 8));

    draw_text_at(renderer, font, COLOR_WHITE, rect.x + 8, rect.y + rect.h - 34, "0");
    draw_thick_line(renderer, cx, cy, pointer_x, pointer_y, 2, COLOR_WHITE);
    SDL_Point arrow[3] = {
        {pointer_x, pointer_y + 8},
        {pointer_x - 5, pointer_y},
        {pointer_x + 5, pointer_y},
    };
    fill_polygon(renderer, arrow, 3, COLOR_WHITE);
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

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    fill_rect(renderer, (SDL_Rect){0, 0, width, height}, COLOR_DARK);

    const float sx = (float)width / 900.0f;
    const float sy = (float)height / 800.0f;
#define SR(x, y, w, h) (SDL_Rect){(int)lrintf((x) * sx), (int)lrintf((y) * sy), (int)lrintf((w) * sx), (int)lrintf((h) * sy)}

    draw_fma(renderer, font, data, SR(203, 31, 494, 57));
    draw_speed_tape(renderer, font, data, SR(94, 132, 104, 530));
    draw_thrust_indicator(renderer, font, data, SR(596, 126, 80, 80));
    draw_attitude(renderer, font, data, SR(225, 215, 430, 405));
    draw_altitude_tape(renderer, font, data, SR(692, 132, 114, 528));
    draw_vertical_speed(renderer, font, data, SR(831, 184, 73, 388));
    draw_heading(renderer, font, data, SR(200, 660, 500, 130));

#undef SR
}
