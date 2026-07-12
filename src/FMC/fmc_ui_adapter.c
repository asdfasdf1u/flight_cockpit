#include "fmc_ui_adapter.h"

#include "fmc_connect.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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
           data->origin[0] != '\0';
}

static void update_exec_ready(FMC_Data *data)
{
    if (data != NULL)
    {
        data->origin_exec_pending = route_ready_to_exec(data);
    }
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
    }
    if (via_to_list != NULL)
    {
        via_to_list_count = 0;
    }
    rte_index = 1;
}

static int add_route_point_geo(FMC_Data *data, const char *ident, double latitude, double longitude, int has_position)
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
    data->route_count++;
    return 1;
}

static int add_route_point(FMC_Data *data, const char *ident)
{
    return add_route_point_geo(data, ident, 0.0, 0.0, 0);
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

    if (via_to_list == NULL)
    {
        initVIATO();
    }
    if (via_to_list == NULL)
    {
        set_text(data->message, sizeof(data->message), "ROUTE MEMORY ERR");
        return 0;
    }
    if (via_to_list_count >= MAX_VIATO_NUM)
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
    set_text(via_to_list[via_to_list_count].VIA, sizeof(via_to_list[via_to_list_count].VIA), "DIRECT");
    set_text(via_to_list[via_to_list_count].TO, sizeof(via_to_list[via_to_list_count].TO), to);
    if (data->route_count < FMC_MAX_ROUTE_POINTS)
    {
        if (wpt != NULL)
        {
            add_route_point_geo(data, to, wpt->lat, wpt->lon, 1);
        }
        else if (arp != NULL)
        {
            add_route_point_geo(data, to, arp->datum_lat, arp->datum_lon, 1);
        }
        else
        {
            add_route_point(data, to);
        }
    }
    via_to_list_count++;
    data->origin_exec_pending = 1;
    fmc_data_clear_scratchpad(data);
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
            }
            continue;
        }

        add_route_point_geo(data, ident, latitude, longitude, 1);
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
        Airport *arp = fmc_query_airport_by_icao(data->destination);
        if (arp != NULL)
        {
            add_route_point_geo(data, data->destination, arp->datum_lat, arp->datum_lon, 1);
        }
        else
        {
            add_route_point(data, data->destination);
        }
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
    set_text(dest_field, dest_size, airport != NULL ? airport->icao_code : airport_code);
    fmc_data_clear_scratchpad(data);
    sync_library_route_fields(data);
    update_exec_ready(data);
    update_auto_route(data);
    data->origin_exec_pending = 1;
    if (data->origin[0] != '\0' && data->destination[0] != '\0' && data->route_count > 0)
    {
        return 1;
    }
    snprintf(data->message, sizeof(data->message), "%s SET%s", label, airport != NULL ? "" : " NO COORD");
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
    snprintf(data->message, sizeof(data->message), "%s SET", label);
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
    initVIATO();
    load_airport_data();
    load_waypoint_data();
}

void fmc_data_destroy(FMC_Data *data)
{
    (void)data;
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
        return 1;
    }
    data->configured_route_page = rte_index - 1;
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
        return 1;
    }
    data->configured_route_page = rte_index - 1;
    return 0;
}

int fmc_data_set_route_field(FMC_Data *data, FMC_RouteField field)
{
    if (data == NULL)
    {
        return 0;
    }

    switch (field)
    {
    case FMC_ROUTE_FIELD_ORIGIN:
        return set_airport_field(data, data->origin, sizeof(data->origin), "ORIGIN");
    case FMC_ROUTE_FIELD_DESTINATION:
        return set_airport_field(data, data->destination, sizeof(data->destination), "DEST");
    case FMC_ROUTE_FIELD_COMPANY_ROUTE:
        return set_scratchpad_text(data, data->company_route, sizeof(data->company_route), "CO ROUTE");
    case FMC_ROUTE_FIELD_FLIGHT_NO:
        return set_scratchpad_text(data, data->flight_no, sizeof(data->flight_no), "FLT NO");
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

int fmc_data_set_phase_parameter(FMC_Data *data, int field_index)
{
    if (data == NULL)
    {
        return 0;
    }

    if (field_index == 1)
    {
        return set_scratchpad_text(data, data->climb_target_speed_text, sizeof(data->climb_target_speed_text), "SPEED");
    }
    if (field_index == 2)
    {
        return set_scratchpad_text(data, data->climb_spd_alt_limit_text, sizeof(data->climb_spd_alt_limit_text), "SPD/ALT");
    }
    if (field_index == 3)
    {
        return set_scratchpad_text(data, data->climb_transition_alt_text, sizeof(data->climb_transition_alt_text), "TRANS ALT");
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

int fmc_data_set_legs_parameter(FMC_Data *data, int field_index)
{
    (void)field_index;
    if (data == NULL)
    {
        return 0;
    }
    return set_scratchpad_text(data, data->legs_sequence, sizeof(data->legs_sequence), "LEGS");
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

static void set_sim_route_point(SimRoutePoint *point, const char *ident, const char *type, double latitude, double longitude, int has_position)
{
    if (point == NULL)
    {
        return;
    }

    memset(point, 0, sizeof(*point));
    set_text(point->ident, sizeof(point->ident), ident);
    set_text(point->type, sizeof(point->type), type);
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
    set_text(data->fms_plan_path, sizeof(data->fms_plan_path), route->source_path);
    data->route_loaded_from_file = route->source == SIM_ROUTE_SOURCE_FMC_FMS_FILE;
    data->route_source = route->source;
    data->active_waypoint_index = route->active_waypoint_index > 0 ? route->active_waypoint_index - 1 : 0;

    if (via_to_list == NULL)
    {
        initVIATO();
    }

    for (int i = 1; i < route->point_count && data->route_count < FMC_MAX_ROUTE_POINTS; ++i)
    {
        const SimRoutePoint *point = &route->points[i];
        add_route_point_geo(data, point->ident, point->latitude, point->longitude, point->has_position);
        if (via_to_list != NULL && via_to_list_count < MAX_VIATO_NUM)
        {
            set_text(via_to_list[via_to_list_count].VIA, sizeof(via_to_list[via_to_list_count].VIA), "DIRECT");
            set_text(via_to_list[via_to_list_count].TO, sizeof(via_to_list[via_to_list_count].TO), point->ident);
            via_to_list_count++;
        }
    }

    sync_library_route_fields(data);
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
    route->active_waypoint_index = data->active_waypoint_index + 1;

    int coordinate_count = 0;
    if (route->point_count < SIM_ROUTE_MAX_POINTS && data->origin[0] != '\0')
    {
        Airport *origin_airport = fmc_query_airport_by_icao(data->origin);
        const int has_position = origin_airport != NULL && fmc_valid_position(origin_airport->datum_lat, origin_airport->datum_lon);
        set_sim_route_point(&route->points[route->point_count++],
                            data->origin,
                            "AIRPORT",
                            has_position ? origin_airport->datum_lat : 0.0,
                            has_position ? origin_airport->datum_lon : 0.0,
                            has_position);
        if (has_position)
        {
            coordinate_count++;
        }
    }

    for (int i = 0; i < data->route_count && route->point_count < SIM_ROUTE_MAX_POINTS; ++i)
    {
        const int has_position = data->route_has_position[i] &&
                                 fmc_valid_position(data->route_latitudes[i], data->route_longitudes[i]);
        set_sim_route_point(&route->points[route->point_count++],
                            data->route_points[i],
                            strcmp(data->route_points[i], data->destination) == 0 ? "AIRPORT" : "FIX",
                            has_position ? data->route_latitudes[i] : 0.0,
                            has_position ? data->route_longitudes[i] : 0.0,
                            has_position);
        if (has_position)
        {
            coordinate_count++;
        }
    }

    route->valid = route->point_count > 0;
    route->has_coordinates = route->point_count > 0 && coordinate_count == route->point_count;
    return route->valid;
}

int fmc_data_exec_route_selection(FMC_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    update_exec_ready(data);

    if (!data->origin_exec_pending)
    {
        set_text(data->message, sizeof(data->message), "ENTER ORIGIN");
        return 0;
    }

    setOrigin(data->origin);
    if (data->destination[0] != '\0')
    {
        setDestination(data->destination);
    }
    if (data->flight_no[0] != '\0')
    {
        setFlt_no(data->flight_no);
    }
    data->origin_exec_pending = 0;
    if (data->destination[0] != '\0')
    {
        snprintf(data->message, sizeof(data->message), "RTE %s-%s SENT", data->origin, data->destination);
    }
    else
    {
        snprintf(data->message, sizeof(data->message), "ORIGIN %s SENT", data->origin);
    }
    return 1;
}
