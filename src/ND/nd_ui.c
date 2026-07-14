#include "nd_ui.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define ND_DEG_TO_RAD 0.01745329251994329577f
#define ND_MAX_RENDER_TARGETS 192
#define ND_MAP_MAX_RANGE_NM 80.0f
#define ND_MAP_FORWARD_SECTOR_DEG 45.0f
#define ND_LABEL_SCALE 0.62f
#define ND_SYMBOL_MARGIN 9
#define ND_SYMBOL_DEDUP_PIXELS 14
#define ND_ROUTE_CLIP_STEPS 384
#define ND_ROUTE_EDGE_REFINE_STEPS 10
#define ND_ROUTE_ARC_INSET_PIXELS 30.0f
#define ND_ROUTE_NOSE_GAP_PIXELS 6.0f
#define ND_ROUTE_MIN_CONTEXT_PIXELS 32.0f

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

typedef struct ND_RenderTarget
{
    const ND_NavPoint *point;
    ND_MapLayer layer;
    int x;
    int y;
    int priority;
    float distance_nm;
} ND_RenderTarget;

typedef struct ND_RouteScreenPoint
{
    float x;
    float y;
    float distance_nm;
    float relative_bearing_deg;
    int valid;
} ND_RouteScreenPoint;

static const SDL_Color COLOR_BLACK = {0, 0, 0, 255};
static const SDL_Color COLOR_WHITE = {236, 238, 232, 255};
static const SDL_Color COLOR_GRAY = {170, 176, 172, 255};
static const SDL_Color COLOR_GREEN = {30, 230, 45, 255};
static const SDL_Color COLOR_BLUE = {80, 150, 255, 255};
static const SDL_Color COLOR_MAGENTA = {238, 46, 210, 255};

static ND_UIRenderStats g_last_render_stats;

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

static void draw_text_scaled(SDL_Renderer *renderer, TTF_Font *font, SDL_Color color, int x, int y, float scale, const char *text)
{
    if (renderer == NULL || font == NULL || text == NULL || scale <= 0.0f)
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

    SDL_Rect dest = {x, y, (int)((float)surface->w * scale), (int)((float)surface->h * scale)};
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
    if (data->active_waypoint_name[0] == '\0' || strcmp(data->active_waypoint_name, "----") == 0)
    {
        snprintf(distance_text, sizeof(distance_text), "----NM");
    }
    else
    {
        snprintf(distance_text, sizeof(distance_text), "%04.1fNM", data->active_waypoint_distance_nm);
    }

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

static float nd_map_range_nm(const ND_Data *data)
{
    float range_nm = ND_MAP_MAX_RANGE_NM;
    if (data != NULL && data->range_nm > 1.0f && data->range_nm < range_nm)
    {
        range_nm = data->range_nm;
    }

    return range_nm;
}

static float nd_map_projection_radius(const ND_Layout *layout)
{
    return layout != NULL ? (float)layout->arc_radius * 0.92f : 1.0f;
}

static float nd_route_arc_clip_radius(const ND_Layout *layout)
{
    if (layout == NULL)
    {
        return 1.0f;
    }

    float radius = (float)layout->arc_radius - ND_ROUTE_ARC_INSET_PIXELS;
    if (radius < 1.0f)
    {
        radius = 1.0f;
    }
    return radius;
}

static void nd_aircraft_nose_point(const ND_Layout *layout, float *x, float *y)
{
    if (layout == NULL || x == NULL || y == NULL)
    {
        return;
    }

    *x = (float)layout->center_x;
    *y = (float)(layout->aircraft_y - 34);
}

static SDL_Rect nd_map_clip_rect(const ND_Layout *layout)
{
    const int margin = 10;
    const int top = 100;
    return (SDL_Rect){
        margin,
        top,
        layout->width - margin * 2,
        layout->height - top - margin};
}

static int rect_contains_point_margin(const SDL_Rect *rect, int x, int y, int margin)
{
    return rect != NULL &&
           x >= rect->x + margin &&
           x < rect->x + rect->w - margin &&
           y >= rect->y + margin &&
           y < rect->y + rect->h - margin;
}

static int rect_contains_pointf(const SDL_Rect *rect, float x, float y)
{
    return rect != NULL &&
           x >= (float)rect->x &&
           x < (float)(rect->x + rect->w) &&
           y >= (float)rect->y &&
           y < (float)(rect->y + rect->h);
}

static SDL_Rect nd_route_safe_rect(const ND_Layout *layout)
{
    const SDL_Rect map_clip = nd_map_clip_rect(layout);
    const int inset = 18;
    const int bottom_reserved = 72;
    SDL_Rect safe = {
        map_clip.x + inset,
        map_clip.y + inset,
        map_clip.w - inset * 2,
        map_clip.h - inset * 2 - bottom_reserved};

    if (safe.w < 1)
    {
        safe.w = 1;
    }
    if (safe.h < 1)
    {
        safe.h = 1;
    }

    return safe;
}

static int nd_route_over_fixed_info(const ND_Layout *layout, float x, float y)
{
    const SDL_Rect status_rect = {0, layout->height - 236, 128, 102};
    return rect_contains_pointf(&status_rect, x, y);
}

static int nd_route_screen_point_in_display_area(const ND_Layout *layout, float x, float y)
{
    if (layout == NULL)
    {
        return 0;
    }

    const SDL_Rect safe_rect = nd_route_safe_rect(layout);
    if (!rect_contains_pointf(&safe_rect, x, y))
    {
        return 0;
    }

    if (nd_route_over_fixed_info(layout, x, y))
    {
        return 0;
    }

    const float aircraft_dx = x - (float)layout->center_x;
    const float aircraft_dy = (float)layout->aircraft_y - y;
    const float map_radius = nd_map_projection_radius(layout);
    if (aircraft_dy < 0.0f || aircraft_dx * aircraft_dx + aircraft_dy * aircraft_dy > map_radius * map_radius)
    {
        return 0;
    }

    const float arc_dx = x - (float)layout->center_x;
    const float arc_dy = y - (float)layout->arc_center_y;
    const float arc_radius = nd_route_arc_clip_radius(layout);
    if (arc_dx * arc_dx + arc_dy * arc_dy > arc_radius * arc_radius)
    {
        return 0;
    }

    const float relative = atan2f(aircraft_dx, aircraft_dy) / ND_DEG_TO_RAD;
    return relative >= -ND_MAP_FORWARD_SECTOR_DEG && relative <= ND_MAP_FORWARD_SECTOR_DEG;
}

static int nd_project_latlon_to_route_screen(
    const ND_Layout *layout,
    const ND_Data *data,
    double latitude,
    double longitude,
    ND_RouteScreenPoint *out)
{
    if (layout == NULL || data == NULL || out == NULL ||
        !isfinite(latitude) || !isfinite(longitude))
    {
        return 0;
    }

    const float range_nm = nd_map_range_nm(data);
    if (range_nm <= 0.1f)
    {
        return 0;
    }

    const float aircraft_lat_rad = (float)data->latitude * ND_DEG_TO_RAD;
    const float lon_scale = cosf(aircraft_lat_rad);
    const double delta_lat = latitude - data->latitude;
    const double delta_lon = longitude - data->longitude;
    const float north_nm = (float)(delta_lat * 60.0);
    const float east_nm = (float)(delta_lon * 60.0 * (double)lon_scale);
    const float distance_nm = sqrtf(north_nm * north_nm + east_nm * east_nm);
    const float bearing = atan2f(east_nm, north_nm) / ND_DEG_TO_RAD;
    const float relative_bearing = normalize_signed_degrees(bearing - data->track);
    const float relative_rad = relative_bearing * ND_DEG_TO_RAD;
    const float distance_ratio = distance_nm / range_nm;
    const float map_radius = nd_map_projection_radius(layout);

    out->x = (float)layout->center_x + sinf(relative_rad) * distance_ratio * map_radius;
    out->y = (float)layout->aircraft_y - cosf(relative_rad) * distance_ratio * map_radius;
    out->distance_nm = distance_nm;
    out->relative_bearing_deg = relative_bearing;
    out->valid = 1;
    return 1;
}

static int nd_project_point_to_screen(
    const ND_Layout *layout,
    const ND_Data *data,
    const ND_NavPoint *point,
    float range_nm,
    int *x,
    int *y)
{
    if (layout == NULL || data == NULL || point == NULL || x == NULL || y == NULL)
    {
        return 0;
    }

    if (range_nm <= 0.1f)
    {
        return 0;
    }

    const float relative_bearing = normalize_signed_degrees(point->bearing_deg - data->track);
    const float relative_rad = relative_bearing * ND_DEG_TO_RAD;
    const float distance_ratio = point->distance_nm / range_nm;
    const float map_radius = nd_map_projection_radius(layout);

    *x = layout->center_x + (int)(sinf(relative_rad) * distance_ratio * map_radius);
    *y = layout->aircraft_y - (int)(cosf(relative_rad) * distance_ratio * map_radius);

    const SDL_Rect clip = nd_map_clip_rect(layout);
    return rect_contains_point_margin(&clip, *x, *y, ND_SYMBOL_MARGIN);
}

static int nd_project_latlon_to_screen(
    const ND_Layout *layout,
    const ND_Data *data,
    double latitude,
    double longitude,
    int *x,
    int *y)
{
    if (layout == NULL || data == NULL || x == NULL || y == NULL)
    {
        return 0;
    }

    const float range_nm = nd_map_range_nm(data);
    if (range_nm <= 0.1f)
    {
        return 0;
    }

    const float aircraft_lat_rad = (float)data->latitude * ND_DEG_TO_RAD;
    const float lon_scale = cosf(aircraft_lat_rad);
    const double delta_lat = latitude - data->latitude;
    const double delta_lon = longitude - data->longitude;
    const float north_nm = (float)(delta_lat * 60.0);
    const float east_nm = (float)(delta_lon * 60.0 * (double)lon_scale);
    const float distance_nm = sqrtf(north_nm * north_nm + east_nm * east_nm);
    if (distance_nm > range_nm)
    {
        return 0;
    }

    const float bearing = atan2f(east_nm, north_nm) / ND_DEG_TO_RAD;
    const float relative_bearing = normalize_signed_degrees(bearing - data->track);
    if (relative_bearing < -ND_MAP_FORWARD_SECTOR_DEG || relative_bearing > ND_MAP_FORWARD_SECTOR_DEG)
    {
        return 0;
    }

    const float relative_rad = relative_bearing * ND_DEG_TO_RAD;
    const float distance_ratio = distance_nm / range_nm;
    const float map_radius = nd_map_projection_radius(layout);

    *x = layout->center_x + (int)(sinf(relative_rad) * distance_ratio * map_radius);
    *y = layout->aircraft_y - (int)(cosf(relative_rad) * distance_ratio * map_radius);

    const SDL_Rect clip = nd_map_clip_rect(layout);
    return rect_contains_point_margin(&clip, *x, *y, ND_SYMBOL_MARGIN);
}

static void draw_waypoint_triangle(SDL_Renderer *renderer, int x, int y, SDL_Color color)
{
    set_color(renderer, color);
    SDL_RenderDrawLine(renderer, x, y - 5, x - 6, y + 5);
    SDL_RenderDrawLine(renderer, x - 6, y + 5, x + 6, y + 5);
    SDL_RenderDrawLine(renderer, x + 6, y + 5, x, y - 5);
}

static void draw_hollow_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color)
{
    set_color(renderer, color);

    int previous_x = cx + radius;
    int previous_y = cy;
    for (int degree = 1; degree <= 360; ++degree)
    {
        const float radians = (float)degree * ND_DEG_TO_RAD;
        const int x = cx + (int)(cosf(radians) * (float)radius);
        const int y = cy + (int)(sinf(radians) * (float)radius);
        SDL_RenderDrawLine(renderer, previous_x, previous_y, x, y);
        previous_x = x;
        previous_y = y;
    }
}

static int nav_point_map_layer(const ND_NavPoint *point, ND_MapLayer *layer)
{
    if (point == NULL || layer == NULL)
    {
        return 0;
    }

    switch (point->type)
    {
    case ND_POINT_WAYPOINT:
        *layer = ND_MAP_LAYER_WPT;
        return 1;
    case ND_POINT_AIRPORT:
    case ND_POINT_TOWER:
        *layer = ND_MAP_LAYER_ARPT;
        return 1;
    case ND_POINT_VOR:
    case ND_POINT_NDB:
    case ND_POINT_ILS:
        *layer = ND_MAP_LAYER_STA;
        return 1;
    default:
        break;
    }

    return 0;
}

static int nav_point_is_map_target(const ND_NavPoint *point)
{
    ND_MapLayer layer;
    return nav_point_map_layer(point, &layer);
}

static SDL_Color nav_point_symbol_color(const ND_NavPoint *point)
{
    switch (point->type)
    {
    case ND_POINT_AIRPORT:
    case ND_POINT_TOWER:
        return COLOR_WHITE;
    case ND_POINT_VOR:
    case ND_POINT_NDB:
    case ND_POINT_ILS:
    case ND_POINT_WAYPOINT:
    default:
        return COLOR_BLUE;
    }
}

static int nav_point_priority(const ND_NavPoint *point)
{
    if (point == NULL)
    {
        return 0;
    }

    if (!nav_point_is_map_target(point))
    {
        return 0;
    }

    if (point->active)
    {
        return 5;
    }

    switch (point->type)
    {
    case ND_POINT_AIRPORT:
    case ND_POINT_TOWER:
        return 4;
    case ND_POINT_WAYPOINT:
        return 3;
    case ND_POINT_VOR:
    case ND_POINT_NDB:
    case ND_POINT_ILS:
        return 2;
    default:
        return 0;
    }
}

static int nav_point_data_valid(const ND_NavPoint *point)
{
    return point != NULL &&
           point->ident[0] != '\0' &&
           isfinite(point->latitude) &&
           isfinite(point->longitude) &&
           isfinite(point->distance_nm) &&
           isfinite(point->bearing_deg);
}

static void count_displayed_target(ND_UIRenderStats *stats, ND_MapLayer layer)
{
    if (stats == NULL)
    {
        return;
    }

    switch (layer)
    {
    case ND_MAP_LAYER_WPT:
        ++stats->displayed_wpt;
        break;
    case ND_MAP_LAYER_ARPT:
        ++stats->displayed_arpt;
        break;
    case ND_MAP_LAYER_STA:
        ++stats->displayed_sta;
        break;
    default:
        break;
    }
}

static int collect_render_target(
    const ND_Layout *layout,
    const ND_Data *data,
    const ND_NavPoint *point,
    ND_RenderTarget *target,
    ND_UIRenderStats *stats)
{
    if (!nav_point_data_valid(point))
    {
        ++stats->filtered_invalid;
        return 0;
    }

    ND_MapLayer layer;
    if (!nav_point_map_layer(point, &layer))
    {
        ++stats->filtered_category;
        return 0;
    }

    if (!nd_data_get_map_layer_visible(data, layer))
    {
        ++stats->filtered_layer;
        return 0;
    }

    if (!point->visible)
    {
        ++stats->filtered_distance;
        return 0;
    }

    const float range_nm = nd_map_range_nm(data);
    if (point->distance_nm < 0.0f || point->distance_nm > range_nm)
    {
        ++stats->filtered_distance;
        return 0;
    }

    const float relative_bearing = normalize_signed_degrees(point->bearing_deg - data->track);
    if (relative_bearing < -ND_MAP_FORWARD_SECTOR_DEG || relative_bearing > ND_MAP_FORWARD_SECTOR_DEG)
    {
        ++stats->filtered_bearing;
        return 0;
    }

    int x = 0;
    int y = 0;
    if (!nd_project_point_to_screen(layout, data, point, range_nm, &x, &y))
    {
        ++stats->filtered_bounds;
        return 0;
    }

    target->point = point;
    target->layer = layer;
    target->x = x;
    target->y = y;
    target->priority = nav_point_priority(point);
    target->distance_nm = point->distance_nm;
    return 1;
}

static void draw_nav_point_symbol(SDL_Renderer *renderer, const ND_NavPoint *point, int x, int y, SDL_Color color)
{
    switch (point->type)
    {
    case ND_POINT_AIRPORT:
    case ND_POINT_TOWER:
        draw_hollow_circle(renderer, x, y, 7, color);
        break;
    case ND_POINT_VOR:
    case ND_POINT_NDB:
    case ND_POINT_ILS:
        draw_hollow_circle(renderer, x, y, 6, color);
        break;
    case ND_POINT_WAYPOINT:
    default:
        draw_waypoint_triangle(renderer, x, y, color);
        break;
    }
}

static int render_target_symbol_radius(const ND_RenderTarget *target)
{
    if (target == NULL)
    {
        return 6;
    }

    switch (target->point->type)
    {
    case ND_POINT_AIRPORT:
    case ND_POINT_TOWER:
        return 8;
    case ND_POINT_VOR:
    case ND_POINT_NDB:
    case ND_POINT_ILS:
        return 7;
    case ND_POINT_WAYPOINT:
    default:
        return 6;
    }
}

static int render_target_overlaps_existing_symbol(const ND_RenderTarget *candidate, const ND_RenderTarget *targets, int target_count)
{
    if (candidate == NULL || targets == NULL)
    {
        return 0;
    }

    for (int i = 0; i < target_count; ++i)
    {
        if (targets[i].layer != candidate->layer)
        {
            continue;
        }

        const int dx = candidate->x - targets[i].x;
        const int dy = candidate->y - targets[i].y;
        const int min_distance = render_target_symbol_radius(candidate) +
                                 render_target_symbol_radius(&targets[i]) +
                                 ND_SYMBOL_DEDUP_PIXELS;
        if (dx * dx + dy * dy < min_distance * min_distance)
        {
            return 1;
        }
    }

    return 0;
}

static void draw_nav_points(SDL_Renderer *renderer, TTF_Font *font, const ND_Layout *layout, const ND_Data *data)
{
    ND_RenderTarget targets[ND_MAX_RENDER_TARGETS];
    int target_count = 0;

    memset(&g_last_render_stats, 0, sizeof(g_last_render_stats));

    for (int i = 0; i < data->nav_point_count; ++i)
    {
        if (target_count >= ND_MAX_RENDER_TARGETS)
        {
            ++g_last_render_stats.filtered_bounds;
            continue;
        }

        if (collect_render_target(
                layout,
                data,
                &data->nav_points[i],
                &targets[target_count],
                &g_last_render_stats))
        {
            if (render_target_overlaps_existing_symbol(&targets[target_count], targets, target_count))
            {
                ++g_last_render_stats.filtered_symbol_overlap;
                continue;
            }
            count_displayed_target(&g_last_render_stats, targets[target_count].layer);
            ++target_count;
        }
    }

    for (int priority = 1; priority <= 5; ++priority)
    {
        for (int i = 0; i < target_count; ++i)
        {
            const ND_RenderTarget *target = &targets[i];
            if (target->priority != priority)
            {
                continue;
            }

            const SDL_Color symbol_color = nav_point_symbol_color(target->point);
            draw_nav_point_symbol(renderer, target->point, target->x, target->y, symbol_color);
        }
    }

    if (!nd_data_get_map_labels_visible(data))
    {
        g_last_render_stats.labels_hidden_compact = target_count;
        return;
    }

    const SDL_Rect clip = nd_map_clip_rect(layout);

    for (int i = 0; i < target_count; ++i)
    {
        const ND_RenderTarget *target = &targets[i];
        const ND_NavPoint *point = target->point;

        int text_w = 0;
        int text_h = 0;
        if (TTF_SizeUTF8(font, point->ident, &text_w, &text_h) != 0)
        {
            ++g_last_render_stats.labels_hidden_conflict;
            continue;
        }

        text_w = (int)((float)text_w * ND_LABEL_SCALE);
        text_h = (int)((float)text_h * ND_LABEL_SCALE);

        SDL_Rect label_rect = {
            target->x - text_w / 2,
            target->y + 12,
            text_w,
            text_h};

        if (label_rect.x < clip.x)
        {
            label_rect.x = clip.x;
        }
        if (label_rect.x + label_rect.w > clip.x + clip.w)
        {
            label_rect.x = clip.x + clip.w - label_rect.w;
        }
        if (label_rect.y + label_rect.h > clip.y + clip.h)
        {
            label_rect.y = clip.y + clip.h - label_rect.h;
        }

        draw_text_scaled(renderer, font, COLOR_WHITE, label_rect.x, label_rect.y, ND_LABEL_SCALE, point->ident);
        ++g_last_render_stats.labels_drawn;
    }
}

static void draw_route_line_segment(SDL_Renderer *renderer, float x1, float y1, float x2, float y2)
{
    SDL_RenderDrawLine(renderer, (int)(x1 + 0.5f), (int)(y1 + 0.5f), (int)(x2 + 0.5f), (int)(y2 + 0.5f));
    SDL_RenderDrawLine(renderer, (int)(x1 + 1.5f), (int)(y1 + 0.5f), (int)(x2 + 1.5f), (int)(y2 + 0.5f));
}

static float route_line_length(float x1, float y1, float x2, float y2)
{
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

static int route_line_point_inside(const ND_Layout *layout, const ND_RouteScreenPoint *a, const ND_RouteScreenPoint *b, float t)
{
    const float x = a->x + (b->x - a->x) * t;
    const float y = a->y + (b->y - a->y) * t;
    return nd_route_screen_point_in_display_area(layout, x, y);
}

static float refine_route_clip_edge(const ND_Layout *layout, const ND_RouteScreenPoint *a, const ND_RouteScreenPoint *b, float low, float high, int low_inside)
{
    for (int i = 0; i < ND_ROUTE_EDGE_REFINE_STEPS; ++i)
    {
        const float mid = (low + high) * 0.5f;
        const int mid_inside = route_line_point_inside(layout, a, b, mid);
        if (mid_inside == low_inside)
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }

    return (low + high) * 0.5f;
}

static int draw_clipped_route_segment(
    SDL_Renderer *renderer,
    const ND_Layout *layout,
    const ND_RouteScreenPoint *a,
    const ND_RouteScreenPoint *b,
    int is_active_segment,
    int has_visible_endpoint)
{
    if (renderer == NULL || layout == NULL || a == NULL || b == NULL || !a->valid || !b->valid)
    {
        return 0;
    }

    int drawn = 0;
    int previous_inside = route_line_point_inside(layout, a, b, 0.0f);
    float previous_t = 0.0f;
    float visible_start_t = previous_inside ? 0.0f : -1.0f;

    const float line_length = route_line_length(a->x, a->y, b->x, b->y);
    int clip_steps = (int)ceilf(line_length / 8.0f);
    if (clip_steps < ND_ROUTE_CLIP_STEPS)
    {
        clip_steps = ND_ROUTE_CLIP_STEPS;
    }
    if (clip_steps > 4096)
    {
        clip_steps = 4096;
    }

    for (int step = 1; step <= clip_steps; ++step)
    {
        const float t = (float)step / (float)clip_steps;
        const int inside = route_line_point_inside(layout, a, b, t);

        if (inside != previous_inside)
        {
            const float edge_t = refine_route_clip_edge(layout, a, b, previous_t, t, previous_inside);
            if (inside)
            {
                visible_start_t = edge_t;
            }
            else if (visible_start_t >= 0.0f)
            {
                const float x1 = a->x + (b->x - a->x) * visible_start_t;
                const float y1 = a->y + (b->y - a->y) * visible_start_t;
                const float x2 = a->x + (b->x - a->x) * edge_t;
                const float y2 = a->y + (b->y - a->y) * edge_t;
                if (is_active_segment || has_visible_endpoint ||
                    route_line_length(x1, y1, x2, y2) >= ND_ROUTE_MIN_CONTEXT_PIXELS)
                {
                    draw_route_line_segment(renderer, x1, y1, x2, y2);
                    ++drawn;
                }
                visible_start_t = -1.0f;
            }
        }

        previous_inside = inside;
        previous_t = t;
    }

    if (previous_inside && visible_start_t >= 0.0f)
    {
        const float x1 = a->x + (b->x - a->x) * visible_start_t;
        const float y1 = a->y + (b->y - a->y) * visible_start_t;
        if (is_active_segment || has_visible_endpoint ||
            route_line_length(x1, y1, b->x, b->y) >= ND_ROUTE_MIN_CONTEXT_PIXELS)
        {
            draw_route_line_segment(renderer, x1, y1, b->x, b->y);
            ++drawn;
        }
    }

    return drawn;
}

static int route_active_index_valid(const ND_Data *data)
{
    return data != NULL &&
           data->route_active_index >= 0 &&
           data->route_active_index < data->route_point_count;
}

static int route_point_in_forward_sector(const ND_RouteScreenPoint *point)
{
    return point != NULL &&
           point->valid &&
           point->relative_bearing_deg >= -ND_MAP_FORWARD_SECTOR_DEG &&
           point->relative_bearing_deg <= ND_MAP_FORWARD_SECTOR_DEG;
}

static int build_active_segment_start(const ND_Layout *layout, const ND_RouteScreenPoint *target, ND_RouteScreenPoint *start)
{
    if (layout == NULL || target == NULL || start == NULL || !target->valid)
    {
        return 0;
    }

    float nose_x = 0.0f;
    float nose_y = 0.0f;
    nd_aircraft_nose_point(layout, &nose_x, &nose_y);

    const float dx = target->x - nose_x;
    const float dy = target->y - nose_y;
    const float length = sqrtf(dx * dx + dy * dy);
    if (length < 1.0f)
    {
        return 0;
    }

    memset(start, 0, sizeof(*start));
    start->x = nose_x + dx / length * ND_ROUTE_NOSE_GAP_PIXELS;
    start->y = nose_y + dy / length * ND_ROUTE_NOSE_GAP_PIXELS;
    start->distance_nm = 0.0f;
    start->relative_bearing_deg = 0.0f;
    start->valid = 1;
    return 1;
}

static int route_point_visible(const ND_RouteScreenPoint *point, float range_nm)
{
    return point != NULL &&
           point->valid &&
           point->distance_nm <= range_nm &&
           point->relative_bearing_deg >= -ND_MAP_FORWARD_SECTOR_DEG &&
           point->relative_bearing_deg <= ND_MAP_FORWARD_SECTOR_DEG;
}

static void draw_route_layer(SDL_Renderer *renderer, TTF_Font *font, const ND_Layout *layout, const ND_Data *data)
{
    if (renderer == NULL || layout == NULL || data == NULL || !data->route_valid)
    {
        return;
    }

    ND_RouteScreenPoint screen_points[ND_MAX_ROUTE_POINTS];
    int visible[ND_MAX_ROUTE_POINTS];
    int continuous_route_point[ND_MAX_ROUTE_POINTS];
    memset(screen_points, 0, sizeof(screen_points));
    memset(visible, 0, sizeof(visible));
    memset(continuous_route_point, 0, sizeof(continuous_route_point));

    const int count = data->route_point_count < ND_MAX_ROUTE_POINTS ? data->route_point_count : ND_MAX_ROUTE_POINTS;
    const float range_nm = nd_map_range_nm(data);
    for (int i = 0; i < count; ++i)
    {
        const ND_RoutePoint *point = &data->route_points[i];
        if (point->has_position)
        {
            if (nd_project_latlon_to_route_screen(layout, data, point->latitude, point->longitude, &screen_points[i]))
            {
                visible[i] = route_point_visible(&screen_points[i], range_nm) &&
                             nd_route_screen_point_in_display_area(layout, screen_points[i].x, screen_points[i].y);
            }
        }
    }

    const int has_active_index = route_active_index_valid(data);
    const int active_index = has_active_index ? data->route_active_index : -1;

    set_color(renderer, COLOR_WHITE);
    if (!has_active_index ||
        !data->route_points[active_index].has_position ||
        !screen_points[active_index].valid ||
        !route_point_in_forward_sector(&screen_points[active_index]))
    {
        return;
    }

    ND_RouteScreenPoint active_start;
    const int active_segment_drawn = build_active_segment_start(layout, &screen_points[active_index], &active_start)
                                         ? draw_clipped_route_segment(
                                               renderer,
                                               layout,
                                               &active_start,
                                               &screen_points[active_index],
                                               1,
                                               visible[active_index])
                                         : 0;
    g_last_render_stats.route_segments_drawn += active_segment_drawn;

    /* A future segment is meaningful only while it extends the visible active chain. */
    if (active_segment_drawn > 0 && visible[active_index])
    {
        continuous_route_point[active_index] = 1;
        for (int i = active_index + 1; i < count; ++i)
        {
            if (!data->route_points[i - 1].has_position ||
                !data->route_points[i].has_position ||
                !continuous_route_point[i - 1])
            {
                break;
            }

            const int segment_drawn = draw_clipped_route_segment(
                renderer,
                layout,
                &screen_points[i - 1],
                &screen_points[i],
                0,
                1);
            g_last_render_stats.route_segments_drawn += segment_drawn;
            if (segment_drawn <= 0)
            {
                break;
            }

            if (!visible[i])
            {
                /* The chain has left the display; do not draw a later re-entry in isolation. */
                break;
            }
            continuous_route_point[i] = 1;
        }
    }

    for (int i = 0; i < count; ++i)
    {
        if (visible[i] && continuous_route_point[i])
        {
            const int x = (int)(screen_points[i].x + 0.5f);
            const int y = (int)(screen_points[i].y + 0.5f);
            draw_hollow_circle(renderer, x, y, 5, COLOR_WHITE);
            ++g_last_render_stats.route_points_drawn;

            if (font != NULL && data->route_points[i].ident[0] != '\0')
            {
                int text_w = 0;
                int text_h = 0;
                if (TTF_SizeUTF8(font, data->route_points[i].ident, &text_w, &text_h) == 0)
                {
                    text_w = (int)((float)text_w * ND_LABEL_SCALE);
                    text_h = (int)((float)text_h * ND_LABEL_SCALE);
                    draw_text_scaled(renderer,
                                     font,
                                     COLOR_WHITE,
                                     x - text_w / 2,
                                     y + 12,
                                     ND_LABEL_SCALE,
                                     data->route_points[i].ident);
                }
            }
        }
    }
}

static void draw_map_layer_status_row(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const ND_Data *data,
    ND_MapLayer layer,
    int x,
    int y,
    const char *label)
{
    if (nd_data_get_map_layer_visible(data, layer))
    {
        draw_text(renderer, font, COLOR_BLUE, x, y, "%s", label);
    }
}

static void draw_map_layer_status(SDL_Renderer *renderer, TTF_Font *font, const ND_Layout *layout, const ND_Data *data)
{
    const int x = 20;
    const int y = layout->height - 220;

    draw_map_layer_status_row(renderer, font, data, ND_MAP_LAYER_ARPT, x, y, "ARPT");
    draw_map_layer_status_row(renderer, font, data, ND_MAP_LAYER_WPT, x, y + 18, "WPT");
    draw_map_layer_status_row(renderer, font, data, ND_MAP_LAYER_STA, x, y + 36, "STA");
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
    draw_track_line(renderer, &layout);
    draw_nav_points(renderer, font, &layout, data);
    draw_route_layer(renderer, font, &layout, data);
    draw_aircraft_symbol(renderer, &layout);
    draw_top_status(renderer, font, &layout, data);
    draw_active_waypoint_info(renderer, font, &layout, data);
    draw_map_layer_status(renderer, font, &layout, data);
}

void nd_ui_get_last_render_stats(ND_UIRenderStats *stats)
{
    if (stats == NULL)
    {
        return;
    }

    *stats = g_last_render_stats;
}
