#include "cabin_ui.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int nearly_equal(double a, double b)
{
    return fabs(a - b) < 0.000001;
}

static int count_surface_color(SDL_Surface *surface, const SDL_Rect *region, Uint8 expected_red, Uint8 expected_green, Uint8 expected_blue)
{
    const int min_x = region != NULL ? region->x : 0;
    const int min_y = region != NULL ? region->y : 0;
    const int max_x = region != NULL ? region->x + region->w : surface->w;
    const int max_y = region != NULL ? region->y + region->h : surface->h;
    int count = 0;

    assert(SDL_LockSurface(surface) == 0);
    for (int y = min_y; y < max_y; ++y)
    {
        const Uint32 *row = (const Uint32 *)((const unsigned char *)surface->pixels + y * surface->pitch);
        for (int x = min_x; x < max_x; ++x)
        {
            Uint8 red;
            Uint8 green;
            Uint8 blue;
            Uint8 alpha;
            SDL_GetRGBA(row[x], surface->format, &red, &green, &blue, &alpha);
            if (red == expected_red && green == expected_green && blue == expected_blue && alpha == 255)
            {
                count++;
            }
        }
    }
    SDL_UnlockSurface(surface);
    return count;
}

static void assert_route_and_track_visible(SDL_Surface *surface)
{
    assert(count_surface_color(surface, NULL, 126, 188, 242) > 0);
    assert(count_surface_color(surface, NULL, 24, 137, 235) > 0);
}

static void test_render_crossing_route(void)
{
    SDL_Surface *surface;
    SDL_Renderer *renderer;
    Cabin_Data data;
    Cabin_Assets assets;
    SDL_Window *window;
    SDL_Event event;
    const SDL_Rect location_panel = {1328, 24, 240, 228};
    const SDL_Rect weather_panel = {1328, 280, 240, 244};

    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    surface = SDL_CreateRGBSurfaceWithFormat(0, 1600, 900, 32, SDL_PIXELFORMAT_ARGB8888);
    assert(surface != NULL);
    renderer = SDL_CreateSoftwareRenderer(surface);
    assert(renderer != NULL);
    window = SDL_CreateWindow("Cabin geometry test", 0, 0, 1600, 900, SDL_WINDOW_HIDDEN);
    assert(window != NULL);

    cabin_data_init(&data);
    memset(&assets, 0, sizeof(assets));
    data.map_uses_web_mercator = 1;
    data.map_loaded_zoom = 6;
    data.map_zoom = 6;
    data.map_min_zoom = CABIN_MAP_MIN_ZOOM;
    data.map_max_zoom = CABIN_MAP_MAX_ZOOM;
    data.map_api_zoom_enabled = 1;
    data.map_display_center_lat = 35.0;
    data.map_display_center_lon = 110.0;
    data.route_valid = 1;
    data.planned_route_count = 2;
    data.planned_route[0].latitude = 35.0;
    data.planned_route[0].longitude = 80.0;
    data.planned_route[1].latitude = 35.0;
    data.planned_route[1].longitude = 140.0;
    data.flown_track_count = 2;
    data.flown_track_seed_is_default = 0;
    data.flown_track_has_real_point = 1;
    data.flown_track[0].latitude = 34.0;
    data.flown_track[0].longitude = 80.0;
    data.flown_track[1].latitude = 34.0;
    data.flown_track[1].longitude = 140.0;
    data.current_lat = 34.0;
    data.current_lon = 110.0;

    cabin_ui_render(renderer, &assets, &data);
    SDL_RenderPresent(renderer);
    assert_route_and_track_visible(surface);
    assert(count_surface_color(surface, &location_panel, 126, 188, 242) == 0);
    assert(count_surface_color(surface, &weather_panel, 126, 188, 242) == 0);
    assert(count_surface_color(surface, &location_panel, 24, 137, 235) == 0);
    assert(count_surface_color(surface, &weather_panel, 24, 137, 235) == 0);

    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 1554;
    event.button.y = 684;
    cabin_ui_handle_event(window, &event, &data);
    assert(data.map_zoom == 7);
    cabin_ui_render(renderer, &assets, &data);
    SDL_RenderPresent(renderer);
    assert_route_and_track_visible(surface);

    event.button.y = 734;
    cabin_ui_handle_event(window, &event, &data);
    assert(data.map_zoom == 6);
    cabin_ui_render(renderer, &assets, &data);
    SDL_RenderPresent(renderer);
    assert_route_and_track_visible(surface);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    SDL_Quit();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char wind_power[32];

    cabin_ui_format_wind_power("≤3", wind_power, sizeof(wind_power));
    assert(strcmp(wind_power, "≤3级") == 0);
    cabin_ui_format_wind_power("4", wind_power, sizeof(wind_power));
    assert(strcmp(wind_power, "4级") == 0);
    cabin_ui_format_wind_power("2级", wind_power, sizeof(wind_power));
    assert(strcmp(wind_power, "2级") == 0);
    cabin_ui_format_wind_power("", wind_power, sizeof(wind_power));
    assert(strcmp(wind_power, "--") == 0);
    const SDL_Rect viewport = {100, 50, 200, 100};

    double x0 = 0.0;
    double y0 = 100.0;
    double x1 = 400.0;
    double y1 = 100.0;
    assert(cabin_ui_clip_line_to_rect(&viewport, &x0, &y0, &x1, &y1));
    assert(nearly_equal(x0, 100.0));
    assert(nearly_equal(y0, 100.0));
    assert(nearly_equal(x1, 299.0));
    assert(nearly_equal(y1, 100.0));

    x0 = 0.0;
    y0 = 0.0;
    x1 = 400.0;
    y1 = 200.0;
    assert(cabin_ui_clip_line_to_rect(&viewport, &x0, &y0, &x1, &y1));
    assert(x0 >= 100.0 && x0 <= 299.0);
    assert(y0 >= 50.0 && y0 <= 149.0);
    assert(x1 >= 100.0 && x1 <= 299.0);
    assert(y1 >= 50.0 && y1 <= 149.0);

    x0 = 120.0;
    y0 = 10.0;
    x1 = 250.0;
    y1 = 10.0;
    assert(!cabin_ui_clip_line_to_rect(&viewport, &x0, &y0, &x1, &y1));

    x0 = 120.0;
    y0 = 60.0;
    x1 = 250.0;
    y1 = 140.0;
    assert(cabin_ui_clip_line_to_rect(&viewport, &x0, &y0, &x1, &y1));
    assert(nearly_equal(x0, 120.0));
    assert(nearly_equal(y0, 60.0));
    assert(nearly_equal(x1, 250.0));
    assert(nearly_equal(y1, 140.0));

    test_render_crossing_route();

    printf("Cabin UI geometry tests passed.\n");
    return 0;
}
