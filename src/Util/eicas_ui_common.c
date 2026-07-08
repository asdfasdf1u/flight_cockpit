#include "eicas_ui_common.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define PI_F 3.14159265358979323846f

const SDL_Color EICAS_COLOR_BG = {0, 0, 0, 255};
const SDL_Color EICAS_COLOR_DIAL = {64, 64, 64, 255};
const SDL_Color EICAS_COLOR_CYAN = {0, 205, 215, 255};
const SDL_Color EICAS_COLOR_GREEN = {45, 235, 55, 255};
const SDL_Color EICAS_COLOR_RED = {255, 40, 22, 255};
const SDL_Color EICAS_COLOR_WHITE = {245, 245, 245, 255};
const SDL_Color EICAS_COLOR_FRAME_DIM = {70, 50, 68, 255};

float eicas_ui_clamp_float(float value, float min_value, float max_value)
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

void eicas_ui_set_color(SDL_Renderer *renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static int map_x(const EICAS_Canvas *canvas, float x)
{
    return canvas->x + (int)(x * canvas->scale + 0.5f);
}

static int map_y(const EICAS_Canvas *canvas, float y)
{
    return canvas->y + (int)(y * canvas->scale + 0.5f);
}

static int map_len(const EICAS_Canvas *canvas, float value)
{
    int result = (int)(value * canvas->scale + 0.5f);
    return result < 1 ? 1 : result;
}

static SDL_Rect map_rect(const EICAS_Canvas *canvas, float x, float y, float w, float h)
{
    SDL_Rect rect = {map_x(canvas, x), map_y(canvas, y), map_len(canvas, w), map_len(canvas, h)};
    return rect;
}

void eicas_ui_fill_rect(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float x, float y, float w, float h, SDL_Color color)
{
    SDL_Rect rect = map_rect(canvas, x, y, w, h);
    eicas_ui_set_color(renderer, color);
    SDL_RenderFillRect(renderer, &rect);
}

void eicas_ui_draw_rect(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float x, float y, float w, float h, SDL_Color color)
{
    SDL_Rect rect = map_rect(canvas, x, y, w, h);
    eicas_ui_set_color(renderer, color);
    SDL_RenderDrawRect(renderer, &rect);
}

void eicas_ui_draw_line(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float x1, float y1, float x2, float y2, SDL_Color color)
{
    eicas_ui_set_color(renderer, color);
    SDL_RenderDrawLine(renderer, map_x(canvas, x1), map_y(canvas, y1), map_x(canvas, x2), map_y(canvas, y2));
}

void eicas_ui_draw_text(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, SDL_Color color, float x, float y, const char *format, ...)
{
    if (renderer == NULL || font == NULL || canvas == NULL || format == NULL)
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

    SDL_Rect dest = {
        map_x(canvas, x),
        map_y(canvas, y),
        map_len(canvas, (float)surface->w),
        map_len(canvas, (float)surface->h)};
    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void eicas_ui_draw_centered_text(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, SDL_Color color, float center_x, float y, const char *format, ...)
{
    if (renderer == NULL || font == NULL || canvas == NULL || format == NULL)
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

    (void)text_h;
    eicas_ui_draw_text(renderer, font, canvas, color, center_x - (float)text_w * 0.5f, y, "%s", text);
}

void eicas_ui_draw_value_box(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, float x, float y, float w, float h, const char *format, ...)
{
    if (renderer == NULL || font == NULL || canvas == NULL || format == NULL)
    {
        return;
    }

    char text[80];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    eicas_ui_draw_rect(renderer, canvas, x, y, w, h, EICAS_COLOR_WHITE);

    int text_w = 0;
    int text_h = 0;
    if (TTF_SizeUTF8(font, text, &text_w, &text_h) != 0)
    {
        return;
    }

    eicas_ui_draw_text(renderer, font, canvas, EICAS_COLOR_WHITE, x + (w - (float)text_w) * 0.5f, y + (h - (float)text_h) * 0.5f - 1.0f, "%s", text);
}

void eicas_ui_fill_lower_semicircle(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float cx, float cy, float radius, SDL_Color color)
{
    for (int row = 0; row <= (int)radius; ++row)
    {
        const float y = (float)row;
        const float x = sqrtf(radius * radius - y * y);
        eicas_ui_draw_line(renderer, canvas, cx - x, cy + y, cx + x, cy + y, color);
    }
}

void eicas_ui_draw_arc(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float cx, float cy, float radius, float start_deg, float end_deg, SDL_Color color)
{
    float prev_x = cx + cosf(start_deg * PI_F / 180.0f) * radius;
    float prev_y = cy + sinf(start_deg * PI_F / 180.0f) * radius;

    for (float deg = start_deg + 2.0f; deg <= end_deg + 0.1f; deg += 2.0f)
    {
        const float x = cx + cosf(deg * PI_F / 180.0f) * radius;
        const float y = cy + sinf(deg * PI_F / 180.0f) * radius;
        eicas_ui_draw_line(renderer, canvas, prev_x, prev_y, x, y, color);
        prev_x = x;
        prev_y = y;
    }
}

static void percent_gauge_point(float cx, float cy, float radius, float angle_deg, float *x, float *y)
{
    const float angle = angle_deg * PI_F / 180.0f;
    *x = cx + cosf(angle) * radius;
    *y = cy - sinf(angle) * radius;
}

static float percent_gauge_angle(float value)
{
    const float start_angle = 170.0f;
    const float end_angle = 360.0f;
    const float clamped = eicas_ui_clamp_float(value, 0.0f, 100.0f);
    return start_angle + (100.0f - clamped) * (end_angle - start_angle) / 100.0f;
}

static void fill_base_polygon(SDL_Renderer *renderer, const EICAS_Canvas *canvas, const float *xs, const float *ys, int count, SDL_Color color)
{
    if (count < 3)
    {
        return;
    }

    float min_y = ys[0];
    float max_y = ys[0];
    for (int i = 1; i < count; ++i)
    {
        if (ys[i] < min_y)
        {
            min_y = ys[i];
        }
        if (ys[i] > max_y)
        {
            max_y = ys[i];
        }
    }

    for (int row = (int)floorf(min_y); row <= (int)ceilf(max_y); ++row)
    {
        float nodes[32];
        int node_count = 0;
        const float scan_y = (float)row + 0.5f;

        for (int i = 0, j = count - 1; i < count; j = i++)
        {
            const float yi = ys[i];
            const float yj = ys[j];
            if ((yi < scan_y && yj >= scan_y) || (yj < scan_y && yi >= scan_y))
            {
                if (node_count < (int)(sizeof(nodes) / sizeof(nodes[0])))
                {
                    nodes[node_count++] = xs[i] + (scan_y - yi) / (yj - yi) * (xs[j] - xs[i]);
                }
            }
        }

        for (int i = 0; i < node_count - 1; ++i)
        {
            for (int j = i + 1; j < node_count; ++j)
            {
                if (nodes[i] > nodes[j])
                {
                    const float tmp = nodes[i];
                    nodes[i] = nodes[j];
                    nodes[j] = tmp;
                }
            }
        }

        for (int i = 0; i + 1 < node_count; i += 2)
        {
            eicas_ui_draw_line(renderer, canvas, nodes[i], (float)row, nodes[i + 1], (float)row, color);
        }
    }
}

static void draw_percent_tick(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float cx, float cy, float radius, float value)
{
    const float angle = percent_gauge_angle(value);
    const float tick_len = ((int)value % 20) == 0 ? 8.0f : 6.0f;
    float outer_x = 0.0f;
    float outer_y = 0.0f;
    float inner_x = 0.0f;
    float inner_y = 0.0f;

    percent_gauge_point(cx, cy, radius, angle, &outer_x, &outer_y);
    percent_gauge_point(cx, cy, radius - tick_len, angle, &inner_x, &inner_y);
    eicas_ui_draw_line(renderer, canvas, inner_x, inner_y, outer_x, outer_y, EICAS_COLOR_WHITE);
}

void eicas_ui_draw_percent_gauge_with_needle(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, float cx, float cy, float value, SDL_Color needle_color)
{
    const float radius = 60.0f;
    const float start_angle = 170.0f;
    const float end_angle = 360.0f;

    const float needle_angle = percent_gauge_angle(value);
    float fill_xs[128];
    float fill_ys[128];
    int fill_count = 0;
    fill_xs[fill_count] = cx;
    fill_ys[fill_count] = cy;
    ++fill_count;
    for (float angle = needle_angle; angle <= end_angle + 0.1f && fill_count < 128; angle += 2.0f)
    {
        float arc_x = 0.0f;
        float arc_y = 0.0f;
        percent_gauge_point(cx, cy, radius - 1.0f, angle, &arc_x, &arc_y);
        fill_xs[fill_count] = arc_x;
        fill_ys[fill_count] = arc_y;
        ++fill_count;
    }
    fill_base_polygon(renderer, canvas, fill_xs, fill_ys, fill_count, EICAS_COLOR_DIAL);

    float prev_x = 0.0f;
    float prev_y = 0.0f;
    percent_gauge_point(cx, cy, radius, start_angle, &prev_x, &prev_y);
    for (float angle = start_angle + 2.0f; angle <= end_angle + 0.1f; angle += 2.0f)
    {
        float arc_x = 0.0f;
        float arc_y = 0.0f;
        percent_gauge_point(cx, cy, radius, angle, &arc_x, &arc_y);
        eicas_ui_draw_line(renderer, canvas, prev_x, prev_y, arc_x, arc_y, EICAS_COLOR_WHITE);
        prev_x = arc_x;
        prev_y = arc_y;
    }

    for (int tick = 0; tick <= 100; tick += 10)
    {
        draw_percent_tick(renderer, canvas, cx, cy, radius, (float)tick);
    }

    for (int label = 0; label <= 100; label += 20)
    {
        float label_x = 0.0f;
        float label_y = 0.0f;
        percent_gauge_point(cx, cy, radius - 25.0f, percent_gauge_angle((float)label), &label_x, &label_y);
        if (label == 100)
        {
            label_y -= 8.0f;
        }
        eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_WHITE, label_x, label_y - 9.0f, "%d", label);
    }

    float needle_x = 0.0f;
    float needle_y = 0.0f;
    percent_gauge_point(cx, cy, radius + 2.0f, needle_angle, &needle_x, &needle_y);
    eicas_ui_draw_line(renderer, canvas, cx, cy, needle_x, needle_y, needle_color);
    eicas_ui_draw_line(renderer, canvas, cx - 1.0f, cy, needle_x - 1.0f, needle_y, needle_color);
}

void eicas_ui_draw_percent_gauge(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, float cx, float cy, float value)
{
    eicas_ui_draw_percent_gauge_with_needle(renderer, font, canvas, cx, cy, value, EICAS_COLOR_RED);
}

void eicas_ui_draw_percent_gauge_end_line(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float cx, float cy, SDL_Color color)
{
    const float radius = 60.0f;
    const float end_angle = 170.0f;
    float inner_x = 0.0f;
    float inner_y = 0.0f;
    float outer_x = 0.0f;
    float outer_y = 0.0f;

    percent_gauge_point(cx, cy, radius, end_angle, &inner_x, &inner_y);
    percent_gauge_point(cx, cy, radius + 5.0f, end_angle, &outer_x, &outer_y);
    eicas_ui_draw_line(renderer, canvas, inner_x, inner_y, outer_x, outer_y, color);
}

void eicas_ui_draw_percent_gauge_end_y(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, float cx, float cy)
{
    const float radius = 76.0f;
    const float end_angle = 170.0f;
    float stem_x = 0.0f;
    float stem_y = 0.0f;
    float fork_x = 0.0f;
    float fork_y = 0.0f;
    float upper_x = 0.0f;
    float upper_y = 0.0f;
    float lower_x = 0.0f;
    float lower_y = 0.0f;

    (void)font;
    percent_gauge_point(cx, cy, 60.0f, end_angle, &stem_x, &stem_y);
    percent_gauge_point(cx, cy, radius - 8.0f, end_angle, &fork_x, &fork_y);
    percent_gauge_point(fork_x, fork_y, 4.0f, end_angle + 32.0f, &upper_x, &upper_y);
    percent_gauge_point(fork_x, fork_y, 4.0f, end_angle - 32.0f, &lower_x, &lower_y);
    eicas_ui_draw_line(renderer, canvas, stem_x, stem_y, fork_x, fork_y, EICAS_COLOR_GREEN);
    eicas_ui_draw_line(renderer, canvas, fork_x, fork_y, upper_x, upper_y, EICAS_COLOR_GREEN);
    eicas_ui_draw_line(renderer, canvas, fork_x, fork_y, lower_x, lower_y, EICAS_COLOR_GREEN);
}

void eicas_ui_draw_egt_gauge_with_needle(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float cx, float cy, float egt, SDL_Color needle_color)
{
    const float radius = 60.0f;
    const float start_angle = 170.0f;
    const float end_angle = 360.0f;
    float fill_xs[128];
    float fill_ys[128];
    int fill_count = 0;
    const float value = eicas_ui_clamp_float(egt, 500.0f, 900.0f);
    const float ratio = (value - 500.0f) / 400.0f;
    const float needle_angle = 220.0f - ratio * 50.0f;

    fill_xs[fill_count] = cx;
    fill_ys[fill_count] = cy;
    ++fill_count;
    for (float angle = needle_angle; angle <= end_angle && fill_count < 128; angle += 2.0f)
    {
        float arc_x = 0.0f;
        float arc_y = 0.0f;
        percent_gauge_point(cx, cy, radius - 1.0f, angle, &arc_x, &arc_y);
        fill_xs[fill_count] = arc_x;
        fill_ys[fill_count] = arc_y;
        ++fill_count;
    }
    fill_base_polygon(renderer, canvas, fill_xs, fill_ys, fill_count, EICAS_COLOR_DIAL);

    float prev_x = 0.0f;
    float prev_y = 0.0f;
    percent_gauge_point(cx, cy, radius, start_angle, &prev_x, &prev_y);
    for (float angle = start_angle + 2.0f; angle <= end_angle; angle += 2.0f)
    {
        float arc_x = 0.0f;
        float arc_y = 0.0f;
        percent_gauge_point(cx, cy, radius, angle, &arc_x, &arc_y);
        eicas_ui_draw_line(renderer, canvas, prev_x, prev_y, arc_x, arc_y, EICAS_COLOR_WHITE);
        prev_x = arc_x;
        prev_y = arc_y;
    }

    {
        float outer_x = 0.0f;
        float outer_y = 0.0f;
        float inner_x = 0.0f;
        float inner_y = 0.0f;
        percent_gauge_point(cx, cy, radius + 5.0f, start_angle, &outer_x, &outer_y);
        percent_gauge_point(cx, cy, radius, start_angle, &inner_x, &inner_y);
        eicas_ui_draw_line(renderer, canvas, inner_x, inner_y, outer_x, outer_y, EICAS_COLOR_RED);
    }

    float needle_x = 0.0f;
    float needle_y = 0.0f;
    percent_gauge_point(cx, cy, radius + 2.0f, needle_angle, &needle_x, &needle_y);
    eicas_ui_draw_line(renderer, canvas, cx, cy, needle_x, needle_y, needle_color);
    eicas_ui_draw_line(renderer, canvas, cx - 1.0f, cy, needle_x - 1.0f, needle_y, needle_color);
}

void eicas_ui_draw_egt_gauge(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float cx, float cy, float egt)
{
    eicas_ui_draw_egt_gauge_with_needle(renderer, canvas, cx, cy, egt, EICAS_COLOR_RED);
}

static void draw_scale_triangle(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float x, float y, float direction)
{
    const float height = 7.0f;
    const float side = 6.1f;
    const float apex_x = x;
    const float base_x = x + direction * side;
    const float top_y = y - height * 0.5f;
    const float bottom_y = y + height * 0.5f;

    for (int row = 0; row <= (int)height; ++row)
    {
        const float distance_from_center = fabsf((float)row - height * 0.5f) / (height * 0.5f);
        const float inner_x = apex_x + direction * side * distance_from_center;
        eicas_ui_draw_line(renderer, canvas, inner_x, top_y + (float)row, base_x, top_y + (float)row, EICAS_COLOR_WHITE);
    }
    eicas_ui_draw_line(renderer, canvas, base_x, top_y, base_x, bottom_y, EICAS_COLOR_WHITE);
}

void eicas_ui_draw_vertical_scale(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float x, float top, float bottom, float value, float max_value, int ticks_left)
{
    eicas_ui_draw_vertical_scale_with_limit(renderer, canvas, x, top, bottom, value, max_value, ticks_left, 0.22f, 1);
}

void eicas_ui_draw_vertical_scale_with_limit(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float x, float top, float bottom, float value, float max_value, int ticks_left, float limit_ratio, int limit_line_count)
{
    const float clamped = eicas_ui_clamp_float(value / max_value, 0.0f, 1.0f);
    const float marker_y = bottom - (bottom - top) * clamped;
    const float tick_dir = ticks_left ? -1.0f : 1.0f;
    const float triangle_dir = tick_dir;

    eicas_ui_draw_line(renderer, canvas, x, top, x, bottom, EICAS_COLOR_WHITE);
    draw_scale_triangle(renderer, canvas, x, marker_y, triangle_dir);

    if (limit_line_count > 0)
    {
        const float limit = bottom - (bottom - top) * eicas_ui_clamp_float(limit_ratio, 0.0f, 1.0f);
        for (int i = 0; i < limit_line_count; ++i)
        {
            eicas_ui_draw_line(renderer, canvas, x, limit + (float)i * 4.0f, x + tick_dir * 8.0f, limit + (float)i * 4.0f, EICAS_COLOR_RED);
        }
    }
}

EICAS_Canvas eicas_ui_begin_frame(SDL_Renderer *renderer)
{
    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    if (width <= 0 || height <= 0)
    {
        width = (int)EICAS_BASE_SIZE;
        height = (int)EICAS_BASE_SIZE;
    }

    SDL_Rect screen = {0, 0, width, height};
    eicas_ui_set_color(renderer, EICAS_COLOR_BG);
    SDL_RenderFillRect(renderer, &screen);

    const int canvas_size = width < height ? width : height;
    EICAS_Canvas canvas = {
        (float)canvas_size / EICAS_BASE_SIZE,
        (width - canvas_size) / 2,
        (height - canvas_size) / 2};

    eicas_ui_fill_rect(renderer, &canvas, 0.0f, 0.0f, EICAS_BASE_SIZE, EICAS_BASE_SIZE, EICAS_COLOR_BG);
    return canvas;
}
