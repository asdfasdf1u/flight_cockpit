#include "cabin_ui.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#define CABIN_PI 3.14159265358979323846f

static const SDL_Color COLOR_BG = {45, 72, 96, 255};
static const SDL_Color COLOR_PANEL_TITLE = {31, 142, 237, 255};
static const SDL_Color COLOR_PANEL_BODY = {248, 248, 246, 245};
static const SDL_Color COLOR_PANEL_LINE = {176, 184, 190, 255};
static const SDL_Color COLOR_TEXT_DARK = {36, 42, 48, 255};
static const SDL_Color COLOR_WHITE = {255, 255, 255, 255};
static const SDL_Color COLOR_ROUTE = {24, 137, 235, 255};
static const SDL_Color COLOR_ROUTE_SOFT = {126, 188, 242, 255};
static const SDL_Color COLOR_GREEN = {68, 176, 68, 255};
static const SDL_Color COLOR_BLACK_OVERLAY = {16, 26, 35, 205};

typedef struct Cabin_Point
{
    int x;
    int y;
} Cabin_Point;

typedef struct Cabin_Geo_Point
{
    double lat;
    double lon;
} Cabin_Geo_Point;

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

    char text[256];
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

static void draw_thick_line(SDL_Renderer *renderer, int x1, int y1, int x2, int y2, SDL_Color color)
{
    set_color(renderer, color);
    for (int offset = -2; offset <= 2; ++offset)
    {
        SDL_RenderDrawLine(renderer, x1 + offset, y1, x2 + offset, y2);
        SDL_RenderDrawLine(renderer, x1, y1 + offset, x2, y2 + offset);
    }
}

static Cabin_Point bezier_point(Cabin_Point start, Cabin_Point control, Cabin_Point end, float t)
{
    const float u = 1.0f - t;
    Cabin_Point point;
    point.x = (int)(u * u * (float)start.x + 2.0f * u * t * (float)control.x + t * t * (float)end.x);
    point.y = (int)(u * u * (float)start.y + 2.0f * u * t * (float)control.y + t * t * (float)end.y);
    return point;
}

static double lerp_double(double start, double end, float t)
{
    return start + (end - start) * (double)t;
}

static int cabin_geo_bounds_valid(const Cabin_Data *data)
{
    return data != NULL &&
           isfinite(data->map_top_left_lat) &&
           isfinite(data->map_top_left_lon) &&
           isfinite(data->map_bottom_right_lat) &&
           isfinite(data->map_bottom_right_lon) &&
           data->map_top_left_lat > data->map_bottom_right_lat &&
           data->map_bottom_right_lon > data->map_top_left_lon;
}

static int cabin_geo_point_valid(double lat, double lon)
{
    return isfinite(lat) && isfinite(lon) &&
           lat >= -90.0 && lat <= 90.0 &&
           lon >= -180.0 && lon <= 180.0;
}

static int cabin_geo_to_pixel(const Cabin_Data *data, const SDL_Rect *map_rect, double lat, double lon, Cabin_Point *point)
{
    if (point == NULL)
    {
        return 0;
    }

    point->x = 0;
    point->y = 0;

    if (map_rect == NULL || map_rect->w <= 0 || map_rect->h <= 0 ||
        !cabin_geo_bounds_valid(data) || !cabin_geo_point_valid(lat, lon))
    {
        return 0;
    }

    const double x_ratio = (lon - data->map_top_left_lon) /
                           (data->map_bottom_right_lon - data->map_top_left_lon);
    const double y_ratio = (data->map_top_left_lat - lat) /
                           (data->map_top_left_lat - data->map_bottom_right_lat);

    if (x_ratio < 0.0 || x_ratio > 1.0 || y_ratio < 0.0 || y_ratio > 1.0)
    {
        return 0;
    }

    point->x = map_rect->x + (int)(x_ratio * (double)map_rect->w + 0.5);
    point->y = map_rect->y + (int)(y_ratio * (double)map_rect->h + 0.5);
    return 1;
}

static Cabin_Geo_Point cabin_route_geo_point(const Cabin_Data *data, float t)
{
    Cabin_Geo_Point point;
    point.lat = lerp_double(data->origin_lat, data->destination_lat, t);
    point.lon = lerp_double(data->origin_lon, data->destination_lon, t);
    return point;
}

static void cabin_log_geo_debug(
    const Cabin_Data *data,
    const SDL_Rect *map_rect,
    Cabin_Point origin_px,
    Cabin_Point destination_px,
    Cabin_Point current_px,
    int has_origin,
    int has_destination,
    int has_current)
{
    static int printed = 0;
    if (printed)
    {
        return;
    }
    printed = 1;

    printf("Cabin Geo: map rect x=%d y=%d w=%d h=%d.\n", map_rect->x, map_rect->y, map_rect->w, map_rect->h);
    printf("Cabin Geo: bounds top-left lat=%.6f lon=%.6f, bottom-right lat=%.6f lon=%.6f.\n",
           data->map_top_left_lat,
           data->map_top_left_lon,
           data->map_bottom_right_lat,
           data->map_bottom_right_lon);
    printf("Cabin Geo: origin lat=%.6f lon=%.6f%s",
           data->origin_lat,
           data->origin_lon,
           has_origin ? "" : " (out of map range)");
    if (has_origin)
    {
        printf(" pixel=(%d,%d)", origin_px.x, origin_px.y);
    }
    printf(".\n");
    printf("Cabin Geo: destination lat=%.6f lon=%.6f%s",
           data->destination_lat,
           data->destination_lon,
           has_destination ? "" : " (out of map range)");
    if (has_destination)
    {
        printf(" pixel=(%d,%d)", destination_px.x, destination_px.y);
    }
    printf(".\n");
    printf("Cabin Geo: current lat=%.6f lon=%.6f progress=%.3f%s",
           data->current_lat,
           data->current_lon,
           data->progress,
           has_current ? "" : " (out of map range)");
    if (has_current)
    {
        printf(" pixel=(%d,%d)", current_px.x, current_px.y);
    }
    printf(".\n");

    if (!has_origin || !has_destination || !has_current)
    {
        printf("Cabin Geo: invalid/out-of-range point detected, route drawing will use fallback or skip clipped points.\n");
    }
}

static void draw_texture_cover(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *dest)
{
    int tex_w = 0;
    int tex_h = 0;
    if (texture == NULL || dest == NULL || SDL_QueryTexture(texture, NULL, NULL, &tex_w, &tex_h) != 0 || tex_w <= 0 || tex_h <= 0)
    {
        return;
    }

    const float scale_x = (float)dest->w / (float)tex_w;
    const float scale_y = (float)dest->h / (float)tex_h;
    const float scale = scale_x > scale_y ? scale_x : scale_y;
    const int src_w = (int)((float)dest->w / scale);
    const int src_h = (int)((float)dest->h / scale);
    const SDL_Rect src = {(tex_w - src_w) / 2, (tex_h - src_h) / 2, src_w, src_h};

    SDL_RenderCopy(renderer, texture, &src, dest);
}

static void draw_map_fallback(SDL_Renderer *renderer, const SDL_Rect *rect)
{
    fill_rect(renderer, rect, COLOR_BG);

    SDL_Color grid = {66, 98, 126, 255};
    set_color(renderer, grid);
    for (int x = rect->x; x < rect->x + rect->w; x += 80)
    {
        SDL_RenderDrawLine(renderer, x, rect->y, x, rect->y + rect->h);
    }
    for (int y = rect->y; y < rect->y + rect->h; y += 80)
    {
        SDL_RenderDrawLine(renderer, rect->x, y, rect->x + rect->w, y);
    }
}

static void draw_map_background(SDL_Renderer *renderer, SDL_Texture *map_texture, const SDL_Rect *map_rect)
{
    if (map_texture != NULL)
    {
        draw_texture_cover(renderer, map_texture, map_rect);
    }
    else
    {
        draw_map_fallback(renderer, map_rect);
    }
}

static void draw_panel_header(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const char *title)
{
    fill_rect(renderer, rect, COLOR_PANEL_TITLE);
    draw_text(renderer, font, COLOR_WHITE, rect->x + 12, rect->y + 8, "%s", title);
}

static void draw_panel_row(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const char *format, ...)
{
    char text[192];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    fill_rect(renderer, rect, COLOR_PANEL_BODY);
    draw_rect(renderer, rect, COLOR_PANEL_LINE);
    draw_text(renderer, font, COLOR_TEXT_DARK, rect->x + 12, rect->y + 10, "%s", text);
}

static void draw_location_panel(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data, int x, int y, int w)
{
    const int header_h = 45;
    const int row_h = 44;

    draw_panel_header(renderer, assets->title_font, &(SDL_Rect){x, y, w, header_h}, "地点信息");
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h, w, row_h}, "出发：%s", data->origin_city);
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h + row_h, w, row_h}, "到达：%s", data->destination_city);
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h + row_h * 2, w, row_h}, "当前位置：%s", data->current_city);
}

static void draw_weather_panel(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data, int x, int y, int w)
{
    const int header_h = 45;
    const int row_h = 38;

    draw_panel_header(renderer, assets->title_font, &(SDL_Rect){x, y, w, header_h}, "天气信息");
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h, w, row_h}, "天气：%s", data->weather);
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h + row_h, w, row_h}, "温度：%.1f°C", data->temperature);
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h + row_h * 2, w, row_h}, "湿度：%.1f%%", data->humidity);
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h + row_h * 3, w, row_h}, "风向：%s", data->wind_direction);
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h + row_h * 4, w, row_h}, "风力：%s", data->wind_power);
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h + row_h * 5, w, row_h}, "WEATHER SOURCE: %s", data->weather_source);
}

static void draw_zoom_button(SDL_Renderer *renderer, TTF_Font *font, SDL_Texture *texture, int x, int y, const char *fallback)
{
    const SDL_Rect rect = {x, y, 40, 40};
    if (texture != NULL)
    {
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        return;
    }

    fill_rect(renderer, &rect, COLOR_PANEL_BODY);
    draw_rect(renderer, &rect, COLOR_PANEL_LINE);
    draw_text(renderer, font, COLOR_TEXT_DARK, x + 12, y + 4, "%s", fallback);
}

static void draw_zoom_controls(SDL_Renderer *renderer, const Cabin_Assets *assets, int width, int height)
{
    const int x = width - 60;
    const int y = height - 90;
    draw_zoom_button(renderer, assets->title_font, assets->add_texture, x, y, "+");
    draw_zoom_button(renderer, assets->title_font, assets->sub_texture, x, y + 42, "-");
}

static void draw_map_source_badge(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data)
{
    const SDL_Rect badge = {16, 16, 128, 30};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    fill_rect(renderer, &badge, COLOR_BLACK_OVERLAY);
    draw_rect(renderer, &badge, COLOR_ROUTE_SOFT);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    draw_text(renderer, assets->small_font, COLOR_WHITE, badge.x + 10, badge.y + 5, "MAP: %s", data->map_source);
}

static void draw_route_fallback(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data, const SDL_Rect *map_rect)
{
    Cabin_Point start = {map_rect->x + map_rect->w * 60 / 100, map_rect->y + map_rect->h * 36 / 100};
    Cabin_Point control = {map_rect->x + map_rect->w * 50 / 100, map_rect->y + map_rect->h * 54 / 100};
    Cabin_Point end = {map_rect->x + map_rect->w * 51 / 100, map_rect->y + map_rect->h - 20};

    Cabin_Point prev = start;
    for (int i = 1; i <= 64; ++i)
    {
        const float t = (float)i / 64.0f;
        Cabin_Point current = bezier_point(start, control, end, t);
        draw_thick_line(renderer, prev.x, prev.y, current.x, current.y, COLOR_ROUTE_SOFT);
        prev = current;
    }

    prev = start;
    const int progress_steps = (int)(data->progress * 64.0f);
    for (int i = 1; i <= progress_steps; ++i)
    {
        const float t = (float)i / 64.0f;
        Cabin_Point current = bezier_point(start, control, end, t);
        draw_thick_line(renderer, prev.x, prev.y, current.x, current.y, COLOR_ROUTE);
        prev = current;
    }

    Cabin_Point plane = bezier_point(start, control, end, data->progress);
    Cabin_Point angle_from = plane;
    Cabin_Point angle_to = bezier_point(start, control, end, data->progress + 0.02f <= 1.0f ? data->progress + 0.02f : data->progress);
    if (data->progress + 0.02f > 1.0f && data->progress - 0.02f >= 0.0f)
    {
        angle_from = bezier_point(start, control, end, data->progress - 0.02f);
        angle_to = plane;
    }
    const double angle = atan2((double)(angle_to.y - angle_from.y), (double)(angle_to.x - angle_from.x)) * 180.0 / CABIN_PI + 90.0;

    draw_filled_circle(renderer, start.x, start.y, 8, COLOR_GREEN);
    draw_filled_circle(renderer, end.x, end.y, 8, COLOR_GREEN);
    draw_text(renderer, assets->small_font, COLOR_TEXT_DARK, start.x - 24, start.y + 12, "%s", data->origin_city);
    draw_text(renderer, assets->small_font, COLOR_TEXT_DARK, end.x - 24, end.y + 12, "%s", data->destination_city);

    if (assets->plane_texture != NULL)
    {
        const SDL_Rect dest = {plane.x - 20, plane.y - 20, 40, 40};
        SDL_RenderCopyEx(renderer, assets->plane_texture, NULL, &dest, angle, NULL, SDL_FLIP_NONE);
    }
    else
    {
        set_color(renderer, COLOR_ROUTE);
        SDL_RenderDrawLine(renderer, plane.x, plane.y - 13, plane.x - 9, plane.y + 10);
        SDL_RenderDrawLine(renderer, plane.x, plane.y - 13, plane.x + 9, plane.y + 10);
        SDL_RenderDrawLine(renderer, plane.x - 9, plane.y + 10, plane.x + 9, plane.y + 10);
    }
}

static void draw_route(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data, const SDL_Rect *map_rect)
{
    Cabin_Point start;
    Cabin_Point end;
    Cabin_Point plane;
    const int has_start = cabin_geo_to_pixel(data, map_rect, data->origin_lat, data->origin_lon, &start);
    const int has_end = cabin_geo_to_pixel(data, map_rect, data->destination_lat, data->destination_lon, &end);
    const int has_plane = cabin_geo_to_pixel(data, map_rect, data->current_lat, data->current_lon, &plane);

    cabin_log_geo_debug(data, map_rect, start, end, plane, has_start, has_end, has_plane);

    if (!has_start || !has_end || !has_plane)
    {
        draw_route_fallback(renderer, assets, data, map_rect);
        return;
    }

    Cabin_Point prev = start;
    for (int i = 1; i <= 64; ++i)
    {
        const float t = (float)i / 64.0f;
        const Cabin_Geo_Point geo = cabin_route_geo_point(data, t);
        Cabin_Point current;
        if (cabin_geo_to_pixel(data, map_rect, geo.lat, geo.lon, &current))
        {
            draw_thick_line(renderer, prev.x, prev.y, current.x, current.y, COLOR_ROUTE_SOFT);
            prev = current;
        }
    }

    prev = start;
    const int progress_steps = (int)(data->progress * 64.0f);
    for (int i = 1; i <= progress_steps; ++i)
    {
        const float t = (float)i / 64.0f;
        const Cabin_Geo_Point geo = cabin_route_geo_point(data, t);
        Cabin_Point current;
        if (cabin_geo_to_pixel(data, map_rect, geo.lat, geo.lon, &current))
        {
            draw_thick_line(renderer, prev.x, prev.y, current.x, current.y, COLOR_ROUTE);
            prev = current;
        }
    }

    Cabin_Point angle_from = plane;
    Cabin_Point angle_to = end;
    const float tangent_t = data->progress + 0.02f <= 1.0f ? data->progress + 0.02f : data->progress - 0.02f;
    if (tangent_t >= 0.0f && tangent_t <= 1.0f)
    {
        const Cabin_Geo_Point tangent_geo = cabin_route_geo_point(data, tangent_t);
        Cabin_Point tangent_candidate;
        if (cabin_geo_to_pixel(data, map_rect, tangent_geo.lat, tangent_geo.lon, &tangent_candidate))
        {
            if (data->progress + 0.02f <= 1.0f)
            {
                angle_to = tangent_candidate;
            }
            else
            {
                angle_from = tangent_candidate;
                angle_to = plane;
            }
        }
    }
    const double angle = atan2((double)(angle_to.y - angle_from.y), (double)(angle_to.x - angle_from.x)) * 180.0 / CABIN_PI + 90.0;

    draw_filled_circle(renderer, start.x, start.y, 8, COLOR_GREEN);
    draw_filled_circle(renderer, end.x, end.y, 8, COLOR_GREEN);
    draw_text(renderer, assets->small_font, COLOR_TEXT_DARK, start.x - 24, start.y + 12, "%s", data->origin_city);
    draw_text(renderer, assets->small_font, COLOR_TEXT_DARK, end.x - 24, end.y + 12, "%s", data->destination_city);

    if (assets->plane_texture != NULL)
    {
        const SDL_Rect dest = {plane.x - 20, plane.y - 20, 40, 40};
        SDL_RenderCopyEx(renderer, assets->plane_texture, NULL, &dest, angle, NULL, SDL_FLIP_NONE);
    }
    else
    {
        set_color(renderer, COLOR_ROUTE);
        SDL_RenderDrawLine(renderer, plane.x, plane.y - 13, plane.x - 9, plane.y + 10);
        SDL_RenderDrawLine(renderer, plane.x, plane.y - 13, plane.x + 9, plane.y + 10);
        SDL_RenderDrawLine(renderer, plane.x - 9, plane.y + 10, plane.x + 9, plane.y + 10);
    }
}

static void draw_flight_info_bar(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data, int width, int height, int panel_x)
{
    const int margin = 24;
    const int bar_h = 102;
    const SDL_Rect bar = {margin, height - bar_h - 24, panel_x - margin * 2, bar_h};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    fill_rect(renderer, &bar, COLOR_BLACK_OVERLAY);
    draw_rect(renderer, &bar, COLOR_ROUTE_SOFT);

    draw_text(renderer, assets->title_font, COLOR_WHITE, bar.x + 18, bar.y + 12, "%s  %s -> %s",
              data->flight_no,
              data->origin_city,
              data->destination_city);
    draw_text(renderer, assets->font, COLOR_WHITE, bar.x + 18, bar.y + 50, "高度 %.0fm   速度 %.0fkm/h   剩余 %.0f分钟",
              data->altitude,
              data->ground_speed,
              data->remaining_time_min);
    draw_text(renderer, assets->font, COLOR_WHITE, bar.x + 18, bar.y + 74, "出发 %s  到达 %s",
              data->origin_airport,
              data->destination_airport);

    const int progress_x = bar.x + bar.w - 285;
    const int progress_y = bar.y + 62;
    const SDL_Rect progress_bg = {progress_x, progress_y, 240, 12};
    const SDL_Rect progress_fg = {progress_x, progress_y, (int)(240.0f * data->progress), 12};
    draw_text(renderer, assets->font, COLOR_WHITE, progress_x, bar.y + 26, "飞行进度 %.0f%%", data->progress * 100.0f);
    fill_rect(renderer, &progress_bg, (SDL_Color){64, 80, 92, 255});
    fill_rect(renderer, &progress_fg, COLOR_ROUTE);
    draw_rect(renderer, &progress_bg, COLOR_WHITE);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static SDL_Rect cabin_geo_map_rect(int width, int height, int panel_x)
{
    const int margin = 24;
    const int bar_h = 102;
    const int bottom_limit = height - bar_h - margin * 2;
    SDL_Rect rect = {0, 0, panel_x - margin, bottom_limit};

    if (rect.w < 1)
    {
        rect.w = width;
    }
    if (rect.h < 1)
    {
        rect.h = height;
    }

    return rect;
}

void cabin_ui_render(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data)
{
    if (renderer == NULL || assets == NULL || data == NULL)
    {
        return;
    }

    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    if (width <= 0 || height <= 0)
    {
        width = 1600;
        height = 900;
    }

    const SDL_Rect map_rect = {0, 0, width, height};
    const int panel_w = width >= 1200 ? 208 : 190;
    const int panel_x = width - panel_w - 32;
    const SDL_Rect geo_map_rect = cabin_geo_map_rect(width, height, panel_x);

    draw_map_background(renderer, assets->map_texture, &map_rect);
    SDL_RenderSetClipRect(renderer, &geo_map_rect);
    draw_route(renderer, assets, data, &geo_map_rect);
    SDL_RenderSetClipRect(renderer, NULL);
    draw_location_panel(renderer, assets, data, panel_x, 22, panel_w);
    draw_weather_panel(renderer, assets, data, panel_x, 225, panel_w);
    draw_flight_info_bar(renderer, assets, data, width, height, panel_x);
    draw_zoom_controls(renderer, assets, width, height);
    draw_map_source_badge(renderer, assets, data);
}
