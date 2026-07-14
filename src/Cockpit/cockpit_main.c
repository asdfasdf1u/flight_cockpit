#include "cockpit_main.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "cockpit_layout.h"
#include "cockpit_ui.h"
#include "cockpit_alarm.h"

#include "../PFD/pfd_data.h"
#include "../PFD/pfd_ui.h"

#include "../ND/nd_data.h"
#include "../ND/nd_ui.h"

#include "../Data/sim_data_center.h"

#include "../Systems/aircraft_systems_data.h"
#include "../Data/sim_data_center.h"
#include "../EICAS1/eicas_data.h"
#include "../EICAS1/eicas1_ui.h"
#include "../EICAS2/eicas2_ui.h"

#include "../FMC/fmc_data.h"
#include "../FMC/fmc_display.h"
#include "../FMC/fmc_event.h"
#include "../FMC/fmc_connect.h"

#include "../Util/xplane_live_data.h"

int fmc_xplane_send_command(const char *command);

#define COCKPIT_WINDOW_WIDTH 1600
#define COCKPIT_WINDOW_HEIGHT 900
#define COCKPIT_TARGET_FRAME_MS 16
#define COCKPIT_PFD_TARGET_FRAME_MS COCKPIT_TARGET_FRAME_MS
#define COCKPIT_SYNC_CHECK_LOG_MS 5000
#define COCKPIT_PFD_PERF_LOG_MS 2000
#define COCKPIT_SCENE_TEXTURE_MAX_WIDTH 2048

#define COCKPIT_PFD_TEXTURE_WIDTH 900
#define COCKPIT_PFD_TEXTURE_HEIGHT 800
#define COCKPIT_ND_TEXTURE_WIDTH 752
#define COCKPIT_ND_TEXTURE_HEIGHT 752
#define COCKPIT_EICAS_TEXTURE_WIDTH 768
#define COCKPIT_EICAS_TEXTURE_HEIGHT 768
#define COCKPIT_FMC_TEXTURE_WIDTH COCKPIT_FMC_IMAGE_WIDTH
#define COCKPIT_FMC_TEXTURE_HEIGHT COCKPIT_FMC_IMAGE_HEIGHT

#define COCKPIT_MIN_SCALE 0.5f
#define COCKPIT_MAX_SCALE 3.0f

typedef struct Cockpit_XPlaneConfig
{
    char ip[16];
    unsigned short port;
} Cockpit_XPlaneConfig;

static unsigned short cockpit_parse_xplane_port(const char *text, unsigned short fallback)
{
    char *end = NULL;
    long value;

    if (text == NULL || text[0] == '\0')
    {
        return fallback;
    }

    value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value <= 0 || value > 65535)
    {
        printf("Cockpit X-Plane: invalid port '%s'; using %u.\n", text, fallback);
        return fallback;
    }

    return (unsigned short)value;
}

static void cockpit_xplane_config_set_ip(Cockpit_XPlaneConfig *config, const char *ip)
{
    if (config == NULL || ip == NULL || ip[0] == '\0')
    {
        return;
    }

    snprintf(config->ip, sizeof(config->ip), "%s", ip);
}

static void cockpit_xplane_config_init(Cockpit_XPlaneConfig *config, int argc, char *argv[])
{
    int i;
    const char *env_ip = getenv("XPLANE_IP");
    const char *env_port = getenv("XPLANE_PORT");

    if (config == NULL)
    {
        return;
    }

    snprintf(config->ip, sizeof(config->ip), "%s", XPLANE_LIVE_DEFAULT_IP);
    config->port = XPLANE_LIVE_DEFAULT_PORT;

    cockpit_xplane_config_set_ip(config, env_ip);
    config->port = cockpit_parse_xplane_port(env_port, config->port);

    for (i = 1; i < argc; ++i)
    {
        const char *arg = argv[i];

        if (arg == NULL)
        {
            continue;
        }
        if (strncmp(arg, "--xplane-ip=", 12) == 0)
        {
            cockpit_xplane_config_set_ip(config, arg + 12);
        }
        else if (strcmp(arg, "--xplane-ip") == 0 && i + 1 < argc)
        {
            cockpit_xplane_config_set_ip(config, argv[++i]);
        }
        else if (strncmp(arg, "--xplane-port=", 15) == 0)
        {
            config->port = cockpit_parse_xplane_port(arg + 15, config->port);
        }
        else if (strcmp(arg, "--xplane-port") == 0 && i + 1 < argc)
        {
            config->port = cockpit_parse_xplane_port(argv[++i], config->port);
        }
    }
}

static void cockpit_xplane_config_apply_env(const Cockpit_XPlaneConfig *config)
{
    char port_text[16];

    if (config == NULL)
    {
        return;
    }

    snprintf(port_text, sizeof(port_text), "%u", config->port);
#ifdef _WIN32
    _putenv_s("XPLANE_IP", config->ip);
    _putenv_s("XPLANE_PORT", port_text);
#else
    setenv("XPLANE_IP", config->ip, 1);
    setenv("XPLANE_PORT", port_text, 1);
#endif
}

typedef struct Cockpit_RenderTargets
{
    SDL_Texture *pfd_texture;
    SDL_Texture *nd_texture;
    SDL_Texture *eicas1_texture;
    SDL_Texture *eicas2_texture;
    SDL_Texture *fmc_texture;
    SDL_Texture *scene_texture;
} Cockpit_RenderTargets;

typedef struct Cockpit_Camera
{
    float scale;
    float offset_x;
    float offset_y;
} Cockpit_Camera;

typedef struct Cockpit_MainState
{
    CockpitAlarmState alarm;
} Cockpit_MainState;

static void cockpit_startup_log(int truncate, const char *format, ...)
{
    char log_path[MAX_PATH];
    DWORD path_length = GetModuleFileNameA(NULL, log_path, (DWORD)sizeof(log_path));
    FILE *log_file = NULL;

    if (path_length > 0 && path_length < sizeof(log_path))
    {
        char *file_name = strrchr(log_path, '\\');
        if (file_name != NULL)
        {
            snprintf(file_name + 1, sizeof(log_path) - (size_t)(file_name + 1 - log_path), "cockpit_startup.log");
            log_file = fopen(log_path, truncate ? "w" : "a");
        }
    }

    if (log_file == NULL)
    {
        log_file = fopen("cockpit_startup.log", truncate ? "w" : "a");
    }
    if (log_file == NULL)
    {
        return;
    }

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fputc('\n', log_file);
    fclose(log_file);
}

static void cockpit_show_startup_error(const char *stage, const char *detail)
{
    char message[512];

    snprintf(
        message,
        sizeof(message),
        "Cockpit failed during %s.\n\n%s\n\nDetails were written to build/cockpit_startup.log.",
        stage != NULL ? stage : "startup",
        detail != NULL ? detail : "No additional SDL error was supplied.");
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Cockpit startup failed", message, NULL);
}

static TTF_Font *open_cockpit_font(void)
{
    TTF_Font *font = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 18);
    if (font != NULL)
    {
        return font;
    }

    font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 18);
    if (font != NULL)
    {
        return font;
    }

    return TTF_OpenFont("C:/Windows/Fonts/simhei.ttf", 18);
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

static SDL_Texture *load_texture_optional(SDL_Renderer *renderer, const char *path, int *width, int *height)
{
    if (width != NULL)
    {
        *width = 0;
    }
    if (height != NULL)
    {
        *height = 0;
    }

    SDL_Texture *texture = IMG_LoadTexture(renderer, path);
    if (texture == NULL)
    {
        printf("IMG_LoadTexture failed for %s: %s\n", path, IMG_GetError());
        return NULL;
    }

    if (SDL_QueryTexture(texture, NULL, NULL, width, height) != 0)
    {
        printf("SDL_QueryTexture failed for %s: %s\n", path, SDL_GetError());
    }

    return texture;
}

static SDL_Texture *create_target_texture(SDL_Renderer *renderer, int width, int height)
{
    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        width,
        height);
    if (texture != NULL)
    {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }

    return texture;
}

static void cockpit_scene_texture_dimensions(int world_width, int world_height, int *texture_width, int *texture_height)
{
    float scale;

    if (texture_width == NULL || texture_height == NULL || world_width <= 0 || world_height <= 0)
    {
        return;
    }

    scale = (float)COCKPIT_SCENE_TEXTURE_MAX_WIDTH / (float)world_width;
    if (scale > 1.0f)
    {
        scale = 1.0f;
    }
    *texture_width = (int)((float)world_width * scale + 0.5f);
    *texture_height = (int)((float)world_height * scale + 0.5f);
}

static int create_render_targets(SDL_Renderer *renderer, Cockpit_RenderTargets *targets, int world_width, int world_height)
{
    int scene_width = 0;
    int scene_height = 0;

    if (renderer == NULL || targets == NULL)
    {
        return 0;
    }

    cockpit_scene_texture_dimensions(world_width, world_height, &scene_width, &scene_height);
    if (scene_width <= 0 || scene_height <= 0)
    {
        return 0;
    }

    targets->pfd_texture = create_target_texture(renderer, COCKPIT_PFD_TEXTURE_WIDTH, COCKPIT_PFD_TEXTURE_HEIGHT);
    targets->nd_texture = create_target_texture(renderer, COCKPIT_ND_TEXTURE_WIDTH, COCKPIT_ND_TEXTURE_HEIGHT);
    targets->eicas1_texture = create_target_texture(renderer, COCKPIT_EICAS_TEXTURE_WIDTH, COCKPIT_EICAS_TEXTURE_HEIGHT);
    targets->eicas2_texture = create_target_texture(renderer, COCKPIT_EICAS_TEXTURE_WIDTH, COCKPIT_EICAS_TEXTURE_HEIGHT);
    targets->fmc_texture = create_target_texture(renderer, COCKPIT_FMC_TEXTURE_WIDTH, COCKPIT_FMC_TEXTURE_HEIGHT);
    targets->scene_texture = create_target_texture(renderer, scene_width, scene_height);

    return targets->pfd_texture != NULL &&
           targets->nd_texture != NULL &&
           targets->eicas1_texture != NULL &&
           targets->eicas2_texture != NULL &&
           targets->fmc_texture != NULL &&
           targets->scene_texture != NULL;
}

static void destroy_render_targets(Cockpit_RenderTargets *targets)
{
    if (targets == NULL)
    {
        return;
    }

    if (targets->pfd_texture != NULL)
    {
        SDL_DestroyTexture(targets->pfd_texture);
        targets->pfd_texture = NULL;
    }
    if (targets->nd_texture != NULL)
    {
        SDL_DestroyTexture(targets->nd_texture);
        targets->nd_texture = NULL;
    }
    if (targets->eicas1_texture != NULL)
    {
        SDL_DestroyTexture(targets->eicas1_texture);
        targets->eicas1_texture = NULL;
    }
    if (targets->eicas2_texture != NULL)
    {
        SDL_DestroyTexture(targets->eicas2_texture);
        targets->eicas2_texture = NULL;
    }
    if (targets->fmc_texture != NULL)
    {
        SDL_DestroyTexture(targets->fmc_texture);
        targets->fmc_texture = NULL;
    }
    if (targets->scene_texture != NULL)
    {
        SDL_DestroyTexture(targets->scene_texture);
        targets->scene_texture = NULL;
    }
}

static void render_to_texture(SDL_Renderer *renderer, SDL_Texture *texture, void (*render_func)(SDL_Renderer *, TTF_Font *, const void *), TTF_Font *font, const void *data)
{
    if (renderer == NULL || texture == NULL || render_func == NULL || font == NULL || data == NULL)
    {
        return;
    }

    SDL_SetRenderTarget(renderer, texture);
    SDL_RenderSetViewport(renderer, NULL);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    render_func(renderer, font, data);
}

static void render_pfd_adapter(SDL_Renderer *renderer, TTF_Font *font, const void *data)
{
    pfd_ui_render(renderer, font, (const PFD_Data *)data);
}

static void render_nd_adapter(SDL_Renderer *renderer, TTF_Font *font, const void *data)
{
    nd_ui_render(renderer, font, (const ND_Data *)data);
}

static void render_eicas1_adapter(SDL_Renderer *renderer, TTF_Font *font, const void *data)
{
    eicas1_ui_render(renderer, font, (const AircraftSystems_Data *)data);
}

static void render_eicas2_adapter(SDL_Renderer *renderer, TTF_Font *font, const void *data)
{
    eicas2_ui_render(renderer, font, (const AircraftSystems_Data *)data);
}

static AircraftSystems_WarningLevel sim_warning_level_to_aircraft(SimWarningLevel level)
{
    switch (level)
    {
    case SIM_WARNING_WARNING:
        return AIRCRAFT_SYSTEMS_WARNING_WARNING;
    case SIM_WARNING_CAUTION:
        return AIRCRAFT_SYSTEMS_WARNING_CAUTION;
    case SIM_WARNING_INFO:
    default:
        return AIRCRAFT_SYSTEMS_WARNING_INFO;
    }
}

static void apply_sim_snapshot_to_aircraft_systems(AircraftSystems_Data *data, const SimSnapshot *snapshot)
{
    if (data == NULL || snapshot == NULL)
    {
        return;
    }

    data->engine_left.n1 = snapshot->n1_left;
    data->engine_left.n2 = snapshot->n2_left;
    data->engine_left.egt = snapshot->egt_left;
    data->engine_left.fuel_flow = snapshot->fuel_flow_left;
    data->engine_left.eicas1_fuel_flow_display_valid = 0;
    data->engine_left.eicas2_fuel_flow_display_valid = 0;
    data->engine_left.oil_pressure = snapshot->oil_pressure_left;
    data->engine_left.oil_temp = snapshot->oil_temperature_left;
    data->engine_left.oil_quantity = snapshot->oil_quantity_left;
    data->engine_left.vibration = snapshot->vibration_left;
    data->engine_left.running = snapshot->n1_left > 20.0f || snapshot->n2_left > 20.0f;

    data->engine_right.n1 = snapshot->n1_right;
    data->engine_right.n2 = snapshot->n2_right;
    data->engine_right.egt = snapshot->egt_right;
    data->engine_right.fuel_flow = snapshot->fuel_flow_right;
    data->engine_right.eicas1_fuel_flow_display_valid = 0;
    data->engine_right.eicas2_fuel_flow_display_valid = 0;
    data->engine_right.oil_pressure = snapshot->oil_pressure_right;
    data->engine_right.oil_temp = snapshot->oil_temperature_right;
    data->engine_right.oil_quantity = snapshot->oil_quantity_right;
    data->engine_right.vibration = snapshot->vibration_right;
    data->engine_right.running = snapshot->n1_right > 20.0f || snapshot->n2_right > 20.0f;

    data->total_air_temperature = snapshot->total_air_temperature;
    data->fuel_quantity = snapshot->fuel_quantity;
    data->fuel_left_quantity = snapshot->fuel_left_quantity;
    data->fuel_center_quantity = snapshot->fuel_center_quantity;
    data->fuel_right_quantity = snapshot->fuel_right_quantity;
    data->fuel_total_quantity = snapshot->fuel_left_quantity + snapshot->fuel_center_quantity + snapshot->fuel_right_quantity;
    data->fuel_tank_quantities_valid = snapshot->has_eicas_upper;
    data->hydraulic_pressure = snapshot->hydraulic_pressure;
    data->cabin_pressure = snapshot->cabin_pressure;
    data->battery_voltage = snapshot->battery_voltage;
    data->gear_down = snapshot->gear_down;
    data->flaps_level = snapshot->flaps_level;
    data->parking_brake_on = snapshot->parking_brake_on;
    data->simulation_time = snapshot->sim_time;

    data->warning_count = 0;
    for (int i = 0; i < snapshot->warning_count && i < AIRCRAFT_SYSTEMS_MAX_WARNINGS; ++i)
    {
        snprintf(data->warnings[i].text, sizeof(data->warnings[i].text), "%s", snapshot->warnings[i].text);
        data->warnings[i].level = sim_warning_level_to_aircraft(snapshot->warnings[i].level);
        data->warnings[i].active = snapshot->warnings[i].active;
        ++data->warning_count;
    }
}

static void render_fmc_to_texture(
    SDL_Renderer *renderer,
    SDL_Texture *texture,
    TTF_Font *font,
    const FMC_Display_Assets *assets,
    const FMC_Event_State *state,
    const FMC_Data *data)
{
    if (renderer == NULL || texture == NULL || font == NULL || data == NULL)
    {
        return;
    }

    SDL_SetRenderTarget(renderer, texture);
    SDL_RenderSetViewport(renderer, NULL);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (assets != NULL && assets->panel_texture != NULL)
    {
        SDL_RenderCopy(renderer, assets->panel_texture, NULL, &(SDL_Rect){0, 0, COCKPIT_FMC_TEXTURE_WIDTH, COCKPIT_FMC_TEXTURE_HEIGHT});
        fmc_display_render_screen_only(renderer, font, data, &COCKPIT_FMC_SCREEN_RECT);
        fmc_display_render_exec_light_only(renderer, data);
        fmc_display_render_hover_only(renderer, state);
    }
    else
    {
        fmc_display_render(renderer, font, assets, state, data);
    }
}

static void update_module_textures(
    SDL_Renderer *renderer,
    TTF_Font *font,
    Cockpit_RenderTargets *targets,
    int refresh_pfd,
    const PFD_Data *pfd_data,
    const ND_Data *nd_data,
    const AircraftSystems_Data *systems_data,
    const FMC_Display_Assets *fmc_assets,
    const FMC_Event_State *fmc_state,
    const FMC_Data *fmc_data,
    Uint32 *pfd_render_elapsed_ms)
{
    if (pfd_render_elapsed_ms != NULL)
    {
        *pfd_render_elapsed_ms = 0;
    }
    if (refresh_pfd)
    {
        const Uint32 pfd_render_start = SDL_GetTicks();
        render_to_texture(renderer, targets->pfd_texture, render_pfd_adapter, font, pfd_data);
        if (pfd_render_elapsed_ms != NULL)
        {
            *pfd_render_elapsed_ms = SDL_GetTicks() - pfd_render_start;
        }
    }
    render_to_texture(renderer, targets->nd_texture, render_nd_adapter, font, nd_data);
    render_to_texture(renderer, targets->eicas1_texture, render_eicas1_adapter, font, systems_data);
    render_to_texture(renderer, targets->eicas2_texture, render_eicas2_adapter, font, systems_data);
    render_fmc_to_texture(renderer, targets->fmc_texture, font, fmc_assets, fmc_state, fmc_data);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderSetViewport(renderer, NULL);
}

static void update_scene_texture(
    SDL_Renderer *renderer,
    TTF_Font *font,
    Cockpit_RenderTargets *targets,
    const Cockpit_Layout *layout,
    SDL_Texture *background_texture,
    const CockpitAlarmState *alarm_state,
    Uint32 ticks)
{
    int scene_width = 0;
    int scene_height = 0;
    float scene_scale = 1.0f;

    if (renderer == NULL || targets == NULL || targets->scene_texture == NULL || layout == NULL)
    {
        return;
    }
    if (SDL_QueryTexture(targets->scene_texture, NULL, NULL, &scene_width, &scene_height) != 0 ||
        scene_width <= 0 || scene_height <= 0)
    {
        return;
    }
    scene_scale = (float)scene_width / (float)layout->world_width;

    SDL_SetRenderTarget(renderer, targets->scene_texture);
    SDL_RenderSetViewport(renderer, NULL);
    SDL_RenderSetScale(renderer, scene_scale, scene_scale);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    cockpit_ui_render_scene(
        renderer,
        font,
        layout,
        background_texture,
        targets->pfd_texture,
        targets->nd_texture,
        targets->eicas1_texture,
        targets->nd_texture,
        targets->pfd_texture,
        targets->eicas2_texture,
        targets->fmc_texture);

    cockpit_alarm_render(renderer, layout, alarm_state, ticks);

    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderSetViewport(renderer, NULL);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
}

static void apply_sim_snapshot_to_pfd(PFD_Data *data, const SimSnapshot *snapshot)
{
    if (data == NULL || snapshot == NULL)
    {
        return;
    }

    data->pitch = snapshot->pitch;
    data->roll = snapshot->roll;
    data->yaw = snapshot->yaw;
    data->altitude = snapshot->altitude;
    data->agl_altitude = snapshot->agl_altitude;
    data->throttle = snapshot->throttle;
    data->airspeed_current = snapshot->airspeed;
    data->airspeed_target = snapshot->airspeed_target;
    data->vertical_speed = snapshot->vertical_speed;
    data->heading = snapshot->heading;
    data->heading_target = snapshot->heading_target;
    data->altitude_target = snapshot->altitude_target;
    data->autopilot_on = 1;
    data->simulation_time = snapshot->sim_time;
    data->using_file_data = 1;
    data->file_sample_index = snapshot->pfd_frame_index;
    data->file_sample_accumulator = 0.0f;
    snprintf(data->flight_mode, sizeof(data->flight_mode), "%s", "SIM SNAP");
}

static void apply_sim_snapshot_to_nd(ND_Data *data, const SimSnapshot *snapshot)
{
    if (data == NULL || snapshot == NULL)
    {
        return;
    }

    data->latitude = snapshot->latitude;
    data->longitude = snapshot->longitude;
    data->heading = snapshot->heading;
    data->track = snapshot->track;
    data->ground_speed = snapshot->ground_speed;
    data->true_air_speed = snapshot->true_air_speed;
    data->simulation_time = snapshot->sim_time;
    data->data_frame_index = snapshot->nd_frame_index;
    data->data_frame_elapsed = snapshot->sim_time;
    nd_data_recalculate_nav_points(data);
}

static void apply_sim_snapshot_to_cockpit_modules(
    const SimSnapshot *snapshot,
    PFD_Data *pfd_data,
    ND_Data *nd_data,
    AircraftSystems_Data *systems_data)
{
    apply_sim_snapshot_to_pfd(pfd_data, snapshot);
    apply_sim_snapshot_to_nd(nd_data, snapshot);
    apply_sim_snapshot_to_aircraft_systems(systems_data, snapshot);
}

static void print_data_source_summary(
    int use_unified_data,
    const SimDataCenter *sim_data_center,
    const PFD_Data *pfd_data,
    const ND_Data *nd_data,
    int eicas_data_loaded,
    const FMC_Data *fmc_data,
    int fmc_uses_unified_route)
{
    const SimPlannedRoute *route = sim_data_center_route(sim_data_center);

    printf("Cockpit Data Sources: X-Plane live bridge enabled at %s:%u; local loaders remain as fallback.\n",
           XPLANE_LIVE_DEFAULT_IP,
           XPLANE_LIVE_DEFAULT_PORT);
    printf("Cockpit Data Sources: unified SimDataCenter is %s.\n",
           use_unified_data ? "active" : "not available");
    printf("Cockpit Data Sources: PFD=%s; ND=%s%s%s%s; EICAS=%s; FMC=%s.\n",
           pfd_data != NULL && pfd_data->using_file_data ? "assets/pfd.dat" : "mock fallback",
           nd_data != NULL && nd_data->data_file_loaded ? "assets/nd.dat" : "mock flight fallback",
           nd_data != NULL && nd_data->earth_fix_loaded ? " + earth_fix.dat" : "",
           nd_data != NULL && nd_data->earth_nav_loaded ? " + earth_nav.dat" : "",
           nd_data != NULL && nd_data->apt_loaded ? " + apt.dat" : "",
           eicas_data_loaded ? "assets/eicas1.dat/assets/eicas2.dat" : "mock fallback",
           fmc_uses_unified_route ? "UnifiedRoute" : (fmc_data != NULL && fmc_data->route_loaded_from_file ? fmc_data->fms_plan_path : "default mock route"));
    if (route != NULL)
    {
        printf("Cockpit Route: source=%s origin=%s destination=%s points=%d first=%s last=%s FMC=%s.\n",
               sim_data_center_route_source_name(route->source),
               route->origin,
               route->destination,
               route->point_count,
               route->point_count > 0 ? route->points[0].ident : "----",
               route->point_count > 0 ? route->points[route->point_count - 1].ident : "----",
               fmc_uses_unified_route ? "UnifiedRoute" : "FMC fallback");
    }
    else
    {
        printf("Cockpit Route: unified route unavailable; FMC keeps fallback route.\n");
    }
    printf("Cockpit Data Sync: X-Plane live data has priority; SimSnapshot/local data are fallback; FMC route uses UnifiedRoute when available.\n");
    fflush(stdout);
}

static void print_cockpit_sync_check(
    const SimSnapshot *snapshot,
    const SimDataCenter *sim_data_center,
    int fmc_uses_unified_route)
{
    const SimPlannedRoute *route = sim_data_center_route(sim_data_center);
    printf("[Cockpit Sync Check]\n");
    printf("sim_time=%.2f\n", snapshot != NULL ? snapshot->sim_time : 0.0f);
    printf("pfd/nd/eicas source=%s\n", snapshot != NULL ? "SimSnapshot" : "fallback");
    if (route != NULL)
    {
        printf("route: source=%s, origin=%s, destination=%s, points=%d, first=%s, last=%s\n",
               sim_data_center_route_source_name(route->source),
               route->origin,
               route->destination,
               route->point_count,
               route->point_count > 0 ? route->points[0].ident : "----",
               route->point_count > 0 ? route->points[route->point_count - 1].ident : "----");
    }
    else
    {
        printf("route: unavailable\n");
    }
    printf("fmc: source=%s\n", fmc_uses_unified_route ? "UnifiedRoute" : "FMCFallback");
    fflush(stdout);
}

static void reset_camera(Cockpit_Camera *camera, int window_width, int window_height, int world_width, int world_height)
{
    if (camera == NULL || world_width <= 0 || world_height <= 0)
    {
        return;
    }

    const float scale_x = (float)window_width / (float)world_width;
    const float scale_y = (float)window_height / (float)world_height;
    camera->scale = scale_x < scale_y ? scale_x : scale_y;
    camera->offset_x = ((float)window_width - (float)world_width * camera->scale) * 0.5f;
    camera->offset_y = ((float)window_height - (float)world_height * camera->scale) * 0.5f;
}

static void screen_to_world(int screen_x, int screen_y, const Cockpit_Camera *camera, float *world_x, float *world_y)
{
    if (camera == NULL || world_x == NULL || world_y == NULL)
    {
        return;
    }

    *world_x = ((float)screen_x - camera->offset_x) / camera->scale;
    *world_y = ((float)screen_y - camera->offset_y) / camera->scale;
}

static void zoom_camera_at(Cockpit_Camera *camera, int mouse_x, int mouse_y, float zoom_factor)
{
    if (camera == NULL)
    {
        return;
    }

    const float old_scale = camera->scale;
    const float new_scale = clamp_float(old_scale * zoom_factor, COCKPIT_MIN_SCALE, COCKPIT_MAX_SCALE);
    if (new_scale == old_scale)
    {
        return;
    }

    const float world_x = ((float)mouse_x - camera->offset_x) / old_scale;
    const float world_y = ((float)mouse_y - camera->offset_y) / old_scale;
    camera->scale = new_scale;
    camera->offset_x = (float)mouse_x - world_x * new_scale;
    camera->offset_y = (float)mouse_y - world_y * new_scale;
}

static void handle_fmc_text_input(FMC_Data *data, const char *text)
{
    if (data == NULL || text == NULL)
    {
        return;
    }

    for (int i = 0; text[i] != '\0'; ++i)
    {
        fmc_data_append_char(data, text[i]);
    }
}

static void handle_fmc_keydown(FMC_Data *data, SDL_Keycode key)
{
    if (data == NULL)
    {
        return;
    }

    if (key == SDLK_BACKSPACE)
    {
        fmc_data_backspace(data);
        fmc_xplane_send_command("sim/FMS/clear");
    }
    else if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) &&
             data->current_page == FMC_PAGE_ROUTE)
    {
        fmc_data_exec_route_selection(data);
    }
    else if (key == SDLK_F1)
    {
        fmc_data_set_page(data, FMC_PAGE_INDEX);
        fmc_xplane_send_command("sim/FMS/init");
    }
    else if (key == SDLK_F2)
    {
        fmc_data_set_page(data, FMC_PAGE_ROUTE);
        fmc_xplane_send_command("sim/FMS/fpln");
    }
    else if (key == SDLK_F3)
    {
        fmc_data_set_page(data, FMC_PAGE_DEP_ARR);
        fmc_xplane_send_command("sim/FMS/dep_arr");
    }
    else if (key == SDLK_F4)
    {
        fmc_data_set_page(data, FMC_PAGE_PERF);
    }
    else if (key == SDLK_F5)
    {
        fmc_data_set_page(data, FMC_PAGE_LEGS);
        fmc_xplane_send_command("sim/FMS/legs");
    }
}

static int cockpit_view_shows_nd(Cockpit_ViewMode view_mode)
{
    return view_mode == COCKPIT_VIEW_MAIN || view_mode == COCKPIT_VIEW_ND_ZOOM;
}

static void log_fmc_draft_route(const FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    const int draft_point_count = (data->origin[0] != '\0' ? 1 : 0) + data->route_count;
    printf("FMC Route Diagnostic: draft point_count=%d origin=%s destination=%s pending_mod=%d.\n",
           draft_point_count,
           data->origin[0] != '\0' ? data->origin : "----",
           data->destination[0] != '\0' ? data->destination : "----",
           fmc_data_route_has_uncommitted_changes(data));
    if (data->origin[0] != '\0')
    {
        printf("FMC Route Diagnostic: draft[0]=%s has_position=%d lat=%.6f lon=%.6f.\n",
               data->origin,
               data->origin_has_position,
               data->origin_latitude,
               data->origin_longitude);
    }
    for (int i = 0; i < data->route_count; ++i)
    {
        printf("FMC Route Diagnostic: draft[%d]=%s has_position=%d lat=%.6f lon=%.6f.\n",
               i + 1,
               data->route_points[i],
               data->route_has_position[i],
               data->route_latitudes[i],
               data->route_longitudes[i]);
    }
}

static void log_planned_route(const SimPlannedRoute *route, const char *stage)
{
    if (route == NULL)
    {
        printf("FMC Route Diagnostic: %s planned_route unavailable.\n", stage != NULL ? stage : "route");
        return;
    }

    printf("FMC Route Diagnostic: %s planned_route point_count=%d active_waypoint_index=%d.\n",
           stage != NULL ? stage : "route",
           route->point_count,
           route->active_waypoint_index);
    for (int i = 0; i < route->point_count; ++i)
    {
        const SimRoutePoint *point = &route->points[i];
        printf("FMC Route Diagnostic: %s planned[%d]=%s type=%s has_position=%d lat=%.6f lon=%.6f.\n",
               stage != NULL ? stage : "route",
               i,
               point->ident,
               point->type,
               point->has_position,
               point->latitude,
               point->longitude);
    }
}

static int sync_nd_route_from_sim_center(ND_Data *data, const SimDataCenter *sim_data_center, int force_check, const char *reason)
{
    if (data == NULL || sim_data_center == NULL)
    {
        return 0;
    }

    const int revision = sim_data_center_route_revision(sim_data_center);
    const SimPlannedRoute *route = sim_data_center_route(sim_data_center);
    if (force_check)
    {
        printf("ND activated: current route revision=%d cached route revision=%d reason=%s.\n",
               revision,
               data->route_cached_revision,
               reason != NULL ? reason : "activate");
        log_planned_route(route, "ND activation");
    }

    return nd_data_sync_planned_route(data, route, revision, force_check);
}

static int route_has_drawable_coordinates(const SimPlannedRoute *route)
{
    if (route == NULL || !route->valid || route->point_count <= 0)
    {
        return 0;
    }

    int drawable_count = 0;
    for (int i = 0; i < route->point_count; ++i)
    {
        const SimRoutePoint *point = &route->points[i];
        if (point->has_position &&
            point->latitude >= -90.0 && point->latitude <= 90.0 &&
            point->longitude >= -180.0 && point->longitude <= 180.0 &&
            (point->latitude != 0.0 || point->longitude != 0.0))
        {
            drawable_count++;
        }
    }

    return drawable_count > 0;
}

static int submit_fmc_route_to_sim_center(FMC_Data *data, SimDataCenter *sim_data_center)
{
    if (data == NULL || sim_data_center == NULL)
    {
        return 0;
    }

    const int before_revision = sim_data_center_route_revision(sim_data_center);
    log_fmc_draft_route(data);
    printf("FMC Route: EXEC submit requested; FMC_Data=%p SimDataCenter=%p planned_route=%p draft_points=%d pending_mod=%d clear_pending=%d revision_before=%d.\n",
           (void *)data, (void *)sim_data_center, (void *)&sim_data_center->planned_route,
           data->route_count,
           fmc_data_route_has_uncommitted_changes(data),
           fmc_data_route_clear_pending(data),
           before_revision);

    if (fmc_data_route_clear_pending(data))
    {
        sim_data_center_clear_route(sim_data_center);
        fmc_data_mark_route_committed(data);
        snprintf(data->message, sizeof(data->message), "RTE CLEARED");
        printf("FMC Route: clear committed; planned_route points=0 revision=%d pending_mod=%d.\n",
               sim_data_center_route_revision(sim_data_center),
               fmc_data_route_has_uncommitted_changes(data));
        return 1;
    }

    SimPlannedRoute route;
    if (!fmc_data_export_planned_route(data, &route) ||
        route.origin[0] == '\0' ||
        route.destination[0] == '\0' ||
        route.point_count < 2 ||
        !route_has_drawable_coordinates(&route))
    {
        snprintf(data->message, sizeof(data->message), "RTE EXEC FAIL");
        printf("FMC Route: EXEC failed; draft origin=%s destination=%s exported_points=%d has_coordinates=%d revision=%d pending_mod=%d.\n",
               route.origin[0] != '\0' ? route.origin : "----",
               route.destination[0] != '\0' ? route.destination : "----",
               route.point_count,
               route.has_coordinates,
               sim_data_center_route_revision(sim_data_center),
               fmc_data_route_has_uncommitted_changes(data));
        return 0;
    }

    sim_data_center_set_route(sim_data_center, &route);
    fmc_data_mark_route_committed(data);
    fmc_data_sync_route_to_xplane(data);
    snprintf(data->message, sizeof(data->message), "RTE %s-%s EXEC", route.origin, route.destination);
    printf("FMC Route: EXEC committed; origin=%s destination=%s planned_route points=%d revision=%d pending_mod=%d.\n",
           route.origin,
           route.destination,
           route.point_count,
           sim_data_center_route_revision(sim_data_center),
           fmc_data_route_has_uncommitted_changes(data));
    log_planned_route(sim_data_center_route(sim_data_center), "EXEC committed");
    return 1;
}

static int handle_nd_map_keydown(ND_Data *data, SDL_Keycode key)
{
    if (data == NULL)
    {
        return 0;
    }

    switch (key)
    {
    case SDLK_1:
    case SDLK_KP_1:
        nd_data_toggle_map_layer_visible(data, ND_MAP_LAYER_WPT);
        return 1;
    case SDLK_2:
    case SDLK_KP_2:
        nd_data_toggle_map_layer_visible(data, ND_MAP_LAYER_ARPT);
        return 1;
    case SDLK_3:
    case SDLK_KP_3:
        nd_data_toggle_map_layer_visible(data, ND_MAP_LAYER_STA);
        return 1;
    case SDLK_l:
        nd_data_toggle_map_labels_visible(data);
        return 1;
    default:
        break;
    }

    return 0;
}

static void map_zoom_click_to_fmc(int screen_x, int screen_y, SDL_Rect zoom_rect, int *fmc_x, int *fmc_y)
{
    if (fmc_x == NULL || fmc_y == NULL)
    {
        return;
    }

    *fmc_x = (screen_x - zoom_rect.x) * COCKPIT_FMC_TEXTURE_WIDTH / zoom_rect.w;
    *fmc_y = (screen_y - zoom_rect.y) * COCKPIT_FMC_TEXTURE_HEIGHT / zoom_rect.h;
}

static int handle_cockpit_fmc_panel_button(
    FMC_Event_State *state,
    FMC_Data *data,
    SimDataCenter *sim_data_center,
    int *fmc_uses_unified_route,
    int fmc_x,
    int fmc_y)
{
    if (data == NULL)
    {
        return 0;
    }

    const int button_count = fmc_key_button_count();
    for (int i = 0; i < button_count; ++i)
    {
        const FMC_Button *button = fmc_key_button_at(i);
        if (button != NULL && fmc_key_button_contains_base_point(button, fmc_x, fmc_y))
        {
            if (state != NULL)
            {
                state->hovered_button_index = i;
                state->hovered_button = button->id;
            }

            if (button->id == FMC_BUTTON_EXEC)
            {
                const int committed = submit_fmc_route_to_sim_center(data, sim_data_center);
                if (fmc_uses_unified_route != NULL)
                {
                    *fmc_uses_unified_route = committed && sim_data_center_has_route(sim_data_center);
                }
                return 1;
            }

            break;
        }
    }

    const int handled = fmc_event_handle_mouse_button_base(state, data, fmc_x, fmc_y);
    return handled;
}

static Cockpit_ViewMode cockpit_module_hit_test(const Cockpit_Layout *layout, float world_x, float world_y)
{
    if (layout == NULL)
    {
        return COCKPIT_VIEW_MAIN;
    }

    if ((world_x >= (float)layout->capt_pfd_rect.x &&
         world_x < (float)(layout->capt_pfd_rect.x + layout->capt_pfd_rect.w) &&
         world_y >= (float)layout->capt_pfd_rect.y &&
         world_y < (float)(layout->capt_pfd_rect.y + layout->capt_pfd_rect.h)) ||
        (world_x >= (float)layout->fo_pfd_rect.x &&
         world_x < (float)(layout->fo_pfd_rect.x + layout->fo_pfd_rect.w) &&
         world_y >= (float)layout->fo_pfd_rect.y &&
         world_y < (float)(layout->fo_pfd_rect.y + layout->fo_pfd_rect.h)))
    {
        return COCKPIT_VIEW_PFD_ZOOM;
    }

    if ((world_x >= (float)layout->capt_nd_rect.x &&
         world_x < (float)(layout->capt_nd_rect.x + layout->capt_nd_rect.w) &&
         world_y >= (float)layout->capt_nd_rect.y &&
         world_y < (float)(layout->capt_nd_rect.y + layout->capt_nd_rect.h)) ||
        (world_x >= (float)layout->fo_nd_rect.x &&
         world_x < (float)(layout->fo_nd_rect.x + layout->fo_nd_rect.w) &&
         world_y >= (float)layout->fo_nd_rect.y &&
         world_y < (float)(layout->fo_nd_rect.y + layout->fo_nd_rect.h)))
    {
        return COCKPIT_VIEW_ND_ZOOM;
    }

    if (world_x >= (float)layout->eicas1_rect.x &&
        world_x < (float)(layout->eicas1_rect.x + layout->eicas1_rect.w) &&
        world_y >= (float)layout->eicas1_rect.y &&
        world_y < (float)(layout->eicas1_rect.y + layout->eicas1_rect.h))
    {
        return COCKPIT_VIEW_EICAS1_ZOOM;
    }

    if (world_x >= (float)layout->eicas2_rect.x &&
        world_x < (float)(layout->eicas2_rect.x + layout->eicas2_rect.w) &&
        world_y >= (float)layout->eicas2_rect.y &&
        world_y < (float)(layout->eicas2_rect.y + layout->eicas2_rect.h))
    {
        return COCKPIT_VIEW_EICAS2_ZOOM;
    }

    return COCKPIT_VIEW_MAIN;
}

static int point_in_rect(int x, int y, const SDL_Rect *rect)
{
    return rect != NULL &&
           x >= rect->x &&
           x < rect->x + rect->w &&
           y >= rect->y &&
           y < rect->y + rect->h;
}

static void render_window(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Cockpit_RenderTargets *targets,
    const Cockpit_Layout *layout,
    const Cockpit_Camera *camera,
    Cockpit_ViewMode view_mode,
    Cockpit_FmcSide selected_fmc,
    SDL_Texture *fmc_background_texture,
    int show_fmc_debug,
    int window_width,
    int window_height)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Rect scene_dest = {
        (int)camera->offset_x,
        (int)camera->offset_y,
        (int)((float)layout->world_width * camera->scale),
        (int)((float)layout->world_height * camera->scale)};

    SDL_RenderCopy(renderer, targets->scene_texture, NULL, &scene_dest);

    if (view_mode == COCKPIT_VIEW_FMC_ZOOM)
    {
        SDL_Rect zoom_rect = cockpit_ui_fmc_zoom_rect(window_width, window_height);
        cockpit_ui_render_fmc_zoom_overlay(renderer, font, targets->fmc_texture, fmc_background_texture, zoom_rect, selected_fmc, show_fmc_debug);
    }
    else if (view_mode == COCKPIT_VIEW_PFD_ZOOM)
    {
        SDL_Rect zoom_rect = cockpit_ui_module_zoom_rect(window_width, window_height, COCKPIT_PFD_TEXTURE_WIDTH, COCKPIT_PFD_TEXTURE_HEIGHT);
        cockpit_ui_render_module_zoom_overlay(renderer, font, targets->pfd_texture, zoom_rect, "PFD");
    }
    else if (view_mode == COCKPIT_VIEW_ND_ZOOM)
    {
        SDL_Rect zoom_rect = cockpit_ui_module_zoom_rect(window_width, window_height, COCKPIT_ND_TEXTURE_WIDTH, COCKPIT_ND_TEXTURE_HEIGHT);
        cockpit_ui_render_module_zoom_overlay(renderer, font, targets->nd_texture, zoom_rect, "ND");
    }
    else if (view_mode == COCKPIT_VIEW_EICAS1_ZOOM)
    {
        SDL_Rect zoom_rect = cockpit_ui_module_zoom_rect(window_width, window_height, COCKPIT_EICAS_TEXTURE_WIDTH, COCKPIT_EICAS_TEXTURE_HEIGHT);
        cockpit_ui_render_module_zoom_overlay(renderer, font, targets->eicas1_texture, zoom_rect, "EICAS1");
    }
    else if (view_mode == COCKPIT_VIEW_EICAS2_ZOOM)
    {
        SDL_Rect zoom_rect = cockpit_ui_module_zoom_rect(window_width, window_height, COCKPIT_EICAS_TEXTURE_WIDTH, COCKPIT_EICAS_TEXTURE_HEIGHT);
        cockpit_ui_render_module_zoom_overlay(renderer, font, targets->eicas2_texture, zoom_rect, "EICAS2");
    }

    SDL_RenderPresent(renderer);
}

static int cockpit_main_run_internal(SimDataCenter *sim_data_center, const Cockpit_XPlaneConfig *xplane_config)
{
    Cockpit_XPlaneConfig resolved_xplane_config;

    if (sim_data_center == NULL)
    {
        return -1;
    }

    if (xplane_config == NULL)
    {
        cockpit_xplane_config_init(&resolved_xplane_config, 0, NULL);
        xplane_config = &resolved_xplane_config;
    }
    cockpit_xplane_config_apply_env(xplane_config);
    printf("Cockpit X-Plane: using %s:%u.\n", xplane_config->ip, xplane_config->port);

    // 使用最近邻缩放，避免纹理缩放时产生模糊
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    const Uint32 cockpit_sdl_flags = SDL_INIT_VIDEO | SDL_INIT_TIMER;
    const int cockpit_owns_sdl = SDL_WasInit(0) == 0;
    const int cockpit_owns_ttf = TTF_WasInit() == 0;
    int cockpit_initialized_sdl = 0;

    cockpit_startup_log(1, "Cockpit startup: entered (owns_sdl=%d, owns_ttf=%d).", cockpit_owns_sdl, cockpit_owns_ttf);

    if ((SDL_WasInit(cockpit_sdl_flags) & cockpit_sdl_flags) != cockpit_sdl_flags)
    {
        if (SDL_InitSubSystem(cockpit_sdl_flags) != 0)
        {
            printf("SDL_InitSubSystem failed: %s\n", SDL_GetError());
            cockpit_startup_log(0, "FAILED: SDL_InitSubSystem: %s", SDL_GetError());
            cockpit_show_startup_error("SDL initialization", SDL_GetError());
            return -1;
        }
        cockpit_initialized_sdl = 1;
    }
    cockpit_startup_log(0, "SDL video/timer ready.");

    if (cockpit_owns_ttf && TTF_Init() != 0)
    {
        printf("TTF_Init failed: %s\n", TTF_GetError());
        cockpit_startup_log(0, "FAILED: TTF_Init: %s", TTF_GetError());
        cockpit_show_startup_error("font subsystem initialization", TTF_GetError());
        if (cockpit_owns_sdl)
        {
            SDL_Quit();
        }
        else if (cockpit_initialized_sdl)
        {
            SDL_QuitSubSystem(cockpit_sdl_flags);
        }
        return -1;
    }
    cockpit_startup_log(0, "SDL_ttf ready.");

    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0)
    {
        printf("IMG_Init PNG failed: %s\n", IMG_GetError());
        cockpit_startup_log(0, "WARNING: IMG_Init PNG: %s", IMG_GetError());
    }

    SDL_Window *window = SDL_CreateWindow(
        "Cockpit - Integrated Flight Deck",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        COCKPIT_WINDOW_WIDTH,
        COCKPIT_WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window == NULL)
    {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        cockpit_startup_log(0, "FAILED: SDL_CreateWindow: %s", SDL_GetError());
        cockpit_show_startup_error("window creation", SDL_GetError());
        IMG_Quit();
        if (cockpit_owns_ttf)
        {
            TTF_Quit();
        }
        if (cockpit_owns_sdl)
        {
            SDL_Quit();
        }
        else if (cockpit_initialized_sdl)
        {
            SDL_QuitSubSystem(cockpit_sdl_flags);
        }
        return -1;
    }
    cockpit_startup_log(0, "Cockpit window created.");

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (renderer == NULL)
    {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
    }

    if (renderer == NULL)
    {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        cockpit_startup_log(0, "FAILED: SDL_CreateRenderer: %s", SDL_GetError());
        cockpit_show_startup_error("renderer creation", SDL_GetError());
        SDL_DestroyWindow(window);
        IMG_Quit();
        if (cockpit_owns_ttf)
        {
            TTF_Quit();
        }
        if (cockpit_owns_sdl)
        {
            SDL_Quit();
        }
        else if (cockpit_initialized_sdl)
        {
            SDL_QuitSubSystem(cockpit_sdl_flags);
        }
        return -1;
    }
    cockpit_startup_log(0, "Cockpit renderer created.");

    TTF_Font *font = open_cockpit_font();
    if (font == NULL)
    {
        printf("TTF_OpenFont failed: %s\n", TTF_GetError());
        cockpit_startup_log(0, "FAILED: TTF_OpenFont: %s", TTF_GetError());
        cockpit_show_startup_error("font loading", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        if (cockpit_owns_ttf)
        {
            TTF_Quit();
        }
        if (cockpit_owns_sdl)
        {
            SDL_Quit();
        }
        else if (cockpit_initialized_sdl)
        {
            SDL_QuitSubSystem(cockpit_sdl_flags);
        }
        return -1;
    }
    cockpit_startup_log(0, "Cockpit font loaded.");

    int world_width = 8026;
    int world_height = 3136;
    SDL_Texture *background_texture = load_texture_optional(renderer, cockpit_layout_background_path(), &world_width, &world_height);
    SDL_Texture *fmc_background_texture = load_texture_optional(renderer, cockpit_layout_fmc_background_path(), NULL, NULL);

    Cockpit_Layout layout = cockpit_layout_default(world_width, world_height);
    Cockpit_RenderTargets targets = {NULL, NULL, NULL, NULL, NULL, NULL};
    if (!create_render_targets(renderer, &targets, layout.world_width, layout.world_height))
    {
        printf("SDL_CreateTexture target failed: %s\n", SDL_GetError());
        cockpit_startup_log(0, "FAILED: create_render_targets: %s", SDL_GetError());
        cockpit_show_startup_error("render-target creation", SDL_GetError());
        destroy_render_targets(&targets);
        if (background_texture != NULL)
        {
            SDL_DestroyTexture(background_texture);
        }
        if (fmc_background_texture != NULL)
        {
            SDL_DestroyTexture(fmc_background_texture);
        }
        TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        if (cockpit_owns_ttf)
        {
            TTF_Quit();
        }
        if (cockpit_owns_sdl)
        {
            SDL_Quit();
        }
        else if (cockpit_initialized_sdl)
        {
            SDL_QuitSubSystem(cockpit_sdl_flags);
        }
        return -1;
    }
    {
        int scene_width = 0;
        int scene_height = 0;
        SDL_QueryTexture(targets.scene_texture, NULL, NULL, &scene_width, &scene_height);
        cockpit_startup_log(0, "Render targets created (world=%d x %d, scene=%d x %d).",
                            layout.world_width, layout.world_height, scene_width, scene_height);
    }

    PFD_Data pfd_data;
    ND_Data nd_data;
    AircraftSystems_Data systems_data;
    EICAS_Data eicas_data;
    XPlaneLiveData xplane_live_data;
    FMC_Data fmc_data;
    int eicas_data_loaded = 0;
    const int sim_data_ready = sim_data_center_is_ready(sim_data_center);
    const SimSnapshot *sim_snapshot = sim_data_center_snapshot(sim_data_center);
    pfd_data_init(&pfd_data);
    nd_data_init(&nd_data);
    aircraft_systems_data_init(&systems_data);
    eicas_data_init(&eicas_data);
    xplane_live_data_init(&xplane_live_data, xplane_config->ip, xplane_config->port);
    if (sim_data_ready && sim_snapshot != NULL)
    {
        eicas_data_loaded = sim_data_center_has_eicas_data(sim_data_center);
        apply_sim_snapshot_to_cockpit_modules(sim_snapshot, &pfd_data, &nd_data, &systems_data);
        printf("Cockpit: unified SimDataCenter initialized; embedded displays will use one SimSnapshot.\n");
        if (!sim_data_center_has_pfd_data(sim_data_center) ||
            !sim_data_center_has_nd_data(sim_data_center) ||
            !sim_data_center_has_eicas_data(sim_data_center))
        {
            printf("Cockpit: unified snapshot has partial source data; missing fields use SimSnapshot defaults.\n");
        }
        fflush(stdout);
    }
    else
    {
        printf("Cockpit: unified SimDataCenter initialization failed; falling back to legacy/module data path.\n");
        eicas_data_loaded = eicas_data_load_files(&eicas_data, "assets/eicas1.dat", "assets/eicas2.dat");
        if (eicas_data_loaded)
        {
            eicas_data_apply_to_aircraft_systems(&eicas_data, &systems_data);
        }
        else
        {
            printf("Cockpit EICAS: using mock fallback data.\n");
            fflush(stdout);
        }
    }
    fmc_data_init(&fmc_data);
    printf("Cockpit FMC: FMC_Data=%p SimDataCenter=%p planned_route=%p revision=%d.\n",
           (void *)&fmc_data, (void *)sim_data_center, (void *)&sim_data_center->planned_route,
           sim_data_center_route_revision(sim_data_center));
    int fmc_uses_unified_route = 0;
    if (sim_data_ready && sim_data_center_has_route(sim_data_center))
    {
        const SimPlannedRoute *route = sim_data_center_route(sim_data_center);
        fmc_uses_unified_route = fmc_data_apply_planned_route(&fmc_data, route);
        printf("Cockpit FMC: unified route %s, source=%s origin=%s destination=%s points=%d.\n",
               fmc_uses_unified_route ? "enabled" : "unavailable",
               route != NULL ? sim_data_center_route_source_name(route->source) : "NONE",
               route != NULL ? route->origin : "----",
               route != NULL ? route->destination : "----",
               route != NULL ? route->point_count : 0);
        fflush(stdout);
    }
    else
    {
        printf("Cockpit FMC: unified route unavailable; keeping existing FMC state.\n");
        fflush(stdout);
    }
    if (sim_data_ready)
    {
        sync_nd_route_from_sim_center(&nd_data, sim_data_center, 1, "startup");
    }
    print_data_source_summary(sim_data_ready, sim_data_center, &pfd_data, &nd_data, eicas_data_loaded, &fmc_data, fmc_uses_unified_route);
    cockpit_startup_log(0, "Data initialized (sim_data_ready=%d, eicas_data_loaded=%d).", sim_data_ready, eicas_data_loaded);

    FMC_Display_Assets fmc_display_assets;
    fmc_display_assets_load(renderer, &fmc_display_assets);

    FMC_Event_State fmc_event_state;
    fmc_event_state_init(&fmc_event_state);

    Cockpit_MainState cockpit_state;
    cockpit_alarm_init(&cockpit_state.alarm);

    int window_width = COCKPIT_WINDOW_WIDTH;
    int window_height = COCKPIT_WINDOW_HEIGHT;
    Cockpit_Camera camera;
    reset_camera(&camera, window_width, window_height, layout.world_width, layout.world_height);

    Cockpit_ViewMode view_mode = COCKPIT_VIEW_MAIN;
    Cockpit_FmcSide selected_fmc = COCKPIT_FMC_NONE;
    int show_fmc_debug = 0;
    int suppress_debug_text_input = 0;
    int dragging = 0;
    int last_mouse_x = 0;
    int last_mouse_y = 0;

    SDL_StartTextInput();

    int running = 1;
    cockpit_startup_log(0, "Entering cockpit event loop.");
    SDL_Event event;
    Uint32 last_ticks = SDL_GetTicks();
    Uint32 last_pfd_render_ticks = 0;
    Uint32 last_sync_check_log_ticks = 0;
    Uint32 pfd_perf_window_start_ticks = last_ticks;
    unsigned int pfd_perf_render_count = 0;
    unsigned int pfd_perf_skipped_count = 0;
    Uint64 pfd_perf_total_render_ms = 0;

    while (running)
    {
        const Uint32 frame_start = SDL_GetTicks();
        const Cockpit_ViewMode view_mode_before_events = view_mode;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = 0;
            }
            else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                window_width = event.window.data1;
                window_height = event.window.data2;
            }
            else if (event.type == SDL_MOUSEWHEEL && view_mode == COCKPIT_VIEW_MAIN)
            {
                int mouse_x = 0;
                int mouse_y = 0;
                SDL_GetMouseState(&mouse_x, &mouse_y);
                zoom_camera_at(&camera, mouse_x, mouse_y, event.wheel.y > 0 ? 1.12f : 0.89f);
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
            {
                float alarm_world_x = 0.0f;
                float alarm_world_y = 0.0f;
                screen_to_world(event.button.x, event.button.y, &camera, &alarm_world_x, &alarm_world_y);
                if (view_mode == COCKPIT_VIEW_MAIN && cockpit_alarm_handle_click(&cockpit_state.alarm, &sim_data_center->alert_manager, alarm_world_x, alarm_world_y, &layout))
                {
                    dragging = 0;
                }
                else if (view_mode == COCKPIT_VIEW_FMC_ZOOM)
                {
                    SDL_Rect zoom_rect = cockpit_ui_fmc_zoom_rect(window_width, window_height);
                    if (!point_in_rect(event.button.x, event.button.y, &zoom_rect))
                    {
                        view_mode = COCKPIT_VIEW_MAIN;
                        selected_fmc = COCKPIT_FMC_NONE;
                    }
                    else
                    {
                        int fmc_x = 0;
                        int fmc_y = 0;
                        map_zoom_click_to_fmc(event.button.x, event.button.y, zoom_rect, &fmc_x, &fmc_y);
                        handle_cockpit_fmc_panel_button(&fmc_event_state,
                                                        &fmc_data,
                                                        sim_data_center,
                                                        &fmc_uses_unified_route,
                                                        fmc_x,
                                                        fmc_y);
                    }
                }
                else if (view_mode == COCKPIT_VIEW_PFD_ZOOM)
                {
                    SDL_Rect zoom_rect = cockpit_ui_module_zoom_rect(window_width, window_height, COCKPIT_PFD_TEXTURE_WIDTH, COCKPIT_PFD_TEXTURE_HEIGHT);
                    if (!point_in_rect(event.button.x, event.button.y, &zoom_rect))
                    {
                        view_mode = COCKPIT_VIEW_MAIN;
                    }
                }
                else if (view_mode == COCKPIT_VIEW_ND_ZOOM)
                {
                    SDL_Rect zoom_rect = cockpit_ui_module_zoom_rect(window_width, window_height, COCKPIT_ND_TEXTURE_WIDTH, COCKPIT_ND_TEXTURE_HEIGHT);
                    if (!point_in_rect(event.button.x, event.button.y, &zoom_rect))
                    {
                        view_mode = COCKPIT_VIEW_MAIN;
                    }
                }
                else if (view_mode == COCKPIT_VIEW_EICAS1_ZOOM)
                {
                    SDL_Rect zoom_rect = cockpit_ui_module_zoom_rect(window_width, window_height, COCKPIT_EICAS_TEXTURE_WIDTH, COCKPIT_EICAS_TEXTURE_HEIGHT);
                    if (!point_in_rect(event.button.x, event.button.y, &zoom_rect))
                    {
                        view_mode = COCKPIT_VIEW_MAIN;
                    }
                }
                else if (view_mode == COCKPIT_VIEW_EICAS2_ZOOM)
                {
                    SDL_Rect zoom_rect = cockpit_ui_module_zoom_rect(window_width, window_height, COCKPIT_EICAS_TEXTURE_WIDTH, COCKPIT_EICAS_TEXTURE_HEIGHT);
                    if (!point_in_rect(event.button.x, event.button.y, &zoom_rect))
                    {
                        view_mode = COCKPIT_VIEW_MAIN;
                    }
                }
                else
                {
                    float world_x = 0.0f;
                    float world_y = 0.0f;
                    screen_to_world(event.button.x, event.button.y, &camera, &world_x, &world_y);
                    Cockpit_FmcSide side = cockpit_layout_hit_test_fmc(&layout, world_x, world_y);
                    if (side != COCKPIT_FMC_NONE)
                    {
                        view_mode = COCKPIT_VIEW_FMC_ZOOM;
                        selected_fmc = side;
                    }
                    else
                    {
                        Cockpit_ViewMode module_view = cockpit_module_hit_test(&layout, world_x, world_y);
                        if (module_view != COCKPIT_VIEW_MAIN)
                        {
                            view_mode = module_view;
                        }
                        else
                        {
                            dragging = 1;
                            last_mouse_x = event.button.x;
                            last_mouse_y = event.button.y;
                        }
                    }
                }
            }
            else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
            {
                dragging = 0;
            }
            else if (event.type == SDL_MOUSEMOTION)
            {
                if (view_mode == COCKPIT_VIEW_FMC_ZOOM)
                {
                    SDL_Rect zoom_rect = cockpit_ui_fmc_zoom_rect(window_width, window_height);
                    if (point_in_rect(event.motion.x, event.motion.y, &zoom_rect))
                    {
                        int fmc_x = 0;
                        int fmc_y = 0;
                        map_zoom_click_to_fmc(event.motion.x, event.motion.y, zoom_rect, &fmc_x, &fmc_y);
                        fmc_event_update_hover_base(&fmc_event_state, fmc_x, fmc_y);
                    }
                    else
                    {
                        fmc_event_state_init(&fmc_event_state);
                    }
                }
                else if (dragging && view_mode == COCKPIT_VIEW_MAIN)
                {
                    camera.offset_x += (float)(event.motion.x - last_mouse_x);
                    camera.offset_y += (float)(event.motion.y - last_mouse_y);
                    last_mouse_x = event.motion.x;
                    last_mouse_y = event.motion.y;
                }
            }
            else if (event.type == SDL_TEXTINPUT)
            {
                if (view_mode == COCKPIT_VIEW_FMC_ZOOM)
                {
                    if (suppress_debug_text_input &&
                        (event.text.text[0] == 'd' || event.text.text[0] == 'D') &&
                        event.text.text[1] == '\0')
                    {
                        suppress_debug_text_input = 0;
                    }
                    else
                    {
                        handle_fmc_text_input(&fmc_data, event.text.text);
                    }
                }
            }
            else if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    if (view_mode != COCKPIT_VIEW_MAIN)
                    {
                        view_mode = COCKPIT_VIEW_MAIN;
                        selected_fmc = COCKPIT_FMC_NONE;
                    }
                    else
                    {
                        running = 0;
                    }
                }
                else if (event.key.keysym.sym == SDLK_w)
                {
                    reset_camera(&camera, window_width, window_height, layout.world_width, layout.world_height);
                }
                else if (event.key.keysym.sym == SDLK_d)
                {
                    show_fmc_debug = !show_fmc_debug;
                    suppress_debug_text_input = view_mode == COCKPIT_VIEW_FMC_ZOOM;
                }
                else if (event.key.repeat == 0 && event.key.keysym.sym == SDLK_F5)
                {
                    sim_data_center_set_demo_alert(sim_data_center, ALERT_TYPE_ENGINE_FIRE, 1);
                }
                else if (event.key.repeat == 0 && event.key.keysym.sym == SDLK_F6)
                {
                    sim_data_center_set_demo_alert(sim_data_center, ALERT_TYPE_CABIN_ALTITUDE, 1);
                }
                else if (event.key.repeat == 0 && event.key.keysym.sym == SDLK_F7)
                {
                    sim_data_center_set_demo_alert(sim_data_center, ALERT_TYPE_CRASH, 1);
                }
                else if (event.key.repeat == 0 && event.key.keysym.sym == SDLK_F8)
                {
                    sim_data_center_clear_demo_alerts(sim_data_center);
                }
                else if (event.key.repeat == 0 &&
                         view_mode == COCKPIT_VIEW_ND_ZOOM &&
                         handle_nd_map_keydown(&nd_data, event.key.keysym.sym))
                {
                }
                else if (view_mode == COCKPIT_VIEW_FMC_ZOOM)
                {
                    if ((event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) &&
                        fmc_data.current_page == FMC_PAGE_ROUTE)
                    {
                        fmc_uses_unified_route = submit_fmc_route_to_sim_center(&fmc_data, sim_data_center) &&
                                                 sim_data_center_has_route(sim_data_center);
                    }
                    else
                    {
                        handle_fmc_keydown(&fmc_data, event.key.keysym.sym);
                    }
                }
            }
        }

        if (view_mode_before_events != view_mode &&
            (view_mode_before_events == COCKPIT_VIEW_FMC_ZOOM || view_mode == COCKPIT_VIEW_FMC_ZOOM))
        {
            printf("Cockpit FMC: %s FMC_Data=%p SimDataCenter=%p revision=%d origin=%s destination=%s draft_points=%d.\n",
                   view_mode == COCKPIT_VIEW_FMC_ZOOM ? "entered" : "left",
                   (void *)&fmc_data, (void *)sim_data_center,
                   sim_data_center_route_revision(sim_data_center),
                   fmc_data.origin[0] != '\0' ? fmc_data.origin : "----",
                   fmc_data.destination[0] != '\0' ? fmc_data.destination : "----",
                   fmc_data.route_count);
        }

        if (sim_data_ready)
        {
            if (!cockpit_view_shows_nd(view_mode_before_events) && cockpit_view_shows_nd(view_mode))
            {
                sync_nd_route_from_sim_center(&nd_data, sim_data_center, 1, "view");
            }
            else if (cockpit_view_shows_nd(view_mode))
            {
                sync_nd_route_from_sim_center(&nd_data, sim_data_center, 0, "visible");
            }
        }

        const Uint32 current_ticks = SDL_GetTicks();
        float delta_time = (float)(current_ticks - last_ticks) / 1000.0f;
        last_ticks = current_ticks;
        if (delta_time > 0.1f)
        {
            delta_time = 0.1f;
        }

        const int live_data_active = xplane_live_data_update(&xplane_live_data, &pfd_data, &nd_data, &eicas_data, &systems_data, delta_time);
        if (sim_data_ready)
        {
            sim_data_center_update(sim_data_center, delta_time);
            sim_snapshot = sim_data_center_snapshot(sim_data_center);
        }

        if (!live_data_active && sim_data_ready && sim_snapshot != NULL)
        {
            apply_sim_snapshot_to_cockpit_modules(sim_snapshot, &pfd_data, &nd_data, &systems_data);
        }
        else if (!live_data_active)
        {
            pfd_data_update_mock(&pfd_data, delta_time);
            nd_data_update_mock(&nd_data, delta_time);
            if (eicas_data_loaded)
            {
                eicas_data_update(&eicas_data, delta_time);
                eicas_data_apply_to_aircraft_systems(&eicas_data, &systems_data);
            }
            else
            {
                aircraft_systems_data_update_mock(&systems_data, delta_time);
            }
        }
        fmc_data_update_mock(&fmc_data, delta_time);
        cockpit_alarm_update(&cockpit_state.alarm, sim_data_center_alerts(sim_data_center));
        if (last_sync_check_log_ticks == 0 ||
            current_ticks - last_sync_check_log_ticks >= COCKPIT_SYNC_CHECK_LOG_MS)
        {
            last_sync_check_log_ticks = current_ticks;
            print_cockpit_sync_check(sim_snapshot, sim_data_center, fmc_uses_unified_route);
        }

        const int refresh_pfd = last_pfd_render_ticks == 0 ||
                                current_ticks - last_pfd_render_ticks >= COCKPIT_PFD_TARGET_FRAME_MS;
        if (refresh_pfd)
        {
            last_pfd_render_ticks = current_ticks;
        }

        Uint32 pfd_render_elapsed_ms = 0;
        update_module_textures(renderer, font, &targets, refresh_pfd, &pfd_data, &nd_data, &systems_data,
                               &fmc_display_assets, &fmc_event_state, &fmc_data, &pfd_render_elapsed_ms);
        if (refresh_pfd)
        {
            ++pfd_perf_render_count;
            pfd_perf_total_render_ms += pfd_render_elapsed_ms;
        }
        else
        {
            ++pfd_perf_skipped_count;
        }
        if (current_ticks - pfd_perf_window_start_ticks >= COCKPIT_PFD_PERF_LOG_MS)
        {
            const float average_pfd_render_ms = pfd_perf_render_count > 0
                                                    ? (float)pfd_perf_total_render_ms / (float)pfd_perf_render_count
                                                    : 0.0f;
            printf("Cockpit PFD Perf: interval=%ums renders=%u skipped=%u avg_render=%.2fms target=%dms.\n",
                   current_ticks - pfd_perf_window_start_ticks,
                   pfd_perf_render_count,
                   pfd_perf_skipped_count,
                   average_pfd_render_ms,
                   COCKPIT_PFD_TARGET_FRAME_MS);
            pfd_perf_window_start_ticks = current_ticks;
            pfd_perf_render_count = 0;
            pfd_perf_skipped_count = 0;
            pfd_perf_total_render_ms = 0;
        }
        update_scene_texture(renderer, font, &targets, &layout, background_texture, &cockpit_state.alarm, current_ticks);
        render_window(renderer, font, &targets, &layout, &camera, view_mode, selected_fmc, fmc_background_texture, show_fmc_debug, window_width, window_height);

        const Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < COCKPIT_TARGET_FRAME_MS)
        {
            SDL_Delay(COCKPIT_TARGET_FRAME_MS - frame_time);
        }
    }

    SDL_StopTextInput();
    xplane_live_data_shutdown(&xplane_live_data);
    cockpit_startup_log(0, "Cockpit event loop ended normally.");
    cockpit_alarm_destroy(&cockpit_state.alarm);
    fmc_data_destroy(&fmc_data);
    fmc_display_assets_destroy(&fmc_display_assets);
    pfd_ui_clear_text_cache(renderer);
    destroy_render_targets(&targets);
    if (background_texture != NULL)
    {
        SDL_DestroyTexture(background_texture);
    }
    if (fmc_background_texture != NULL)
    {
        SDL_DestroyTexture(fmc_background_texture);
    }
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    if (cockpit_owns_ttf)
    {
        TTF_Quit();
    }
    if (cockpit_owns_sdl)
    {
        SDL_Quit();
    }
    else if (cockpit_initialized_sdl)
    {
        SDL_QuitSubSystem(cockpit_sdl_flags);
    }

    return 0;
}

int cockpit_main_run_with_sim_data_center(SimDataCenter *sim_data_center)
{
    if (sim_data_center == NULL || !sim_data_center_is_ready(sim_data_center))
    {
        printf("Cockpit SHARED: SimDataCenter unavailable.\n");
        return -1;
    }

    printf("Cockpit SHARED: SimDataCenter=%p planned_route=%p revision=%d.\n",
           (void *)sim_data_center, (void *)&sim_data_center->planned_route,
           sim_data_center_route_revision(sim_data_center));
    return cockpit_main_run_internal(sim_data_center, NULL);
}

int cockpit_main_run(void)
{
    return cockpit_main_run_with_args(0, NULL);
}

int cockpit_main_run_with_args(int argc, char *argv[])
{
    SimDataCenter *sim_data_center = (SimDataCenter *)malloc(sizeof(*sim_data_center));
    Cockpit_XPlaneConfig xplane_config;
    int result;

    if (sim_data_center == NULL)
    {
        printf("Cockpit STANDALONE: SimDataCenter allocation failed.\n");
        return -1;
    }
    if (!sim_data_center_init(sim_data_center))
    {
        printf("Cockpit STANDALONE: SimDataCenter initialization failed.\n");
        sim_data_center_destroy(sim_data_center);
        free(sim_data_center);
        return -1;
    }

    printf("Cockpit STANDALONE: SimDataCenter=%p planned_route=%p revision=%d.\n",
           (void *)sim_data_center, (void *)&sim_data_center->planned_route,
           sim_data_center_route_revision(sim_data_center));
    cockpit_xplane_config_init(&xplane_config, argc, argv);
    result = cockpit_main_run_internal(sim_data_center, &xplane_config);
    sim_data_center_destroy(sim_data_center);
    free(sim_data_center);
    return result;
}
