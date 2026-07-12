#include "cabin_ui.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define CABIN_PI 3.14159265358979323846f
#define CABIN_MAP_ZOOM_MIN 0.85f
#define CABIN_MAP_ZOOM_MAX 1.65f
#define CABIN_MAP_ZOOM_STEP 0.15f

static const SDL_Color COLOR_BG = {45, 72, 96, 255};
static const SDL_Color COLOR_PANEL_TITLE = {31, 142, 237, 255};
static const SDL_Color COLOR_PANEL_BODY = {248, 248, 246, 245};
static const SDL_Color COLOR_PANEL_LINE = {176, 184, 190, 255};
static const SDL_Color COLOR_TEXT_DARK = {36, 42, 48, 255};
static const SDL_Color COLOR_WHITE = {255, 255, 255, 255};
static const SDL_Color COLOR_ROUTE = {24, 137, 235, 255};
static const SDL_Color COLOR_ROUTE_SOFT = {126, 188, 242, 255};
static const SDL_Color COLOR_GREEN = {68, 176, 68, 255};
static const SDL_Color COLOR_BLACK_OVERLAY = {16, 26, 35, 198};
static const SDL_Color COLOR_PROGRESS_BG = {64, 80, 92, 255};
static const SDL_Color COLOR_EMERGENCY_RED = {242, 24, 35, 255};
static const SDL_Color COLOR_EMERGENCY_DARK = {70, 0, 4, 232};

static float g_map_zoom = 1.0f;
static float g_map_pan_x = 0.0f;
static float g_map_pan_y = 0.0f;
static int g_map_dragging = 0;
static int g_last_mouse_x = 0;
static int g_last_mouse_y = 0;
static int g_compact_mode = 0;

typedef struct Cabin_Point
{
    int x;
    int y;
} Cabin_Point;

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

static void draw_text_clipped(SDL_Renderer *renderer, TTF_Font *font, SDL_Color color, const SDL_Rect *clip_rect, int x, int y, const char *format, ...)
{
    if (renderer == NULL || font == NULL || clip_rect == NULL || format == NULL)
    {
        return;
    }

    char text[256];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    SDL_Rect old_clip;
    const SDL_bool had_clip = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &old_clip);
    SDL_RenderSetClipRect(renderer, clip_rect);
    draw_text(renderer, font, color, x, y, "%s", text);
    SDL_RenderSetClipRect(renderer, had_clip ? &old_clip : NULL);
}

static void draw_text_centered(SDL_Renderer *renderer, TTF_Font *font, SDL_Color color, const SDL_Rect *rect, const char *text)
{
    if (renderer == NULL || font == NULL || rect == NULL || text == NULL)
    {
        return;
    }

    int width = 0;
    int height = 0;
    if (TTF_SizeUTF8(font, text, &width, &height) != 0)
    {
        return;
    }

    const int x = rect->x + (rect->w - width) / 2;
    const int y = rect->y + (rect->h - height) / 2;
    draw_text_clipped(renderer, font, color, rect, x, y, "%s", text);
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
    for (int offset = -1; offset <= 1; ++offset)
    {
        SDL_RenderDrawLine(renderer, x1 + offset, y1, x2 + offset, y2);
        SDL_RenderDrawLine(renderer, x1, y1 + offset, x2, y2 + offset);
    }
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

static void draw_polyline_geo(SDL_Renderer *renderer,
                              const Cabin_Data *data,
                              const SDL_Rect *map_rect,
                              const Cabin_Trajectory_Point *points,
                              int point_count,
                              SDL_Color color)
{
    Cabin_Point previous = {0, 0};
    int has_previous = 0;

    if (renderer == NULL || data == NULL || map_rect == NULL || points == NULL || point_count <= 0)
    {
        return;
    }

    for (int i = 0; i < point_count; ++i)
    {
        Cabin_Point current = {0, 0};
        if (cabin_geo_to_pixel(data, map_rect, points[i].latitude, points[i].longitude, &current))
        {
            if (has_previous)
            {
                draw_thick_line(renderer, previous.x, previous.y, current.x, current.y, color);
            }
            previous = current;
            has_previous = 1;
        }
    }
}

static double cabin_bearing_degrees(double lat1, double lon1, double lat2, double lon2)
{
    if (!cabin_geo_point_valid(lat1, lon1) || !cabin_geo_point_valid(lat2, lon2))
    {
        return 225.0;
    }

    const double phi1 = lat1 * CABIN_PI / 180.0;
    const double phi2 = lat2 * CABIN_PI / 180.0;
    const double delta_lambda = (lon2 - lon1) * CABIN_PI / 180.0;
    const double y = sin(delta_lambda) * cos(phi2);
    const double x = cos(phi1) * sin(phi2) -
                     sin(phi1) * cos(phi2) * cos(delta_lambda);
    double bearing = atan2(y, x) * 180.0 / CABIN_PI;
    if (bearing < 0.0)
    {
        bearing += 360.0;
    }

    return bearing;
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

    printf("Cabin Geo: map source=%s.\n", data->map_source);
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
        printf("Cabin Geo: invalid/out-of-range point detected, route drawing will skip invalid points.\n");
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
    draw_rect(renderer, rect, COLOR_PANEL_LINE);
    draw_text_clipped(renderer, font, COLOR_WHITE, rect, rect->x + 14, rect->y + 8, "%s", title);
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
    const SDL_Rect clip_rect = {rect->x + 12, rect->y + 2, rect->w - 24, rect->h - 4};
    const int font_h = font != NULL ? TTF_FontHeight(font) : 20;
    const int text_y = rect->y + (rect->h - font_h) / 2;
    draw_text_clipped(renderer, font, COLOR_TEXT_DARK, &clip_rect, rect->x + 14, text_y, "%s", text);
}

static const char *safe_text_or(const char *text, const char *fallback)
{
    return text != NULL && text[0] != '\0' ? text : fallback;
}

static void draw_location_panel(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data, int x, int y, int w)
{
    const int header_h = 44;
    const int row_h = 46;

    draw_panel_header(renderer, assets->title_font, &(SDL_Rect){x, y, w, header_h}, "地点信息");
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h, w, row_h}, "%s", safe_text_or(data->current_city, "飞行途中"));
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h + row_h, w, row_h}, "%s", safe_text_or(data->current_district, "未知区域"));
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h + row_h * 2, w, row_h}, "%s", safe_text_or(data->current_town, "未知位置"));
    draw_rect(renderer, &(SDL_Rect){x, y, w, header_h + row_h * 3}, COLOR_TEXT_DARK);
}

static void draw_weather_panel(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data, int x, int y, int w)
{
    const int header_h = 44;
    const int row_h = 40;

    draw_panel_header(renderer, assets->title_font, &(SDL_Rect){x, y, w, header_h}, "天气信息");
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h, w, row_h}, "天气：%s", data->weather);
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h + row_h, w, row_h}, "温度：%.0f°C", data->temperature);
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h + row_h * 2, w, row_h}, "湿度：%.0f%%", data->humidity);
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h + row_h * 3, w, row_h}, "风速：%s", safe_text_or(data->wind_power, "--"));
    draw_panel_row(renderer, assets->font, &(SDL_Rect){x, y + header_h + row_h * 4, w, row_h}, "风向：%s", safe_text_or(data->wind_direction, "--"));
    draw_rect(renderer, &(SDL_Rect){x, y, w, header_h + row_h * 5}, COLOR_TEXT_DARK);
}

static void draw_fullscreen_symbol(SDL_Renderer *renderer, const SDL_Rect *rect)
{
    const int pad = 11;
    const int len = 10;

    set_color(renderer, COLOR_TEXT_DARK);
    SDL_RenderDrawLine(renderer, rect->x + pad, rect->y + pad, rect->x + pad + len, rect->y + pad);
    SDL_RenderDrawLine(renderer, rect->x + pad, rect->y + pad, rect->x + pad, rect->y + pad + len);
    SDL_RenderDrawLine(renderer, rect->x + rect->w - pad, rect->y + pad, rect->x + rect->w - pad - len, rect->y + pad);
    SDL_RenderDrawLine(renderer, rect->x + rect->w - pad, rect->y + pad, rect->x + rect->w - pad, rect->y + pad + len);
    SDL_RenderDrawLine(renderer, rect->x + pad, rect->y + rect->h - pad, rect->x + pad + len, rect->y + rect->h - pad);
    SDL_RenderDrawLine(renderer, rect->x + pad, rect->y + rect->h - pad, rect->x + pad, rect->y + rect->h - pad - len);
    SDL_RenderDrawLine(renderer, rect->x + rect->w - pad, rect->y + rect->h - pad, rect->x + rect->w - pad - len, rect->y + rect->h - pad);
    SDL_RenderDrawLine(renderer, rect->x + rect->w - pad, rect->y + rect->h - pad, rect->x + rect->w - pad, rect->y + rect->h - pad - len);
}

static void draw_map_control_button(SDL_Renderer *renderer, TTF_Font *font, SDL_Texture *texture, int x, int y, const char *fallback)
{
    const SDL_Rect rect = {x, y, 44, 44};

    if (texture != NULL)
    {
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        return;
    }

    fill_rect(renderer, &rect, COLOR_PANEL_BODY);
    draw_rect(renderer, &rect, COLOR_PANEL_LINE);

    if (fallback != NULL && strcmp(fallback, "FULL") == 0)
    {
        draw_fullscreen_symbol(renderer, &rect);
        return;
    }

    draw_text_centered(renderer, font, COLOR_TEXT_DARK, &rect, fallback);
}

static SDL_Rect cabin_bottom_bar_rect(int width, int height)
{
    const int margin = 24;
    const int bar_h = 104;
    const int right_reserved = width >= 1000 ? 324 : 24;
    SDL_Rect bar = {margin, height - bar_h - 22, width - margin - right_reserved - margin, bar_h};

    if (bar.w < 520)
    {
        bar.w = width - margin * 2;
    }
    if (bar.w < 1)
    {
        bar.w = width;
        bar.x = 0;
    }

    return bar;
}

static void cabin_info_panel_rects(int width, int height, SDL_Rect *location_rect, SDL_Rect *weather_rect)
{
    (void)height;

    const int panel_w = width >= 1200 ? 240 : 220;
    int panel_x = width - panel_w - 32;
    if (panel_x < 24)
    {
        panel_x = 24;
    }

    const int panel_y = 24;
    const int location_h = 44 + 46 * 3;
    const int weather_h = 44 + 40 * 5;
    const int weather_y = panel_y + location_h + 28;

    if (location_rect != NULL)
    {
        *location_rect = (SDL_Rect){panel_x, panel_y, panel_w, location_h};
    }
    if (weather_rect != NULL)
    {
        *weather_rect = (SDL_Rect){panel_x, weather_y, panel_w, weather_h};
    }
}

static void cabin_map_control_rects(int width, int height, SDL_Rect *fullscreen_rect, SDL_Rect *add_rect, SDL_Rect *sub_rect)
{
    const int button = 44;
    const int gap = 6;
    const int controls_h = button * 3 + gap * 2;
    const SDL_Rect bottom_bar = cabin_bottom_bar_rect(width, height);
    const int x = width - button - 24;
    int y = g_compact_mode ? height - controls_h - 24 : bottom_bar.y - controls_h - 18;

    if (y < 24)
    {
        y = height - controls_h - 24;
    }

    if (fullscreen_rect != NULL)
    {
        *fullscreen_rect = (SDL_Rect){x, y, button, button};
    }
    if (add_rect != NULL)
    {
        *add_rect = (SDL_Rect){x, y + button + gap, button, button};
    }
    if (sub_rect != NULL)
    {
        *sub_rect = (SDL_Rect){x, y + (button + gap) * 2, button, button};
    }
}

static void draw_map_controls(SDL_Renderer *renderer, const Cabin_Assets *assets, int width, int height)
{
    SDL_Rect fullscreen_rect;
    SDL_Rect add_rect;
    SDL_Rect sub_rect;
    cabin_map_control_rects(width, height, &fullscreen_rect, &add_rect, &sub_rect);

    draw_map_control_button(renderer, assets->title_font, assets->fullscreen_texture, fullscreen_rect.x, fullscreen_rect.y, "FULL");
    draw_map_control_button(renderer, assets->title_font, assets->add_texture, add_rect.x, add_rect.y, "+");
    draw_map_control_button(renderer, assets->title_font, assets->sub_texture, sub_rect.x, sub_rect.y, "-");
}

static void draw_status_badge(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data, const SDL_Rect *map_rect)
{
    if (map_rect == NULL)
    {
        return;
    }

    const SDL_Rect badge = {map_rect->x + 16, map_rect->y + 16, 244, 28};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    fill_rect(renderer, &badge, COLOR_BLACK_OVERLAY);
    draw_rect(renderer, &badge, COLOR_ROUTE_SOFT);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    draw_text_clipped(renderer, assets->small_font, COLOR_WHITE, &badge, badge.x + 10, badge.y + 4,
                      "MAP: %s  WEATHER: %s",
                      data->map_source,
                      data->weather_source);
}

static int point_in_rect(int x, int y, const SDL_Rect *rect)
{
    return rect != NULL &&
           x >= rect->x &&
           x < rect->x + rect->w &&
           y >= rect->y &&
           y < rect->y + rect->h;
}

static void cabin_clamp_map_pan(int width, int height)
{
    const float zoom = clamp_float(g_map_zoom, CABIN_MAP_ZOOM_MIN, CABIN_MAP_ZOOM_MAX);
    const float max_x = (float)width * 0.45f * zoom;
    const float max_y = (float)height * 0.45f * zoom;

    g_map_pan_x = clamp_float(g_map_pan_x, -max_x, max_x);
    g_map_pan_y = clamp_float(g_map_pan_y, -max_y, max_y);
}

static SDL_Rect cabin_zoomed_map_rect(int width, int height)
{
    const float zoom = clamp_float(g_map_zoom, CABIN_MAP_ZOOM_MIN, CABIN_MAP_ZOOM_MAX);
    const int zoom_w = (int)((float)width * zoom + 0.5f);
    const int zoom_h = (int)((float)height * zoom + 0.5f);

    cabin_clamp_map_pan(width, height);
    SDL_Rect rect = {(width - zoom_w) / 2 + (int)g_map_pan_x,
                     (height - zoom_h) / 2 + (int)g_map_pan_y,
                     zoom_w,
                     zoom_h};

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

void cabin_ui_handle_event(SDL_Window *window, const SDL_Event *event)
{
    if (window == NULL || event == NULL)
    {
        return;
    }

    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_c)
    {
        g_compact_mode = !g_compact_mode;
        printf("Cabin UI: %s mode.\n", g_compact_mode ? "compact" : "full");
        return;
    }

    if (event->type != SDL_MOUSEBUTTONDOWN &&
        event->type != SDL_MOUSEBUTTONUP &&
        event->type != SDL_MOUSEMOTION)
    {
        return;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    if (width <= 0 || height <= 0)
    {
        width = 1600;
        height = 900;
    }

    if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT)
    {
        g_map_dragging = 0;
        return;
    }

    if (event->type == SDL_MOUSEMOTION)
    {
        if (g_map_dragging)
        {
            const int mouse_x = event->motion.x;
            const int mouse_y = event->motion.y;
            g_map_pan_x += (float)(mouse_x - g_last_mouse_x);
            g_map_pan_y += (float)(mouse_y - g_last_mouse_y);
            g_last_mouse_x = mouse_x;
            g_last_mouse_y = mouse_y;
            cabin_clamp_map_pan(width, height);
        }
        return;
    }

    if (event->button.button != SDL_BUTTON_LEFT)
    {
        return;
    }

    SDL_Rect fullscreen_rect;
    SDL_Rect add_rect;
    SDL_Rect sub_rect;
    SDL_Rect location_rect;
    SDL_Rect weather_rect;
    const SDL_Rect bottom_bar = cabin_bottom_bar_rect(width, height);
    cabin_map_control_rects(width, height, &fullscreen_rect, &add_rect, &sub_rect);
    cabin_info_panel_rects(width, height, &location_rect, &weather_rect);

    const int mouse_x = event->button.x;
    const int mouse_y = event->button.y;

    if (point_in_rect(mouse_x, mouse_y, &fullscreen_rect))
    {
        const Uint32 flags = SDL_GetWindowFlags(window);
        const Uint32 fullscreen_flag = SDL_WINDOW_FULLSCREEN_DESKTOP;
        SDL_SetWindowFullscreen(window, (flags & fullscreen_flag) ? 0 : fullscreen_flag);
    }
    else if (point_in_rect(mouse_x, mouse_y, &add_rect))
    {
        g_map_zoom = clamp_float(g_map_zoom + CABIN_MAP_ZOOM_STEP, CABIN_MAP_ZOOM_MIN, CABIN_MAP_ZOOM_MAX);
        cabin_clamp_map_pan(width, height);
        printf("Cabin UI: map zoom %.2f.\n", g_map_zoom);
    }
    else if (point_in_rect(mouse_x, mouse_y, &sub_rect))
    {
        g_map_zoom = clamp_float(g_map_zoom - CABIN_MAP_ZOOM_STEP, CABIN_MAP_ZOOM_MIN, CABIN_MAP_ZOOM_MAX);
        cabin_clamp_map_pan(width, height);
        printf("Cabin UI: map zoom %.2f.\n", g_map_zoom);
    }
    else if (!point_in_rect(mouse_x, mouse_y, &location_rect) &&
             !point_in_rect(mouse_x, mouse_y, &weather_rect) &&
             (g_compact_mode || !point_in_rect(mouse_x, mouse_y, &bottom_bar)))
    {
        g_map_dragging = 1;
        g_last_mouse_x = mouse_x;
        g_last_mouse_y = mouse_y;
    }
}

static void draw_route(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data, const SDL_Rect *map_rect)
{
    Cabin_Point start = {0, 0};
    Cabin_Point end = {0, 0};
    Cabin_Point plane = {0, 0};
    const int has_start = cabin_geo_to_pixel(data, map_rect, data->origin_lat, data->origin_lon, &start);
    const int has_end = cabin_geo_to_pixel(data, map_rect, data->destination_lat, data->destination_lon, &end);
    const int has_plane = cabin_geo_to_pixel(data, map_rect, data->current_lat, data->current_lon, &plane);

    cabin_log_geo_debug(data, map_rect, start, end, plane, has_start, has_end, has_plane);

    draw_polyline_geo(renderer,
                      data,
                      map_rect,
                      data->planned_route,
                      data->planned_route_count,
                      COLOR_ROUTE_SOFT);

    draw_polyline_geo(renderer,
                      data,
                      map_rect,
                      data->flown_track,
                      data->flown_track_count,
                      COLOR_ROUTE);

    if (!has_plane)
    {
        static int printed_skip_warning = 0;
        if (!printed_skip_warning)
        {
            printf("Cabin Route: skip plane draw because current point is outside the configured map bounds.\n");
            printed_skip_warning = 1;
        }
        return;
    }

    Cabin_Point last_trace_pixel = {0, 0};
    int has_last_trace_pixel = 0;
    if (data->flown_track_count > 0)
    {
        const Cabin_Trajectory_Point *last_trace = &data->flown_track[data->flown_track_count - 1];
        if (cabin_geo_to_pixel(data, map_rect, last_trace->latitude, last_trace->longitude, &last_trace_pixel))
        {
            has_last_trace_pixel = 1;
        }
    }

    if (has_last_trace_pixel && (last_trace_pixel.x != plane.x || last_trace_pixel.y != plane.y))
    {
        draw_thick_line(renderer, last_trace_pixel.x, last_trace_pixel.y, plane.x, plane.y, COLOR_ROUTE);
    }

    double heading_from_lat = data->origin_lat;
    double heading_from_lon = data->origin_lon;
    double heading_to_lat = data->current_lat;
    double heading_to_lon = data->current_lon;
    if (data->flown_track_count >= 2)
    {
        const Cabin_Trajectory_Point *last = &data->flown_track[data->flown_track_count - 1];
        const Cabin_Trajectory_Point *previous = &data->flown_track[data->flown_track_count - 2];
        if (fabs(last->latitude - data->current_lat) < 0.000001 &&
            fabs(last->longitude - data->current_lon) < 0.000001)
        {
            heading_from_lat = previous->latitude;
            heading_from_lon = previous->longitude;
            heading_to_lat = last->latitude;
            heading_to_lon = last->longitude;
        }
        else
        {
            heading_from_lat = last->latitude;
            heading_from_lon = last->longitude;
        }
    }
    else if (data->planned_route_count >= 2)
    {
        int segment = (int)floorf(clamp_float(data->progress, 0.0f, 1.0f) * (float)(data->planned_route_count - 1));
        if (segment >= data->planned_route_count - 1)
        {
            segment = data->planned_route_count - 2;
        }
        if (segment < 0)
        {
            segment = 0;
        }
        heading_from_lat = data->planned_route[segment].latitude;
        heading_from_lon = data->planned_route[segment].longitude;
        heading_to_lat = data->planned_route[segment + 1].latitude;
        heading_to_lon = data->planned_route[segment + 1].longitude;
    }
    double bearing = cabin_bearing_degrees(heading_from_lat, heading_from_lon, heading_to_lat, heading_to_lon);
    if (data->has_heading && isfinite(data->track))
    {
        bearing = data->track;
    }

    if (has_start)
    {
        draw_filled_circle(renderer, start.x, start.y, 8, COLOR_GREEN);
        draw_text(renderer, assets->small_font, COLOR_TEXT_DARK, start.x - 24, start.y + 12, "%s", data->origin_city);
    }
    if (has_end)
    {
        draw_filled_circle(renderer, end.x, end.y, 8, COLOR_GREEN);
        draw_text(renderer, assets->small_font, COLOR_TEXT_DARK, end.x - 24, end.y + 12, "%s", data->destination_city);
    }

    if (assets->plane_texture != NULL)
    {
        const SDL_Rect dest = {plane.x - 15, plane.y - 15, 30, 30};
        SDL_RenderCopyEx(renderer, assets->plane_texture, NULL, &dest, bearing, NULL, SDL_FLIP_NONE);
    }
    else
    {
        set_color(renderer, COLOR_ROUTE);
        SDL_RenderDrawLine(renderer, plane.x, plane.y - 13, plane.x - 9, plane.y + 10);
        SDL_RenderDrawLine(renderer, plane.x, plane.y - 13, plane.x + 9, plane.y + 10);
        SDL_RenderDrawLine(renderer, plane.x - 9, plane.y + 10, plane.x + 9, plane.y + 10);
    }
}

static void draw_flight_info_bar(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data, int width, int height)
{
    const SDL_Rect bar = cabin_bottom_bar_rect(width, height);
    const int progress_w = bar.w > 820 ? 250 : 210;
    const int progress_x = bar.x + bar.w - progress_w - 24;
    const int progress_y = bar.y + 66;
    const int text_w = progress_x - bar.x - 34;
    const SDL_Rect text_clip = {bar.x + 14, bar.y + 8, text_w > 220 ? text_w : 220, bar.h - 16};
    const SDL_Rect progress_clip = {progress_x, bar.y + 8, progress_w + 4, bar.h - 16};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    fill_rect(renderer, &bar, COLOR_BLACK_OVERLAY);
    draw_rect(renderer, &bar, COLOR_ROUTE_SOFT);

    draw_text_clipped(renderer, assets->title_font, COLOR_WHITE, &text_clip, bar.x + 18, bar.y + 10, "%s -> %s",
              data->origin_city,
              data->destination_city);
    draw_text_clipped(renderer, assets->font, COLOR_WHITE, &text_clip, bar.x + 18, bar.y + 44, "高度 %.0fm   速度 %.0fkm/h   剩余 %.0f分钟",
              data->altitude,
              data->ground_speed,
              data->remaining_time_min);
    draw_text_clipped(renderer, assets->small_font, COLOR_WHITE, &text_clip, bar.x + 18, bar.y + 73, "航班 %s  机场 %s -> %s",
              data->flight_no,
              data->origin_airport,
              data->destination_airport);

    const float progress = clamp_float(data->progress, 0.0f, 1.0f);
    const SDL_Rect progress_bg = {progress_x, progress_y, progress_w, 12};
    const SDL_Rect progress_fg = {progress_x, progress_y, (int)((float)progress_w * progress), 12};
    draw_text_clipped(renderer, assets->font, COLOR_WHITE, &progress_clip, progress_x, bar.y + 34, "进度 %.0f%%", progress * 100.0f);
    fill_rect(renderer, &progress_bg, COLOR_PROGRESS_BG);
    fill_rect(renderer, &progress_fg, COLOR_ROUTE);
    draw_rect(renderer, &progress_bg, COLOR_WHITE);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static void draw_crash_demo_overlay(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data, int width, int height)
{
    TTF_Font *emergency_font = assets->emergency_font != NULL ? assets->emergency_font : assets->title_font;
    const Uint32 elapsed = SDL_GetTicks() - data->crash_demo_started_ticks;
    const int flash_on = ((elapsed / 110u) % 2u) == 0u;
    const SDL_Color overlay_color = flash_on ? (SDL_Color){126, 0, 8, 74} : (SDL_Color){52, 0, 4, 46};
    const SDL_Color panel_border_color = {215, 30, 40, 255};
    const int panel_width = width > 1000 ? 720 : (width * 4) / 5;
    const int panel_height = height > 600 ? 250 : 210;
    const SDL_Rect screen = {0, 0, width, height};
    const SDL_Rect panel = {(width - panel_width) / 2, (height - panel_height) / 2, panel_width, panel_height};
    const SDL_Rect emergency_line = {panel.x + 18, panel.y + 28, panel.w - 36, 82};
    const SDL_Rect detected_line = {panel.x + 18, panel.y + 106, panel.w - 36, 58};
    const SDL_Rect reset_line = {panel.x + 18, panel.y + panel.h - 56, panel.w - 36, 34};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    fill_rect(renderer, &screen, overlay_color);

    fill_rect(renderer, &panel, COLOR_EMERGENCY_DARK);
    draw_rect(renderer, &panel, panel_border_color);
    draw_rect(renderer, &(SDL_Rect){panel.x + 5, panel.y + 5, panel.w - 10, panel.h - 10}, panel_border_color);
    draw_text_centered(renderer, emergency_font, COLOR_EMERGENCY_RED, &emergency_line, "EMERGENCY");
    draw_text_centered(renderer, assets->title_font, COLOR_WHITE, &detected_line, "CRASH DETECTED");
    draw_text_centered(renderer, assets->small_font, COLOR_WHITE, &reset_line, "CRASH DEMO  -  PRESS R TO RESET");
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static SDL_Rect cabin_geo_map_rect(int width, int height)
{
    SDL_Rect rect = {0, 0, width, height};

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

    const SDL_Rect map_view_rect = cabin_geo_map_rect(width, height);
    const SDL_Rect zoomed_map_rect = cabin_zoomed_map_rect(width, height);
    SDL_Rect location_rect;
    SDL_Rect weather_rect;
    cabin_info_panel_rects(width, height, &location_rect, &weather_rect);

    draw_map_background(renderer, assets->map_texture, &zoomed_map_rect);
    SDL_RenderSetClipRect(renderer, &map_view_rect);
    draw_route(renderer, assets, data, &zoomed_map_rect);
    SDL_RenderSetClipRect(renderer, NULL);
    draw_location_panel(renderer, assets, data, location_rect.x, location_rect.y, location_rect.w);
    draw_weather_panel(renderer, assets, data, weather_rect.x, weather_rect.y, weather_rect.w);
    if (!g_compact_mode)
    {
        draw_flight_info_bar(renderer, assets, data, width, height);
    }
    draw_map_controls(renderer, assets, width, height);
    if (!g_compact_mode)
    {
        draw_status_badge(renderer, assets, data, &map_view_rect);
    }
    if (data->crash_demo_active)
    {
        draw_crash_demo_overlay(renderer, assets, data, width, height);
    }
}

