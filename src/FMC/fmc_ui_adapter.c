#include "fmc_ui_adapter.h"

#include "fmc_connect.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX_VIATO_NUM
#define MAX_VIATO_NUM 20
#endif

#define FMC_TRANS_ALT_MIN 100
#define FMC_TRANS_ALT_MAX 99000
#define FMC_CRZ_ALT_MIN 100
#define FMC_CRZ_ALT_MAX 99000
#define FMC_TRANS_FL_MIN 1
#define FMC_TRANS_FL_MAX 999
#define FMC_VPA_MIN 0.0f
#define FMC_VPA_MAX 90.0f

extern int rte_index;
extern int dep_arr_index;
extern int dep_arr_type;
extern char show_ariport[20];

int is_string_in_range(const char *str, int min_val, int max_val, int *result);
int is_string_in_range_f(const char *str, float min_val, float max_val, float *result);
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define FMC_ACCESS(path) _access((path), 0)
#define FMC_MKDIR(path) _mkdir((path))
#else
#include <sys/stat.h>
#include <unistd.h>
#define FMC_ACCESS(path) access((path), F_OK)
#define FMC_MKDIR(path) mkdir((path), 0755)
#endif

static void set_text(char *dest, int dest_size, const char *text)
{
    if (dest == NULL || dest_size <= 0)
    {
        return;
    }

    if (text == NULL)
    {
        text = "";
    }

    snprintf(dest, (size_t)dest_size, "%s", text);
}

static int fmc_valid_position(double latitude, double longitude);

typedef struct FMC_ResolvedCoordinate
{
    double latitude;
    double longitude;
    int has_position;
    FMC_CoordinateSource source;
} FMC_ResolvedCoordinate;

typedef struct FMC_AirportCoordinateOverride
{
    const char *ident;
    double latitude;
    double longitude;
} FMC_AirportCoordinateOverride;

static const FMC_AirportCoordinateOverride FMC_AIRPORT_COORDINATE_OVERRIDES[] = {
    {"ZBBB", 40.080111, 116.584556},
};

static const char *fmc_coordinate_source_name(FMC_CoordinateSource source)
{
    switch (source)
    {
    case FMC_COORD_SOURCE_ROUTE:
        return "ROUTE";
    case FMC_COORD_SOURCE_OVERRIDE:
        return "OVERRIDE";
    case FMC_COORD_SOURCE_APT_DAT:
        return "APT_DAT";
    case FMC_COORD_SOURCE_INVALID:
    default:
        return "INVALID";
    }
}

static int fmc_ident_equals(const char *a, const char *b)
{
    if (a == NULL || b == NULL)
    {
        return 0;
    }

    while (*a != '\0' && *b != '\0')
    {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
        {
            return 0;
        }
        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

static int fmc_find_airport_coordinate_override(const char *ident, double *latitude, double *longitude)
{
    if (ident == NULL)
    {
        return 0;
    }

    for (int i = 0; i < (int)(sizeof(FMC_AIRPORT_COORDINATE_OVERRIDES) / sizeof(FMC_AIRPORT_COORDINATE_OVERRIDES[0])); ++i)
    {
        const FMC_AirportCoordinateOverride *item = &FMC_AIRPORT_COORDINATE_OVERRIDES[i];
        if (fmc_ident_equals(ident, item->ident) &&
            fmc_valid_position(item->latitude, item->longitude))
        {
            if (latitude != NULL)
            {
                *latitude = item->latitude;
            }
            if (longitude != NULL)
            {
                *longitude = item->longitude;
            }
            return 1;
        }
    }

    return 0;
}

static FMC_ResolvedCoordinate fmc_invalid_coordinate(void)
{
    FMC_ResolvedCoordinate result;
    result.latitude = 0.0;
    result.longitude = 0.0;
    result.has_position = 0;
    result.source = FMC_COORD_SOURCE_INVALID;
    return result;
}

static FMC_ResolvedCoordinate fmc_resolve_airport_coordinate(
    const char *ident,
    int has_existing,
    double existing_latitude,
    double existing_longitude,
    FMC_CoordinateSource existing_source)
{
    double override_latitude = 0.0;
    double override_longitude = 0.0;
    const int has_override = fmc_find_airport_coordinate_override(ident, &override_latitude, &override_longitude);

    if (has_existing && fmc_valid_position(existing_latitude, existing_longitude))
    {
        if (existing_source == FMC_COORD_SOURCE_ROUTE ||
            existing_source == FMC_COORD_SOURCE_OVERRIDE ||
            (existing_source == FMC_COORD_SOURCE_INVALID && !has_override) ||
            (existing_source == FMC_COORD_SOURCE_APT_DAT && !has_override))
        {
            FMC_ResolvedCoordinate result;
            result.latitude = existing_latitude;
            result.longitude = existing_longitude;
            result.has_position = 1;
            result.source = existing_source != FMC_COORD_SOURCE_INVALID
                                ? existing_source
                                : FMC_COORD_SOURCE_ROUTE;
            return result;
        }
    }

    if (has_override)
    {
        FMC_ResolvedCoordinate result;
        result.latitude = override_latitude;
        result.longitude = override_longitude;
        result.has_position = 1;
        result.source = FMC_COORD_SOURCE_OVERRIDE;
        return result;
    }

    Airport *airport = fmc_query_airport_by_icao(ident);
    if (airport != NULL && fmc_valid_position(airport->datum_lat, airport->datum_lon))
    {
        FMC_ResolvedCoordinate result;
        result.latitude = airport->datum_lat;
        result.longitude = airport->datum_lon;
        result.has_position = 1;
        result.source = FMC_COORD_SOURCE_APT_DAT;
        return result;
    }

    return fmc_invalid_coordinate();
}

static void sync_library_route_fields(const FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    set_text(origin, (int)sizeof(origin), data->origin);
    set_text(dest, (int)sizeof(dest), data->destination);
    set_text(co_route, (int)sizeof(co_route), data->company_route);
    set_text(flt_no, (int)sizeof(flt_no), data->flight_no);
}

static int route_ready_to_exec(const FMC_Data *data)
{
    return data != NULL &&
           data->origin[0] != '\0' &&
           data->destination[0] != '\0' &&
           data->flight_no[0] != '\0';
}

static void update_exec_ready(FMC_Data *data)
{
    if (data != NULL)
    {
        data->origin_exec_pending = route_ready_to_exec(data);
    }
}

static void mark_route_modified(FMC_Data *data, const char *reason)
{
    if (data == NULL)
    {
        return;
    }

    if (!data->route_mod_pending)
    {
        printf("FMC Route: MOD entered (%s); draft origin=%s destination=%s points=%d.\n",
               reason != NULL ? reason : "edit",
               data->origin[0] != '\0' ? data->origin : "----",
               data->destination[0] != '\0' ? data->destination : "----",
               data->route_count);
    }

    data->route_mod_pending = 1;
}

static void clear_auto_route(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->route_count = 0;
    data->configured_route_page = 0;
    data->route_loaded_from_file = 0;
    data->route_source = SIM_ROUTE_SOURCE_NONE;
    data->active_waypoint_index = 0;
    data->fms_plan_path[0] = '\0';
    for (int i = 0; i < FMC_MAX_ROUTE_POINTS; ++i)
    {
        data->route_points[i][0] = '\0';
        data->route_latitudes[i] = 0.0;
        data->route_longitudes[i] = 0.0;
        data->route_has_position[i] = 0;
        data->route_coordinate_sources[i] = FMC_COORD_SOURCE_INVALID;
    }
    if (via_to_list != NULL)
    {
        via_to_list_count = 0;
    }
    rte_index = 1;
}

static void set_origin_position_source(FMC_Data *data, double latitude, double longitude, int has_position, FMC_CoordinateSource source)
{
    if (data == NULL)
    {
        return;
    }

    data->origin_latitude = latitude;
    data->origin_longitude = longitude;
    data->origin_has_position = has_position;
    data->origin_coordinate_source = has_position ? source : FMC_COORD_SOURCE_INVALID;
}

static void set_origin_position(FMC_Data *data, double latitude, double longitude, int has_position)
{
    set_origin_position_source(data,
                               latitude,
                               longitude,
                               has_position,
                               has_position ? FMC_COORD_SOURCE_ROUTE : FMC_COORD_SOURCE_INVALID);
}

static int add_route_point_geo_source(FMC_Data *data, const char *ident, double latitude, double longitude, int has_position, FMC_CoordinateSource source)
{
    if (data == NULL || ident == NULL || ident[0] == '\0' || data->route_count >= FMC_MAX_ROUTE_POINTS)
    {
        return 0;
    }

    const int index = data->route_count;
    set_text(data->route_points[index], sizeof(data->route_points[index]), ident);
    data->route_latitudes[index] = latitude;
    data->route_longitudes[index] = longitude;
    data->route_has_position[index] = has_position;
    data->route_coordinate_sources[index] = has_position ? source : FMC_COORD_SOURCE_INVALID;
    data->route_count++;
    return 1;
}

static int add_route_point_geo(FMC_Data *data, const char *ident, double latitude, double longitude, int has_position)
{
    return add_route_point_geo_source(data,
                                      ident,
                                      latitude,
                                      longitude,
                                      has_position,
                                      has_position ? FMC_COORD_SOURCE_ROUTE : FMC_COORD_SOURCE_INVALID);
}

static int route_has_terminal_destination(const FMC_Data *data)
{
    return data != NULL &&
           data->route_count > 0 &&
           data->destination[0] != '\0' &&
           fmc_ident_equals(data->route_points[data->route_count - 1], data->destination);
}

static void sync_viato_list_from_route(const FMC_Data *data)
{
    if (via_to_list == NULL)
    {
        initVIATO();
    }
    if (via_to_list == NULL)
    {
        return;
    }

    via_to_list_count = 0;
    if (data == NULL)
    {
        return;
    }

    for (int i = 0; i < data->route_count && via_to_list_count < MAX_VIATO_NUM; ++i)
    {
        set_text(via_to_list[via_to_list_count].VIA,
                 sizeof(via_to_list[via_to_list_count].VIA),
                 "DIRECT");
        set_text(via_to_list[via_to_list_count].TO,
                 sizeof(via_to_list[via_to_list_count].TO),
                 data->route_points[i]);
        ++via_to_list_count;
    }
}

static int insert_route_point_before_destination(
    FMC_Data *data,
    const char *ident,
    double latitude,
    double longitude,
    int has_position,
    FMC_CoordinateSource source)
{
    if (data == NULL || ident == NULL || ident[0] == '\0' || data->route_count >= FMC_MAX_ROUTE_POINTS)
    {
        return 0;
    }

    const int insert_index = route_has_terminal_destination(data) ? data->route_count - 1 : data->route_count;
    for (int i = data->route_count; i > insert_index; --i)
    {
        memcpy(data->route_points[i], data->route_points[i - 1], sizeof(data->route_points[i]));
        data->route_latitudes[i] = data->route_latitudes[i - 1];
        data->route_longitudes[i] = data->route_longitudes[i - 1];
        data->route_has_position[i] = data->route_has_position[i - 1];
        data->route_coordinate_sources[i] = data->route_coordinate_sources[i - 1];
    }

    set_text(data->route_points[insert_index], sizeof(data->route_points[insert_index]), ident);
    data->route_latitudes[insert_index] = latitude;
    data->route_longitudes[insert_index] = longitude;
    data->route_has_position[insert_index] = has_position;
    data->route_coordinate_sources[insert_index] = has_position ? source : FMC_COORD_SOURCE_INVALID;
    ++data->route_count;
    return 1;
}

static int add_viato_route_point(FMC_Data *data, const char *ident)
{
    Waypoint *wpt = NULL;
    Airport *arp = NULL;
    const char *to = NULL;

    if (data == NULL || ident == NULL || ident[0] == '\0')
    {
        return 0;
    }

    if (data->route_count >= FMC_MAX_ROUTE_POINTS)
    {
        set_text(data->message, sizeof(data->message), "ROUTE FULL");
        return 0;
    }

    wpt = fmc_query_waypoint_by_code(ident);
    arp = fmc_query_airport_by_icao(ident);
    if (wpt == NULL && arp == NULL)
    {
        set_text(data->message, sizeof(data->message), "NOT IN DATABASE");
        return 0;
    }

    to = wpt != NULL ? wpt->wp_code : arp->icao_code;
    if (fmc_ident_equals(to, data->destination) && route_has_terminal_destination(data))
    {
        fmc_data_clear_scratchpad(data);
        set_text(data->message, sizeof(data->message), "DEST ALREADY LAST");
        return 0;
    }

    if (wpt != NULL)
    {
        insert_route_point_before_destination(data, to, wpt->lat, wpt->lon, 1, FMC_COORD_SOURCE_ROUTE);
    }
    else
    {
        FMC_ResolvedCoordinate coord = fmc_resolve_airport_coordinate(to, 0, 0.0, 0.0, FMC_COORD_SOURCE_INVALID);
        insert_route_point_before_destination(data, to, coord.latitude, coord.longitude, coord.has_position, coord.source);
    }
    sync_viato_list_from_route(data);
    update_exec_ready(data);
    fmc_data_clear_scratchpad(data);
    data->route_clear_pending = 0;
    mark_route_modified(data, "add waypoint");
    return 1;
}

static int load_fms_route_file(FMC_Data *data, const char *path)
{
    FILE *fp = NULL;
    char line[256];
    int seen_origin = 0;

    if (data == NULL || path == NULL || data->origin[0] == '\0' || data->destination[0] == '\0')
    {
        return 0;
    }

    fp = fopen(path, "r");
    if (fp == NULL)
    {
        return 0;
    }

    clear_auto_route(data);
    if (via_to_list == NULL)
    {
        initVIATO();
    }
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        int type = 0;
        char ident[FMC_TEXT_LEN] = {0};
        double altitude = 0.0;
        double latitude = 0.0;
        double longitude = 0.0;

        if (sscanf(line, "%d %31s %lf %lf %lf", &type, ident, &altitude, &latitude, &longitude) != 5)
        {
            continue;
        }

        if (!seen_origin)
        {
            if (strcmp(ident, data->origin) == 0)
            {
                seen_origin = 1;
                set_origin_position_source(data,
                                           latitude,
                                           longitude,
                                           fmc_valid_position(latitude, longitude),
                                           FMC_COORD_SOURCE_ROUTE);
            }
            continue;
        }

        add_route_point_geo_source(data, ident, latitude, longitude, 1, FMC_COORD_SOURCE_ROUTE);
        if (via_to_list != NULL && via_to_list_count < MAX_VIATO_NUM)
        {
            set_text(via_to_list[via_to_list_count].VIA, sizeof(via_to_list[via_to_list_count].VIA), "DIRECT");
            set_text(via_to_list[via_to_list_count].TO, sizeof(via_to_list[via_to_list_count].TO), ident);
            via_to_list_count++;
        }
        if (strcmp(ident, data->destination) == 0)
        {
            break;
        }
    }

    fclose(fp);

    if (data->route_count <= 0 ||
        strcmp(data->route_points[data->route_count - 1], data->destination) != 0)
    {
        clear_auto_route(data);
        return 0;
    }

    data->route_loaded_from_file = 1;
    data->route_source = SIM_ROUTE_SOURCE_FMC_FMS_FILE;
    data->active_waypoint_index = data->route_count > 0 ? 0 : -1;
    set_text(data->fms_plan_path, sizeof(data->fms_plan_path), path);
    return 1;
}

static int load_named_auto_route(FMC_Data *data)
{
    char path[256];

    if (data == NULL || data->origin[0] == '\0' || data->destination[0] == '\0')
    {
        clear_auto_route(data);
        return 0;
    }

    snprintf(path, sizeof(path), "assets/%s%s.fms", data->origin, data->destination);
    if (load_fms_route_file(data, path))
    {
        return 1;
    }

    snprintf(path, sizeof(path), "assets/%s-%s.fms", data->origin, data->destination);
    if (load_fms_route_file(data, path))
    {
        return 1;
    }

    snprintf(path, sizeof(path), "assets/%s_%s.fms", data->origin, data->destination);
    return load_fms_route_file(data, path);
}

static void build_direct_route(FMC_Data *data)
{
    clear_auto_route(data);
    if (data != NULL && data->destination[0] != '\0')
    {
        FMC_ResolvedCoordinate coord = fmc_resolve_airport_coordinate(data->destination, 0, 0.0, 0.0, FMC_COORD_SOURCE_INVALID);
        add_route_point_geo_source(data, data->destination, coord.latitude, coord.longitude, coord.has_position, coord.source);
        if (via_to_list == NULL)
        {
            initVIATO();
        }
        if (via_to_list != NULL && via_to_list_count < MAX_VIATO_NUM)
        {
            set_text(via_to_list[via_to_list_count].VIA, sizeof(via_to_list[via_to_list_count].VIA), "DIRECT");
            set_text(via_to_list[via_to_list_count].TO, sizeof(via_to_list[via_to_list_count].TO), data->destination);
            via_to_list_count++;
        }
        data->route_source = SIM_ROUTE_SOURCE_FMC_FALLBACK;
        data->active_waypoint_index = 0;
    }
}

static void update_auto_route(FMC_Data *data)
{
    if (data == NULL || data->origin[0] == '\0' || data->destination[0] == '\0')
    {
        clear_auto_route(data);
        return;
    }

    if (load_named_auto_route(data))
    {
        snprintf(data->message, sizeof(data->message), "RTE %s-%s LOADED", data->origin, data->destination);
        return;
    }

    build_direct_route(data);
    snprintf(data->message, sizeof(data->message), "RTE %s-%s DIRECT", data->origin, data->destination);
}

static int scratchpad_is_airport_code(const FMC_Data *data, const char *label)
{
    if (data == NULL || label == NULL)
    {
        return 0;
    }

    if (data->scratchpad_len != 4)
    {
        snprintf(((FMC_Data *)data)->message, sizeof(((FMC_Data *)data)->message), "%s MUST BE 4 LETTERS", label);
        return 0;
    }

    for (int i = 0; i < data->scratchpad_len; ++i)
    {
        if (!isalpha((unsigned char)data->scratchpad[i]))
        {
            snprintf(((FMC_Data *)data)->message, sizeof(((FMC_Data *)data)->message), "%s FORMAT INVALID", label);
            return 0;
        }
    }

    return 1;
}

static int set_airport_field(FMC_Data *data, char *dest_field, int dest_size, const char *label)
{
    Airport *airport = NULL;
    char airport_code[FMC_TEXT_LEN] = {0};

    if (data == NULL || dest_field == NULL || label == NULL)
    {
        return 0;
    }

    if (!scratchpad_is_airport_code(data, label))
    {
        return 0;
    }

    set_text(airport_code, sizeof(airport_code), data->scratchpad);
    airport = fmc_query_airport_by_icao(data->scratchpad);
    FMC_ResolvedCoordinate coord = fmc_resolve_airport_coordinate(data->scratchpad, 0, 0.0, 0.0, FMC_COORD_SOURCE_INVALID);
    if (airport == NULL && !coord.has_position)
    {
        fmc_data_clear_scratchpad(data);
        set_text(data->message, sizeof(data->message), "NOT IN DATABASE");
        return 0;
    }

    set_text(dest_field, dest_size, airport != NULL ? airport->icao_code : airport_code);
    if (dest_field == data->origin)
    {
        set_origin_position_source(data,
                                   coord.latitude,
                                   coord.longitude,
                                   coord.has_position,
                                   coord.source);
    }
    fmc_data_clear_scratchpad(data);
    sync_library_route_fields(data);
    update_exec_ready(data);
    update_auto_route(data);
    data->route_clear_pending = 0;
    mark_route_modified(data, label);
    if (data->origin[0] != '\0' && data->destination[0] != '\0' && data->route_count > 0)
    {
        return 1;
    }
    snprintf(data->message, sizeof(data->message), "%s SET", label);
    return 1;
}

static int set_scratchpad_text(FMC_Data *data, char *dest_field, int dest_size, const char *label)
{
    if (data == NULL || dest_field == NULL || label == NULL)
    {
        return 0;
    }

    if (data->scratchpad_len <= 0)
    {
        snprintf(data->message, sizeof(data->message), "ENTER %s", label);
        return 0;
    }

    set_text(dest_field, dest_size, data->scratchpad);
    fmc_data_clear_scratchpad(data);
    sync_library_route_fields(data);
    update_exec_ready(data);
    if (strcmp(label, "CO ROUTE") == 0 || strcmp(label, "VIA") == 0)
    {
        mark_route_modified(data, label);
    }
    snprintf(data->message, sizeof(data->message), "%s SET", label);
    return 1;
}

static void clear_route_draft(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->origin[0] = '\0';
    data->destination[0] = '\0';
    set_origin_position(data, 0.0, 0.0, 0);
    data->company_route[0] = '\0';
    data->route_via[0] = '\0';
    clear_auto_route(data);
    sync_library_route_fields(data);
    data->route_delete_pending = 0;
    data->route_clear_pending = 1;
    mark_route_modified(data, "clear draft");
    set_text(data->message, sizeof(data->message), "RTE DELETE MOD");
}

static int delete_last_route_point(FMC_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    data->route_delete_pending = 0;
    if (data->route_count <= 0)
    {
        set_text(data->message, sizeof(data->message), "NO ROUTE POINT");
        return 0;
    }

    int delete_index = data->route_count - 1;
    if (route_has_terminal_destination(data))
    {
        if (data->route_count <= 1)
        {
            set_text(data->message, sizeof(data->message), "NO INTERMEDIATE WPT");
            return 0;
        }
        delete_index = data->route_count - 2;
    }

    for (int i = delete_index; i < data->route_count - 1; ++i)
    {
        memcpy(data->route_points[i], data->route_points[i + 1], sizeof(data->route_points[i]));
        data->route_latitudes[i] = data->route_latitudes[i + 1];
        data->route_longitudes[i] = data->route_longitudes[i + 1];
        data->route_has_position[i] = data->route_has_position[i + 1];
        data->route_coordinate_sources[i] = data->route_coordinate_sources[i + 1];
    }
    --data->route_count;
    data->route_points[data->route_count][0] = '\0';
    data->route_latitudes[data->route_count] = 0.0;
    data->route_longitudes[data->route_count] = 0.0;
    data->route_has_position[data->route_count] = 0;
    data->route_coordinate_sources[data->route_count] = FMC_COORD_SOURCE_INVALID;
    sync_viato_list_from_route(data);
    mark_route_modified(data, "delete waypoint");
    set_text(data->message, sizeof(data->message), "WPT DELETE MOD");
    return 1;
}

void fmc_data_init(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    memset(data, 0, sizeof(*data));
    data->current_page = FMC_PAGE_INDEX;
    data->previous_page = FMC_PAGE_INDEX;
    data->active_phase = FMC_PHASE_PREFLIGHT;
    data->cruise_altitude = crz_alt > 0 ? crz_alt : 35000;
    data->target_speed = tgt_speed1.speed1;
    data->cost_index = 45.0f;
    data->cruise_speed = tgt_speed2.speed1;
    data->descent_speed = tgt_speed3.speed1;
    data->descent_transition_level = trans_fl > 0 ? trans_fl * 100 : 18000;
    data->descent_vertical_speed = 1800;
    set_text(data->climb_target_speed_text, sizeof(data->climb_target_speed_text), "250/.74");
    set_text(data->climb_spd_alt_limit_text, sizeof(data->climb_spd_alt_limit_text), "250/10000");
    set_text(data->climb_transition_alt_text, sizeof(data->climb_transition_alt_text), "18000");
    set_text(data->legs_sequence, sizeof(data->legs_sequence), "AUTO/INHIBIT");
    sync_library_route_fields(data);
    show_ariport[0] = '\0';
    dep_arr_index = 1;
    dep_arr_type = 0;
    initVIATO();
    init_airport_data();
    load_airport_data();
    load_waypoint_data();
    fmc_xplane_connect_init(NULL, 0);
}

void fmc_data_destroy(FMC_Data *data)
{
    (void)data;
    fmc_xplane_connect_shutdown();
    destroy_airport_data();
}

void fmc_data_update_mock(FMC_Data *data, float delta_time)
{
    (void)data;
    (void)delta_time;
}

void fmc_data_set_page(FMC_Data *data, FMC_Page page)
{
    if (data == NULL || page < FMC_PAGE_HOME || page >= FMC_PAGE_COUNT)
    {
        return;
    }

    if (data->current_page != page)
    {
        data->previous_page = data->current_page;
        data->page_switch_count++;
        fmc_data_clear_scratchpad(data);
    }

    data->current_page = page;
    if (page == FMC_PAGE_ROUTE)
    {
        rte_index = 1;
        data->configured_route_page = 0;
    }
    if (page == FMC_PAGE_DEP_ARR)
    {
        show_ariport[0] = '\0';
        dep_arr_index = 1;
    }
    data->message[0] = '\0';
}

void fmc_data_append_char(FMC_Data *data, char c)
{
    if (data == NULL || data->scratchpad_len >= FMC_TEXT_LEN - 1)
    {
        return;
    }

    if (c < 32 || c > 126)
    {
        return;
    }

    data->scratchpad[data->scratchpad_len++] = (char)toupper((unsigned char)c);
    data->scratchpad[data->scratchpad_len] = '\0';
    data->route_delete_pending = 0;
    data->message[0] = '\0';
}

void fmc_data_backspace(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    if (data->scratchpad_len <= 0)
    {
        data->route_delete_pending = 0;
        data->message[0] = '\0';
        return;
    }

    data->scratchpad[--data->scratchpad_len] = '\0';
    data->message[0] = '\0';
}

void fmc_data_show_delete(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->scratchpad[0] = '\0';
    data->scratchpad_len = 0;
    data->route_delete_pending = 1;
    set_text(data->message, sizeof(data->message), "DELETE");
}

void fmc_data_clear_scratchpad(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->scratchpad[0] = '\0';
    data->scratchpad_len = 0;
    data->route_delete_pending = 0;
    data->message[0] = '\0';
}

int fmc_data_route_page_count(const FMC_Data *data)
{
    (void)data;
    return (via_to_list_count + FMC_RTE_PAGE_SIZE - 1) / FMC_RTE_PAGE_SIZE + 1;
}

int fmc_data_route_next_page(FMC_Data *data)
{
    if (data == NULL || data->current_page != FMC_PAGE_ROUTE)
    {
        return 0;
    }
    if (rte_index < (via_to_list_count + FMC_RTE_PAGE_SIZE - 1) / FMC_RTE_PAGE_SIZE + 1)
    {
        rte_index++;
        data->configured_route_page = rte_index - 1;
        data->message[0] = '\0';
        return 1;
    }
    data->configured_route_page = rte_index - 1;
    set_text(data->message, sizeof(data->message), "LAST RTE PAGE");
    return 0;
}

int fmc_data_route_prev_page(FMC_Data *data)
{
    if (data == NULL || data->current_page != FMC_PAGE_ROUTE)
    {
        return 0;
    }
    if (rte_index > 1)
    {
        rte_index--;
        data->configured_route_page = rte_index - 1;
        data->message[0] = '\0';
        return 1;
    }
    data->configured_route_page = rte_index - 1;
    set_text(data->message, sizeof(data->message), "FIRST RTE PAGE");
    return 0;
}

int fmc_data_set_route_field(FMC_Data *data, FMC_RouteField field)
{
    if (data == NULL)
    {
        return 0;
    }

    if (data->route_delete_pending)
    {
        if (field == FMC_ROUTE_FIELD_ORIGIN || field == FMC_ROUTE_FIELD_DESTINATION)
        {
            clear_route_draft(data);
            return 1;
        }
        if (field == FMC_ROUTE_FIELD_TO_FIX)
        {
            return delete_last_route_point(data);
        }

        data->route_delete_pending = 0;
        set_text(data->message, sizeof(data->message), "DELETE INVALID");
        return 0;
    }

    switch (field)
    {
    case FMC_ROUTE_FIELD_ORIGIN:
        if (!set_airport_field(data, data->origin, sizeof(data->origin), "ORIGIN"))
        {
            return 0;
        }
        if (fmc_xplane_set_origin(data->origin) < 0)
        {
            set_text(data->message, sizeof(data->message),
                     fmc_xplane_connect_is_connected() ? "XPLANE SYNC FAIL" : "XPLANE NOT CONNECT");
            return 0;
        }
        return 1;
    case FMC_ROUTE_FIELD_DESTINATION:
        if (!set_airport_field(data, data->destination, sizeof(data->destination), "DEST"))
        {
            return 0;
        }
        if (fmc_xplane_set_destination(data->destination) < 0)
        {
            set_text(data->message, sizeof(data->message),
                     fmc_xplane_connect_is_connected() ? "XPLANE SYNC FAIL" : "XPLANE NOT CONNECT");
            return 0;
        }
        return 1;
    case FMC_ROUTE_FIELD_COMPANY_ROUTE:
        return set_scratchpad_text(data, data->company_route, sizeof(data->company_route), "CO ROUTE");
    case FMC_ROUTE_FIELD_FLIGHT_NO:
        if (!set_scratchpad_text(data, data->flight_no, sizeof(data->flight_no), "FLT NO"))
        {
            return 0;
        }
        if (fmc_xplane_set_flt_no(data->flight_no) < 0)
        {
            set_text(data->message, sizeof(data->message),
                     fmc_xplane_connect_is_connected() ? "XPLANE SYNC FAIL" : "XPLANE NOT CONNECT");
            return 0;
        }
        return 1;
    case FMC_ROUTE_FIELD_VIA:
        if (rte_index != 1)
        {
            return 0;
        }
        return set_scratchpad_text(data, data->route_via, sizeof(data->route_via), "VIA");
    case FMC_ROUTE_FIELD_TO_FIX:
        if (rte_index != 1)
        {
            return 0;
        }
        if (data->scratchpad_len <= 0)
        {
            set_text(data->message, sizeof(data->message), "ENTER TO");
            return 0;
        }
        return add_viato_route_point(data, data->scratchpad);
    default:
        set_text(data->message, sizeof(data->message), "NO ROUTE FIELD");
        return 0;
    }
}

static void format_tgt_speed_text(const TgtSpeed *speed, char *dest, int dest_size)
{
    if (speed == NULL || dest == NULL || dest_size <= 0)
    {
        return;
    }
    snprintf(dest, (size_t)dest_size, "%d/.%02d", speed->speed1, speed->speed2);
}

static int set_phase_invalid(FMC_Data *data)
{
    set_text(data->message, sizeof(data->message), "INVALID ENTRY");
    fmc_data_clear_scratchpad(data);
    set_text(data->message, sizeof(data->message), "INVALID ENTRY");
    return 0;
}

static int set_phase_target_speed(FMC_Data *data, TgtSpeed *target, char *display_text, int display_text_size)
{
    if (setTgtSpeed(data->scratchpad, target) <= 0)
    {
        return set_phase_invalid(data);
    }
    format_tgt_speed_text(target, display_text, display_text_size);
    fmc_data_clear_scratchpad(data);
    data->origin_exec_pending = 1;
    return 1;
}

static int set_phase_spd_alt_limit(FMC_Data *data, SpdAltLimit *target, char *display_text, int display_text_size)
{
    if (setSpdAltLimit(data->scratchpad, target) < 0)
    {
        return set_phase_invalid(data);
    }
    snprintf(display_text, (size_t)display_text_size, "%d/%d", target->spd_limit, target->alt_limit);
    fmc_data_clear_scratchpad(data);
    data->origin_exec_pending = 1;
    return 1;
}

int fmc_data_set_phase_parameter(FMC_Data *data, int field_index)
{
    if (data == NULL)
    {
        return 0;
    }

    if (data->scratchpad_len <= 0)
    {
        set_text(data->message, sizeof(data->message), "ENTER VALUE");
        return 0;
    }

    if (data->current_page == FMC_PAGE_CLIMB)
    {
        if (field_index == 1)
        {
            return set_phase_target_speed(data, &tgt_speed1, data->climb_target_speed_text, sizeof(data->climb_target_speed_text));
        }
        if (field_index == 2)
        {
            return set_phase_spd_alt_limit(data, &spd_alt_limit1, data->climb_spd_alt_limit_text, sizeof(data->climb_spd_alt_limit_text));
        }
        if (field_index == 3)
        {
            if (!is_string_in_range(data->scratchpad, FMC_TRANS_ALT_MIN, FMC_TRANS_ALT_MAX, &trans_alt))
            {
                return set_phase_invalid(data);
            }
            snprintf(data->climb_transition_alt_text, sizeof(data->climb_transition_alt_text), "%d", trans_alt);
            fmc_data_clear_scratchpad(data);
            data->origin_exec_pending = 1;
            return 1;
        }
    }

    if (data->current_page == FMC_PAGE_CRUISE)
    {
        if (field_index == 1)
        {
            return set_phase_target_speed(data, &tgt_speed2, data->climb_target_speed_text, sizeof(data->climb_target_speed_text));
        }
        if (field_index == 4)
        {
            if (!is_string_in_range(data->scratchpad, FMC_CRZ_ALT_MIN, FMC_CRZ_ALT_MAX, &crz_alt))
            {
                return set_phase_invalid(data);
            }
            data->cruise_altitude = crz_alt;
            fmc_data_clear_scratchpad(data);
            data->origin_exec_pending = 1;
            return 1;
        }
    }

    if (data->current_page == FMC_PAGE_DESCENT)
    {
        if (field_index == 1)
        {
            return set_phase_target_speed(data, &tgt_speed3, data->climb_target_speed_text, sizeof(data->climb_target_speed_text));
        }
        if (field_index == 2)
        {
            return set_phase_spd_alt_limit(data, &spd_alt_limit1, data->climb_spd_alt_limit_text, sizeof(data->climb_spd_alt_limit_text));
        }
        if (field_index == 5)
        {
            if (!is_string_in_range(data->scratchpad, FMC_TRANS_FL_MIN, FMC_TRANS_FL_MAX, &trans_fl))
            {
                return set_phase_invalid(data);
            }
            data->descent_transition_level = trans_fl * 100;
            fmc_data_clear_scratchpad(data);
            data->origin_exec_pending = 1;
            return 1;
        }
        if (field_index == 6)
        {
            if (!is_string_in_range_f(data->scratchpad, FMC_VPA_MIN, FMC_VPA_MAX, &vpa))
            {
                return set_phase_invalid(data);
            }
            fmc_data_clear_scratchpad(data);
            data->origin_exec_pending = 1;
            return 1;
        }
    }

    set_text(data->message, sizeof(data->message), "NO PHASE FIELD");
    return 0;
}

int fmc_data_set_dep_arr_parameter(FMC_Data *data, int arrival_side, int field_index)
{
    char *target = NULL;
    const char *label = NULL;

    if (data == NULL)
    {
        return 0;
    }

    if (arrival_side)
    {
        target = field_index == 1 ? data->arrival_runway : field_index == 2 ? data->arrival_procedure : data->arrival_transition;
        label = field_index == 1 ? "ARR RWY" : field_index == 2 ? "ARR PROC" : "ARR TRANS";
    }
    else
    {
        target = field_index == 1 ? data->departure_runway : field_index == 2 ? data->departure_procedure : data->departure_transition;
        label = field_index == 1 ? "DEP RWY" : field_index == 2 ? "DEP PROC" : "DEP TRANS";
    }

    return set_scratchpad_text(data, target, FMC_TEXT_LEN, label);
}

static void reset_dep_arr_selection(SelectDepArr *sda)
{
    if (sda == NULL)
    {
        return;
    }
    sda->select_proc[0] = '\0';
    sda->select_proc_trans[0] = '\0';
    sda->select_runway[0] = '\0';
    sda->select_runway_trans[0] = '\0';
}

static void open_dep_arr_airport(const char *airport, int type)
{
    set_text(show_ariport, sizeof(show_ariport), airport);
    dep_arr_index = 1;
    dep_arr_type = type;
    query_runway_proc_by_airport(show_ariport);
}

static void click_dep_arr_left(FMC_Data *data, int index)
{
    SelectDepArr *sda = &select_dep_arr[dep_arr_type];

    if (strlen(sda->select_proc) > 0 && index != 1)
    {
        if (proc_trans_count == 0)
        {
            return;
        }
        int start_index = 2;
        int end_index = 1 + proc_trans_count;
        if (index >= start_index && index <= end_index)
        {
            set_text(sda->select_proc_trans, sizeof(sda->select_proc_trans), proc_trans[index - 2]);
            dep_arr_index = 1;
        }
    }
    else if (strlen(sda->select_proc) > 0 && index == 1)
    {
        reset_dep_arr_selection(sda);
        query_runway_proc_by_airport(show_ariport);
    }
    else
    {
        int truely_selected = (dep_arr_index - 1) * 5 + index - 1;
        if (truely_selected < proc_count)
        {
            set_text(sda->select_proc, sizeof(sda->select_proc), proc[truely_selected]);
            sda->select_proc_trans[0] = '\0';
            query_trans_by_proc(show_ariport, sda->select_proc);
            query_runway_by_proc(show_ariport, sda->select_proc);
            dep_arr_index = 1;
        }
    }

    data->origin_exec_pending = 1;
    sda->select_flag = 0;
}

static void click_dep_arr_right(FMC_Data *data, int index)
{
    SelectDepArr *sda = &select_dep_arr[dep_arr_type];

    if (strlen(sda->select_runway) > 0 && index != 1)
    {
        if (runway_trans_count == 0)
        {
            return;
        }
        int start_index = 2;
        int end_index = 1 + runway_trans_count;
        if (index >= start_index && index <= end_index)
        {
            set_text(sda->select_runway_trans, sizeof(sda->select_runway_trans), runway_trans[index - 2]);
            dep_arr_index = 1;
        }
    }
    else if (strlen(sda->select_runway) > 0 && index == 1)
    {
        reset_dep_arr_selection(sda);
        query_runway_proc_by_airport(show_ariport);
    }
    else
    {
        int truely_selected = (dep_arr_index - 1) * 5 + index - 1;
        if (truely_selected < runway_count)
        {
            set_text(sda->select_runway, sizeof(sda->select_runway), runway[truely_selected]);
            sda->select_runway_trans[0] = '\0';
            query_trans_by_runway(show_ariport, sda->select_runway);
            query_proc_by_runway(show_ariport, sda->select_runway);
            dep_arr_index = 1;
        }
    }

    data->origin_exec_pending = 1;
    sda->select_flag = 0;
}

int fmc_data_handle_dep_arr_lsk(FMC_Data *data, int right_side, int index)
{
    if (data == NULL || index < 1 || index > 5)
    {
        return 0;
    }

    if (show_ariport[0] == '\0')
    {
        if (!right_side && index == 1 && data->origin[0] != '\0')
        {
            open_dep_arr_airport(data->origin, 0);
            return 1;
        }
        if (right_side && index == 1 && data->origin[0] != '\0')
        {
            open_dep_arr_airport(data->origin, 1);
            return 1;
        }
        if (right_side && index == 2 && data->destination[0] != '\0')
        {
            open_dep_arr_airport(data->destination, 2);
            return 1;
        }
        return 0;
    }

    if (right_side)
    {
        click_dep_arr_right(data, index);
    }
    else
    {
        click_dep_arr_left(data, index);
    }
    return 1;
}

int fmc_data_dep_arr_next_page(FMC_Data *data)
{
    (void)data;
    if (show_ariport[0] == '\0')
    {
        return 0;
    }
    int total_count = (runway_count > proc_count ? (runway_count + 1) / 5 : (proc_count + 1) / 5) + 1;
    if (dep_arr_index < total_count)
    {
        dep_arr_index++;
        return 1;
    }
    return 0;
}

int fmc_data_dep_arr_prev_page(FMC_Data *data)
{
    (void)data;
    if (show_ariport[0] == '\0')
    {
        return 0;
    }
    if (dep_arr_index > 1)
    {
        dep_arr_index--;
        return 1;
    }
    return 0;
}

void fmc_data_dep_arr_back_to_index(FMC_Data *data)
{
    (void)data;
    show_ariport[0] = '\0';
    dep_arr_index = 1;
}

int fmc_data_set_legs_parameter(FMC_Data *data, int field_index)
{
    if (data == NULL)
    {
        return 0;
    }
    if (field_index != 1)
    {
        set_text(data->message, sizeof(data->message), "NO LEGS FIELD");
        return 0;
    }
    if (data->scratchpad_len <= 0)
    {
        set_text(data->message, sizeof(data->message), "ENTER FIX/AFTER");
        return 0;
    }

    const char *separator = strchr(data->scratchpad, '/');
    if (separator == NULL)
    {
        return set_scratchpad_text(data, data->legs_sequence, sizeof(data->legs_sequence), "LEGS");
    }

    const size_t move_length = (size_t)(separator - data->scratchpad);
    const size_t after_length = strlen(separator + 1);
    if (move_length == 0 || move_length >= FMC_TEXT_LEN || after_length == 0 || after_length >= FMC_TEXT_LEN)
    {
        set_text(data->message, sizeof(data->message), "FORMAT FIX/AFTER");
        return 0;
    }

    char move_ident[FMC_TEXT_LEN] = {0};
    char after_ident[FMC_TEXT_LEN] = {0};
    memcpy(move_ident, data->scratchpad, move_length);
    memcpy(after_ident, separator + 1, after_length);

    const int intermediate_end = route_has_terminal_destination(data) ? data->route_count - 1 : data->route_count;
    int move_index = -1;
    int after_index = -2; /* -1 means insert after origin. */
    for (int i = 0; i < intermediate_end; ++i)
    {
        if (fmc_ident_equals(data->route_points[i], move_ident))
        {
            move_index = i;
        }
        if (fmc_ident_equals(data->route_points[i], after_ident))
        {
            after_index = i;
        }
    }
    if (fmc_ident_equals(after_ident, "ORIGIN"))
    {
        after_index = -1;
    }
    if (move_index < 0 || after_index == -2 || move_index == after_index)
    {
        set_text(data->message, sizeof(data->message), "LEGS FIX NOT FOUND");
        return 0;
    }

    char moved_point[FMC_TEXT_LEN];
    const double moved_latitude = data->route_latitudes[move_index];
    const double moved_longitude = data->route_longitudes[move_index];
    const int moved_has_position = data->route_has_position[move_index];
    const FMC_CoordinateSource moved_source = data->route_coordinate_sources[move_index];
    memcpy(moved_point, data->route_points[move_index], sizeof(moved_point));

    for (int i = move_index; i < data->route_count - 1; ++i)
    {
        memcpy(data->route_points[i], data->route_points[i + 1], sizeof(data->route_points[i]));
        data->route_latitudes[i] = data->route_latitudes[i + 1];
        data->route_longitudes[i] = data->route_longitudes[i + 1];
        data->route_has_position[i] = data->route_has_position[i + 1];
        data->route_coordinate_sources[i] = data->route_coordinate_sources[i + 1];
    }
    --data->route_count;
    if (after_index > move_index)
    {
        --after_index;
    }

    const int insert_index = after_index + 1;
    for (int i = data->route_count; i > insert_index; --i)
    {
        memcpy(data->route_points[i], data->route_points[i - 1], sizeof(data->route_points[i]));
        data->route_latitudes[i] = data->route_latitudes[i - 1];
        data->route_longitudes[i] = data->route_longitudes[i - 1];
        data->route_has_position[i] = data->route_has_position[i - 1];
        data->route_coordinate_sources[i] = data->route_coordinate_sources[i - 1];
    }
    memcpy(data->route_points[insert_index], moved_point, sizeof(data->route_points[insert_index]));
    data->route_latitudes[insert_index] = moved_latitude;
    data->route_longitudes[insert_index] = moved_longitude;
    data->route_has_position[insert_index] = moved_has_position;
    data->route_coordinate_sources[insert_index] = moved_source;
    ++data->route_count;

    set_text(data->legs_sequence, sizeof(data->legs_sequence), data->scratchpad);
    sync_viato_list_from_route(data);
    fmc_data_clear_scratchpad(data);
    mark_route_modified(data, "reorder waypoint");
    set_text(data->message, sizeof(data->message), "LEGS ORDER MOD");
    return 1;
}

int fmc_data_set_hold_parameter(FMC_Data *data, int field_index)
{
    if (data == NULL)
    {
        return 0;
    }

    switch (field_index)
    {
    case 1:
        return set_scratchpad_text(data, data->hold_fix, sizeof(data->hold_fix), "FIX");
    case 2:
        return set_scratchpad_text(data, data->hold_inbound_course, sizeof(data->hold_inbound_course), "COURSE");
    case 3:
        return set_scratchpad_text(data, data->hold_turn_direction, sizeof(data->hold_turn_direction), "TURN");
    case 4:
        return set_scratchpad_text(data, data->hold_leg_time, sizeof(data->hold_leg_time), "LEG TIME");
    case 5:
        return set_scratchpad_text(data, data->hold_speed_altitude, sizeof(data->hold_speed_altitude), "SPD/ALT");
    default:
        set_text(data->message, sizeof(data->message), "NO HOLD FIELD");
        return 0;
    }
}

int fmc_data_activate_current_phase(FMC_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    if (data->current_page == FMC_PAGE_CLIMB)
    {
        data->active_phase = FMC_PHASE_CLIMB;
    }
    else if (data->current_page == FMC_PAGE_CRUISE)
    {
        data->active_phase = FMC_PHASE_CRUISE;
    }
    else if (data->current_page == FMC_PAGE_DESCENT)
    {
        data->active_phase = FMC_PHASE_DESCENT;
    }
    else
    {
        return 0;
    }

    set_text(data->message, sizeof(data->message), "PHASE ACTIVE");
    return 1;
}

static int fmc_valid_position(double latitude, double longitude)
{
    return latitude >= -90.0 && latitude <= 90.0 &&
           longitude >= -180.0 && longitude <= 180.0 &&
           !(latitude == 0.0 && longitude == 0.0);
}

static FMC_CoordinateSource fmc_coordinate_source_from_name(const char *source)
{
    if (source == NULL || source[0] == '\0')
    {
        return FMC_COORD_SOURCE_INVALID;
    }
    if (strcmp(source, "ROUTE") == 0)
    {
        return FMC_COORD_SOURCE_ROUTE;
    }
    if (strcmp(source, "OVERRIDE") == 0)
    {
        return FMC_COORD_SOURCE_OVERRIDE;
    }
    if (strcmp(source, "APT_DAT") == 0)
    {
        return FMC_COORD_SOURCE_APT_DAT;
    }
    return FMC_COORD_SOURCE_INVALID;
}

static void set_sim_route_point(
    SimRoutePoint *point,
    const char *ident,
    const char *type,
    double latitude,
    double longitude,
    int has_position,
    FMC_CoordinateSource source)
{
    if (point == NULL)
    {
        return;
    }

    memset(point, 0, sizeof(*point));
    set_text(point->ident, sizeof(point->ident), ident);
    set_text(point->type, sizeof(point->type), type);
    set_text(point->coordinate_source, sizeof(point->coordinate_source), fmc_coordinate_source_name(source));
    point->latitude = latitude;
    point->longitude = longitude;
    point->altitude = 0.0;
    point->has_position = has_position;
}

int fmc_data_apply_planned_route(FMC_Data *data, const SimPlannedRoute *route)
{
    if (data == NULL || route == NULL || !route->valid || route->point_count <= 0)
    {
        return 0;
    }

    clear_auto_route(data);
    set_text(data->origin, sizeof(data->origin), route->origin);
    set_text(data->destination, sizeof(data->destination), route->destination);
    if (route->point_count > 0)
    {
        const SimRoutePoint *origin_point = &route->points[0];
        FMC_CoordinateSource source = fmc_coordinate_source_from_name(origin_point->coordinate_source);
        if (source == FMC_COORD_SOURCE_INVALID && origin_point->has_position)
        {
            source = FMC_COORD_SOURCE_ROUTE;
        }
        set_origin_position_source(data,
                                   origin_point->latitude,
                                   origin_point->longitude,
                                   origin_point->has_position,
                                   source);
    }
    set_text(data->fms_plan_path, sizeof(data->fms_plan_path), route->source_path);
    data->route_loaded_from_file = route->source == SIM_ROUTE_SOURCE_FMC_FMS_FILE;
    data->route_source = route->source;
    data->active_waypoint_index = route->active_waypoint_index > 0 ? route->active_waypoint_index - 1 : 0;
    if (data->active_waypoint_index < 0)
    {
        data->active_waypoint_index = 0;
    }

    if (via_to_list == NULL)
    {
        initVIATO();
    }

    for (int i = 1; i < route->point_count && data->route_count < FMC_MAX_ROUTE_POINTS; ++i)
    {
        const SimRoutePoint *point = &route->points[i];
        FMC_CoordinateSource source = fmc_coordinate_source_from_name(point->coordinate_source);
        if (source == FMC_COORD_SOURCE_INVALID && point->has_position)
        {
            source = FMC_COORD_SOURCE_ROUTE;
        }
        add_route_point_geo_source(data, point->ident, point->latitude, point->longitude, point->has_position, source);
        if (via_to_list != NULL && via_to_list_count < MAX_VIATO_NUM)
        {
            set_text(via_to_list[via_to_list_count].VIA, sizeof(via_to_list[via_to_list_count].VIA), "DIRECT");
            set_text(via_to_list[via_to_list_count].TO, sizeof(via_to_list[via_to_list_count].TO), point->ident);
            via_to_list_count++;
        }
    }

    if (data->active_waypoint_index >= data->route_count)
    {
        data->active_waypoint_index = data->route_count > 0 ? 0 : -1;
    }

    sync_library_route_fields(data);
    data->route_mod_pending = 0;
    data->route_clear_pending = 0;
    data->route_delete_pending = 0;
    snprintf(data->message, sizeof(data->message), "UNIFIED RTE %s-%s", data->origin, data->destination);
    return data->route_count > 0;
}

int fmc_data_export_planned_route(const FMC_Data *data, SimPlannedRoute *route)
{
    if (data == NULL || route == NULL)
    {
        return 0;
    }

    memset(route, 0, sizeof(*route));
    set_text(route->origin, sizeof(route->origin), data->origin);
    set_text(route->destination, sizeof(route->destination), data->destination);
    set_text(route->source_path, sizeof(route->source_path), data->fms_plan_path);
    route->source = data->route_source != SIM_ROUTE_SOURCE_NONE
                        ? data->route_source
                        : (data->route_loaded_from_file ? SIM_ROUTE_SOURCE_FMC_FMS_FILE : SIM_ROUTE_SOURCE_FMC_FALLBACK);
    route->loaded_from_file = data->route_loaded_from_file;
    route->active_waypoint_index = -1;

    int coordinate_count = 0;
    if (route->point_count < SIM_ROUTE_MAX_POINTS && data->origin[0] != '\0')
    {
        const FMC_ResolvedCoordinate coord = fmc_resolve_airport_coordinate(data->origin,
                                                                            data->origin_has_position,
                                                                            data->origin_latitude,
                                                                            data->origin_longitude,
                                                                            data->origin_coordinate_source);
        set_sim_route_point(&route->points[route->point_count++],
                            data->origin,
                            "AIRPORT",
                            coord.has_position ? coord.latitude : 0.0,
                            coord.has_position ? coord.longitude : 0.0,
                            coord.has_position,
                            coord.source);
        if (coord.has_position)
        {
            coordinate_count++;
        }
    }

    for (int i = 0; i < data->route_count && route->point_count < SIM_ROUTE_MAX_POINTS; ++i)
    {
        const int is_destination_airport = strcmp(data->route_points[i], data->destination) == 0;
        FMC_ResolvedCoordinate coord;
        if (is_destination_airport)
        {
            coord = fmc_resolve_airport_coordinate(data->route_points[i],
                                                   data->route_has_position[i],
                                                   data->route_latitudes[i],
                                                   data->route_longitudes[i],
                                                   data->route_coordinate_sources[i]);
        }
        else
        {
            coord.latitude = data->route_latitudes[i];
            coord.longitude = data->route_longitudes[i];
            coord.has_position = data->route_has_position[i] &&
                                 fmc_valid_position(data->route_latitudes[i], data->route_longitudes[i]);
            coord.source = coord.has_position ? data->route_coordinate_sources[i] : FMC_COORD_SOURCE_INVALID;
            if (coord.source == FMC_COORD_SOURCE_INVALID && coord.has_position)
            {
                coord.source = FMC_COORD_SOURCE_ROUTE;
            }
        }
        set_sim_route_point(&route->points[route->point_count++],
                            data->route_points[i],
                            is_destination_airport ? "AIRPORT" : "FIX",
                            coord.has_position ? coord.latitude : 0.0,
                            coord.has_position ? coord.longitude : 0.0,
                            coord.has_position,
                            coord.source);
        if (coord.has_position)
        {
            coordinate_count++;
        }
    }

    /* Origin is not a navigation target. Select the first later point that can be drawn. */
    for (int i = 1; i < route->point_count; ++i)
    {
        if (route->points[i].has_position)
        {
            route->active_waypoint_index = i;
            break;
        }
    }

    route->valid = route->origin[0] != '\0' &&
                   route->destination[0] != '\0' &&
                   route->point_count >= 2;
    route->has_coordinates = route->point_count > 0 && coordinate_count == route->point_count;
    return route->valid;
}

static int path_exists(const char *path)
{
    return path != NULL && path[0] != '\0' && FMC_ACCESS(path) == 0;
}

static int ensure_directory_exists(const char *path)
{
    char temp[SIM_ROUTE_PATH_LEN * 2];
    size_t len = 0;

    if (path == NULL || path[0] == '\0')
    {
        return 0;
    }

    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);
    while (len > 0 && (temp[len - 1] == '\\' || temp[len - 1] == '/'))
    {
        temp[--len] = '\0';
    }

    for (char *p = temp + 1; *p != '\0'; ++p)
    {
        if (*p == '\\' || *p == '/')
        {
            char saved = *p;
            *p = '\0';
            if (!path_exists(temp) && FMC_MKDIR(temp) != 0 && errno != EEXIST)
            {
                return 0;
            }
            *p = saved;
        }
    }

    if (!path_exists(temp) && FMC_MKDIR(temp) != 0 && errno != EEXIST)
    {
        return 0;
    }

    return 1;
}

static int resolve_xplane_fms_plans_dir(char *dest, int dest_size)
{
    const char *direct = getenv("XPLANE_FMS_PLANS_DIR");
    const char *root = getenv("XPLANE_ROOT");
    const char *candidates[] = {
        "D:\\X-Plane 11\\Output\\FMS plans",
        "C:\\X-Plane 11\\Output\\FMS plans",
        "D:\\X-Plane 12\\Output\\FMS plans",
        "C:\\X-Plane 12\\Output\\FMS plans"};

    if (dest == NULL || dest_size <= 0)
    {
        return 0;
    }

    if (direct != NULL && direct[0] != '\0')
    {
        snprintf(dest, (size_t)dest_size, "%s", direct);
        return ensure_directory_exists(dest);
    }

    if (root != NULL && root[0] != '\0')
    {
        snprintf(dest, (size_t)dest_size, "%s\\Output\\FMS plans", root);
        return ensure_directory_exists(dest);
    }

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
    {
        if (path_exists(candidates[i]))
        {
            snprintf(dest, (size_t)dest_size, "%s", candidates[i]);
            return 1;
        }
    }

    return 0;
}

static int xplane_fms_point_type(const SimRoutePoint *point, int index, int count)
{
    if (point == NULL)
    {
        return 11;
    }
    if (index == 0 || index == count - 1 || strcmp(point->type, "AIRPORT") == 0)
    {
        return 1;
    }
    if (strcmp(point->type, "VOR") == 0)
    {
        return 3;
    }
    if (strcmp(point->type, "NDB") == 0)
    {
        return 2;
    }
    return 11;
}

static int fmc_export_route_to_xplane_fms(const FMC_Data *data, char *route_name, int route_name_size)
{
    SimPlannedRoute route;
    char dir[SIM_ROUTE_PATH_LEN * 2];
    char path[SIM_ROUTE_PATH_LEN * 3];
    FILE *fp = NULL;

    if (route_name != NULL && route_name_size > 0)
    {
        route_name[0] = '\0';
    }

    if (!fmc_data_export_planned_route(data, &route) || !route.has_coordinates)
    {
        return 0;
    }

    if (!resolve_xplane_fms_plans_dir(dir, sizeof(dir)))
    {
        return 0;
    }

    snprintf(route_name, (size_t)route_name_size, "%s%s", route.origin, route.destination);
    snprintf(path, sizeof(path), "%s\\%s.fms", dir, route_name);

    fp = fopen(path, "w");
    if (fp == NULL)
    {
        return 0;
    }

    fprintf(fp, "I\n");
    fprintf(fp, "3 version\n");
    fprintf(fp, "0\n");
    fprintf(fp, "%d\n", route.point_count);
    for (int i = 0; i < route.point_count; ++i)
    {
        const SimRoutePoint *point = &route.points[i];
        fprintf(fp,
                "%d %s %.0f %.6f %.6f\n",
                xplane_fms_point_type(point, i, route.point_count),
                point->ident,
                point->altitude,
                point->latitude,
                point->longitude);
    }

    fclose(fp);
    printf("FMC Route: exported X-Plane FMS plan %s.\n", path);
    fflush(stdout);
    return 1;
}

static int sync_route_fields_to_xplane_fmc(const FMC_Data *data, const char *fms_route_name)
{
    const char *co_route_text = NULL;
    int sent_any = 0;

    if (data == NULL)
    {
        return -1;
    }

    if (data->origin[0] != '\0')
    {
        if (fmc_xplane_set_origin(data->origin) < 0)
        {
            return -1;
        }
        sent_any = 1;
    }

    if (data->destination[0] != '\0')
    {
        if (fmc_xplane_set_destination(data->destination) < 0)
        {
            return -1;
        }
        sent_any = 1;
    }

    co_route_text = (fms_route_name != NULL && fms_route_name[0] != '\0')
                        ? fms_route_name
                        : data->company_route;
    if (co_route_text != NULL && co_route_text[0] != '\0')
    {
        if (fmc_xplane_set_co_route(co_route_text) < 0)
        {
            return -1;
        }
        sent_any = 1;
    }

    if (data->flight_no[0] != '\0')
    {
        if (fmc_xplane_set_flt_no(data->flight_no) < 0)
        {
            return -1;
        }
        sent_any = 1;
    }

    if (sent_any && fmc_xplane_set_exec() < 0)
    {
        return -1;
    }

    if (sent_any)
    {
        printf("FMC Route: synced route fields to X-Plane FMC origin=%s destination=%s co_route=%s flight_no=%s.\n",
               data->origin[0] != '\0' ? data->origin : "----",
               data->destination[0] != '\0' ? data->destination : "----",
               co_route_text != NULL && co_route_text[0] != '\0' ? co_route_text : "----",
               data->flight_no[0] != '\0' ? data->flight_no : "----");
        fflush(stdout);
    }

    return sent_any ? 1 : 0;
}

int fmc_data_sync_route_to_xplane(FMC_Data *data)
{
    char fms_route_name[2 * FMC_TEXT_LEN] = {0};
    int fms_exported;
    int xplane_synced;

    if (data == NULL)
    {
        return 0;
    }

    if (!fmc_xplane_connect_is_ready() || !fmc_xplane_connect_is_connected())
    {
        set_text(data->message, sizeof(data->message), "XPLANE NOT CONNECT");
        printf("FMC Route: X-Plane sync skipped; FMC plugin connection is not ready.\n");
        fflush(stdout);
        return 0;
    }

    if (!fmc_xplane_confirm_sync_ready())
    {
        set_text(data->message, sizeof(data->message), "XPLANE CONNECTION LOST");
        printf("FMC Route: X-Plane sync aborted; quick connection confirmation failed.\n");
        fflush(stdout);
        return 0;
    }

    fms_exported = fmc_export_route_to_xplane_fms(data, fms_route_name, sizeof(fms_route_name));
    xplane_synced = sync_route_fields_to_xplane_fmc(data, fms_exported ? fms_route_name : NULL);

    if (xplane_synced > 0)
    {
        if (fms_exported && fms_route_name[0] != '\0')
        {
            snprintf(data->message, sizeof(data->message), "CO RTE %s SENT", fms_route_name);
        }
        else
        {
            snprintf(data->message, sizeof(data->message), "RTE %s-%s SENT", data->origin, data->destination);
        }
        return 1;
    }

    if (fms_exported && fms_route_name[0] != '\0')
    {
        snprintf(data->message, sizeof(data->message), "CO RTE %s READY", fms_route_name);
        return 1;
    }

    set_text(data->message, sizeof(data->message),
             fmc_xplane_connect_is_connected() ? "XPLANE SEND FAIL" : "XPLANE NOT CONNECT");
    return 0;
}

int fmc_data_exec_route_selection(FMC_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    update_exec_ready(data);

    if (data->origin[0] == '\0')
    {
        set_text(data->message, sizeof(data->message), "ENTER ORIGIN");
        return 0;
    }
    if (data->destination[0] == '\0')
    {
        set_text(data->message, sizeof(data->message), "ENTER DEST");
        return 0;
    }
    if (data->flight_no[0] == '\0')
    {
        set_text(data->message, sizeof(data->message), "ENTER FLT NO");
        return 0;
    }

    if (!fmc_data_sync_route_to_xplane(data))
    {
        return 0;
    }

    data->origin_exec_pending = 0;
    return 1;
}

int fmc_data_route_has_uncommitted_changes(const FMC_Data *data)
{
    return data != NULL && data->route_mod_pending;
}

int fmc_data_route_clear_pending(const FMC_Data *data)
{
    return data != NULL && data->route_clear_pending;
}

void fmc_data_mark_route_committed(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->route_mod_pending = 0;
    data->route_clear_pending = 0;
    data->route_delete_pending = 0;
    data->origin_exec_pending = 0;
}
