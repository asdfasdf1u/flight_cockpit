#include "sim_data_center.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SIM_DEG_TO_RAD 0.01745329251994329577f
#define SIM_EICAS_UPPER_FF_SCALE 500.0f
#define SIM_EICAS_LOWER_FF_SCALE 350.0f
#define SIM_FUEL_CENTER_DISPLAY_SCALE 60.9f
#define SIM_FUEL_SIDE_DISPLAY_SCALE 48.7f
#define SIM_EARTH_RADIUS_NM 3440.065
#define SIM_FLIGHT_PHASE_STABLE_FRAMES 8
#define SIM_XPLANE_RECOVERY_STABLE_FRAMES 2

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

static void copy_text(char *dest, int dest_size, const char *src)
{
    if (dest == NULL || dest_size <= 0)
    {
        return;
    }
    snprintf(dest, (size_t)dest_size, "%s", src != NULL ? src : "");
}

static int sim_valid_route_position(double latitude, double longitude)
{
    return latitude >= -90.0 && latitude <= 90.0 &&
           longitude >= -180.0 && longitude <= 180.0 &&
           (latitude != 0.0 || longitude != 0.0);
}

static double sim_route_distance_nm(double lat1, double lon1, double lat2, double lon2)
{
    const double dlat = (lat2 - lat1) * (double)SIM_DEG_TO_RAD;
    const double dlon = (lon2 - lon1) * (double)SIM_DEG_TO_RAD;
    const double lat1_rad = lat1 * (double)SIM_DEG_TO_RAD;
    const double lat2_rad = lat2 * (double)SIM_DEG_TO_RAD;
    const double sin_dlat = sin(dlat * 0.5);
    const double sin_dlon = sin(dlon * 0.5);
    const double a = sin_dlat * sin_dlat + cos(lat1_rad) * cos(lat2_rad) * sin_dlon * sin_dlon;
    const double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return SIM_EARTH_RADIUS_NM * c;
}

static const SimRoutePoint *sim_find_route_point(const SimPlannedRoute *route, const char *ident)
{
    if (route == NULL || ident == NULL || ident[0] == '\0')
    {
        return NULL;
    }

    for (int i = 0; i < route->point_count; ++i)
    {
        if (strcmp(route->points[i].ident, ident) == 0)
        {
            return &route->points[i];
        }
    }

    return NULL;
}

static void sim_data_center_log_route_diagnostics(const SimDataCenter *center)
{
    if (center == NULL || !center->planned_route.valid)
    {
        return;
    }

    const SimPlannedRoute *route = &center->planned_route;
    const SimRoutePoint *zbbb = sim_find_route_point(route, "ZBBB");
    const SimRoutePoint *zuuu = sim_find_route_point(route, "ZUUU");
    const char *zbbb_source = (zbbb != NULL && zbbb->coordinate_source[0] != '\0')
                                  ? zbbb->coordinate_source
                                  : "INVALID";
    const char *zuuu_source = (zuuu != NULL && zuuu->coordinate_source[0] != '\0')
                                  ? zuuu->coordinate_source
                                  : "INVALID";

    printf("FMC Route Diagnostic: aircraft lat=%.6f lon=%.6f.\n",
           center->nd_latitude,
           center->nd_longitude);

    if (zbbb != NULL && zbbb->has_position && sim_valid_route_position(zbbb->latitude, zbbb->longitude))
    {
        printf("FMC Route Diagnostic: ZBBB lat=%.6f lon=%.6f source=%s aircraft_distance=%.1fNM.\n",
               zbbb->latitude,
               zbbb->longitude,
               zbbb_source,
               sim_route_distance_nm(center->nd_latitude, center->nd_longitude, zbbb->latitude, zbbb->longitude));
    }
    else
    {
        printf("FMC Route Diagnostic: ZBBB lat=INVALID lon=INVALID source=%s aircraft_distance=INVALID.\n",
               zbbb_source);
    }

    if (zuuu != NULL && zuuu->has_position && sim_valid_route_position(zuuu->latitude, zuuu->longitude))
    {
        printf("FMC Route Diagnostic: ZUUU lat=%.6f lon=%.6f source=%s.\n",
               zuuu->latitude,
               zuuu->longitude,
               zuuu_source);
    }
    else
    {
        printf("FMC Route Diagnostic: ZUUU lat=INVALID lon=INVALID source=%s.\n",
               zuuu_source);
    }

    printf("FMC Route Diagnostic: planned_route origin=%s destination=%s point_count=%d revision=%d.\n",
           route->origin[0] != '\0' ? route->origin : "----",
           route->destination[0] != '\0' ? route->destination : "----",
           route->point_count,
           center->route_revision);
}

static int sample_index_from_time(float sim_time, int count)
{
    if (count <= 0)
    {
        return 0;
    }

    int index = (int)floorf(sim_time / SIM_SAMPLE_INTERVAL_SEC);
    if (index < 0)
    {
        index = 0;
    }
    return index % count;
}

static int nd_index_from_time(const SimDataStore *store, float sim_time)
{
    if (store == NULL || store->nd_count <= 0)
    {
        return 0;
    }

    if (!store->nd_has_time)
    {
        return sample_index_from_time(sim_time, store->nd_count);
    }

    const float last_time = store->nd_frames[store->nd_count - 1].time_sec;
    float local_time = sim_time;
    if (last_time > 0.0f && local_time > last_time)
    {
        local_time = fmodf(local_time, last_time);
    }

    int low = 0;
    int high = store->nd_count - 1;
    while (low < high)
    {
        const int mid = (low + high + 1) / 2;
        if (store->nd_frames[mid].time_sec <= local_time)
        {
            low = mid;
        }
        else
        {
            high = mid - 1;
        }
    }
    return low;
}

static void add_warning(SimSnapshot *snapshot, const char *text, SimWarningLevel level)
{
    if (snapshot == NULL || text == NULL || snapshot->warning_count >= SIM_SNAPSHOT_MAX_WARNINGS)
    {
        return;
    }

    SimWarning *warning = &snapshot->warnings[snapshot->warning_count++];
    copy_text(warning->text, sizeof(warning->text), text);
    warning->level = level;
    warning->active = 1;
}

static void update_warnings(SimSnapshot *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }

    snapshot->warning_count = 0;
    for (int i = 0; i < SIM_SNAPSHOT_MAX_WARNINGS; ++i)
    {
        snapshot->warnings[i].text[0] = '\0';
        snapshot->warnings[i].level = SIM_WARNING_INFO;
        snapshot->warnings[i].active = 0;
    }

    if (snapshot->oil_pressure_left < 35.0f)
    {
        add_warning(snapshot, "ENG 1 OIL PRESS", SIM_WARNING_WARNING);
    }
    if (snapshot->oil_pressure_right < 35.0f)
    {
        add_warning(snapshot, "ENG 2 OIL PRESS", SIM_WARNING_WARNING);
    }
    if (snapshot->egt_left > 820.0f)
    {
        add_warning(snapshot, "ENG 1 OVERHEAT", SIM_WARNING_WARNING);
    }
    if (snapshot->egt_right > 820.0f)
    {
        add_warning(snapshot, "ENG 2 OVERHEAT", SIM_WARNING_WARNING);
    }
    if (snapshot->fuel_quantity < 20.0f)
    {
        add_warning(snapshot, "LOW FUEL", SIM_WARNING_CAUTION);
    }
    if (snapshot->hydraulic_pressure < 2500.0f)
    {
        add_warning(snapshot, "HYD PRESS", SIM_WARNING_CAUTION);
    }
    if (snapshot->battery_voltage < 24.0f || snapshot->battery_voltage > 30.0f)
    {
        add_warning(snapshot, "ELEC", SIM_WARNING_CAUTION);
    }
    if (snapshot->gear_down == 0 && snapshot->flaps_level >= 20)
    {
        add_warning(snapshot, "CONFIG", SIM_WARNING_CAUTION);
    }

    if (snapshot->warning_count == 0)
    {
        add_warning(snapshot, "NORMAL", SIM_WARNING_INFO);
    }
}

static int snapshot_has_warning(const SimSnapshot *snapshot)
{
    if (snapshot == NULL)
    {
        return 0;
    }

    for (int i = 0; i < snapshot->warning_count; ++i)
    {
        if (snapshot->warnings[i].active && snapshot->warnings[i].level == SIM_WARNING_WARNING)
        {
            return 1;
        }
    }
    return 0;
}

static SimFlightPhase sim_data_center_classify_flight_phase(const SimSnapshot *snapshot)
{
    if (snapshot == NULL || !snapshot->data_valid)
    {
        return SIM_FLIGHT_PHASE_UNKNOWN;
    }
    if (snapshot_has_warning(snapshot))
    {
        return SIM_FLIGHT_PHASE_EMERGENCY;
    }
    if (snapshot->agl_altitude < 100.0f && snapshot->ground_speed < 40.0f)
    {
        return SIM_FLIGHT_PHASE_GROUND;
    }
    if (snapshot->agl_altitude < 2000.0f && snapshot->vertical_speed < -300.0f)
    {
        return SIM_FLIGHT_PHASE_LANDING;
    }
    if (snapshot->agl_altitude < 3000.0f && snapshot->ground_speed >= 80.0f && snapshot->vertical_speed >= 300.0f)
    {
        return SIM_FLIGHT_PHASE_TAKEOFF;
    }
    if (snapshot->vertical_speed >= 300.0f)
    {
        return SIM_FLIGHT_PHASE_CLIMB;
    }
    if (snapshot->vertical_speed <= -300.0f)
    {
        return SIM_FLIGHT_PHASE_DESCENT;
    }
    return SIM_FLIGHT_PHASE_CRUISE;
}

static void sim_data_center_update_flight_phase(SimDataCenter *center)
{
    SimSnapshot *snapshot;
    SimFlightPhase candidate;

    if (center == NULL)
    {
        return;
    }

    snapshot = &center->snapshot;
    candidate = sim_data_center_classify_flight_phase(snapshot);
    if (candidate == SIM_FLIGHT_PHASE_EMERGENCY || candidate == SIM_FLIGHT_PHASE_UNKNOWN)
    {
        center->flight_phase = candidate;
        center->flight_phase_candidate = candidate;
        center->flight_phase_candidate_frames = 0;
    }
    else if (candidate == center->flight_phase)
    {
        center->flight_phase_candidate = candidate;
        center->flight_phase_candidate_frames = 0;
    }
    else if (candidate != center->flight_phase_candidate)
    {
        center->flight_phase_candidate = candidate;
        center->flight_phase_candidate_frames = 1;
    }
    else if (++center->flight_phase_candidate_frames >= SIM_FLIGHT_PHASE_STABLE_FRAMES)
    {
        center->flight_phase = candidate;
        center->flight_phase_candidate_frames = 0;
    }

    snapshot->flight_phase = center->flight_phase;
}

static void init_snapshot_defaults(SimSnapshot *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->current_frame = -1;
    snapshot->updated_frame = -1;
    snapshot->playback_speed = 1.0f;
    snapshot->source = SIM_SNAPSHOT_SOURCE_NONE;
    snapshot->flight_phase = SIM_FLIGHT_PHASE_UNKNOWN;

    snapshot->pitch = 0.0f;
    snapshot->roll = 0.0f;
    snapshot->yaw = 0.0f;
    snapshot->heading = 90.0f;
    snapshot->heading_target = 90.0f;
    snapshot->altitude = 12000.0f;
    snapshot->altitude_target = 12000.0f;
    snapshot->agl_altitude = 11500.0f;
    snapshot->airspeed = 245.0f;
    snapshot->airspeed_target = 250.0f;
    snapshot->vertical_speed = 0.0f;
    snapshot->throttle = 65.0f;

    snapshot->latitude = 39.904200;
    snapshot->longitude = 116.407400;
    snapshot->track = 3.0f;
    snapshot->ground_speed = 262.0f;
    snapshot->true_air_speed = 262.0f;

    snapshot->total_air_temperature = 11.9f;
    snapshot->n1_left = 63.0f;
    snapshot->n1_right = 67.0f;
    snapshot->engine_left_running = 1;
    snapshot->engine_right_running = 1;
    snapshot->n2_left = 70.5f;
    snapshot->n2_right = 73.0f;
    snapshot->egt_left = 663.0f;
    snapshot->egt_right = 672.0f;
    snapshot->fuel_flow_left = 2450.0f;
    snapshot->fuel_flow_right = 4900.0f;
    snapshot->lower_fuel_flow_left = 2450.0f;
    snapshot->lower_fuel_flow_right = 4900.0f;
    snapshot->oil_pressure_left = 40.2f;
    snapshot->oil_pressure_right = 40.9f;
    snapshot->oil_temperature_left = 90.2f;
    snapshot->oil_temperature_right = 91.5f;
    snapshot->oil_quantity_left = 12.0f;
    snapshot->oil_quantity_right = 12.0f;
    snapshot->vibration_left = 1.0f;
    snapshot->vibration_right = 1.1f;
    snapshot->fuel_quantity = 82.0f;
    snapshot->fuel_center_quantity = snapshot->fuel_quantity * SIM_FUEL_CENTER_DISPLAY_SCALE;
    snapshot->fuel_left_quantity = snapshot->fuel_quantity * SIM_FUEL_SIDE_DISPLAY_SCALE;
    snapshot->fuel_right_quantity = snapshot->fuel_quantity * SIM_FUEL_SIDE_DISPLAY_SCALE;
    snapshot->hydraulic_pressure = 3050.0f;
    snapshot->cabin_pressure = 8.2f;
    snapshot->battery_voltage = 27.8f;
    snapshot->gear_down = 0;
    snapshot->flaps_level = 5;
    snapshot->parking_brake_on = 0;
    update_warnings(snapshot);
}

const char *sim_data_center_route_source_name(SimRouteSource source)
{
    switch (source)
    {
    case SIM_ROUTE_SOURCE_FMC_FMS_FILE:
        return "FMC_FMS_FILE";
    case SIM_ROUTE_SOURCE_FMC_FALLBACK:
        return "FMC_FALLBACK";
    case SIM_ROUTE_SOURCE_NONE:
    default:
        return "NONE";
    }
}

const char *sim_snapshot_source_name(SimSnapshotSource source)
{
    switch (source)
    {
    case SIM_SNAPSHOT_SOURCE_DATA_FILES:
        return "DATA_FILES";
    case SIM_SNAPSHOT_SOURCE_XPLANE:
        return "XPLANE";
    case SIM_SNAPSHOT_SOURCE_NONE:
    default:
        return "NONE";
    }
}

const char *sim_flight_phase_name(SimFlightPhase phase)
{
    switch (phase)
    {
    case SIM_FLIGHT_PHASE_GROUND: return "GROUND";
    case SIM_FLIGHT_PHASE_TAKEOFF: return "TAKEOFF";
    case SIM_FLIGHT_PHASE_CLIMB: return "CLIMB";
    case SIM_FLIGHT_PHASE_CRUISE: return "CRUISE";
    case SIM_FLIGHT_PHASE_DESCENT: return "DESCENT";
    case SIM_FLIGHT_PHASE_LANDING: return "LANDING";
    case SIM_FLIGHT_PHASE_EMERGENCY: return "EMERGENCY";
    case SIM_FLIGHT_PHASE_UNKNOWN:
    default: return "UNKNOWN";
    }
}

static void set_route_point(SimRoutePoint *point, const char *ident, const char *type, double latitude, double longitude)
{
    if (point == NULL)
    {
        return;
    }

    memset(point, 0, sizeof(*point));
    copy_text(point->ident, sizeof(point->ident), ident);
    copy_text(point->type, sizeof(point->type), type);
    copy_text(point->coordinate_source, sizeof(point->coordinate_source), "ROUTE");
    point->latitude = latitude;
    point->longitude = longitude;
    point->altitude = 0.0;
    point->has_position = 1;
}

static void init_fallback_route(SimPlannedRoute *route)
{
    if (route == NULL)
    {
        return;
    }

    memset(route, 0, sizeof(*route));
    route->valid = 1;
    route->source = SIM_ROUTE_SOURCE_FMC_FALLBACK;
    route->loaded_from_file = 0;
    route->has_coordinates = 1;
    route->active_waypoint_index = 1;
    copy_text(route->origin, sizeof(route->origin), "KSEA");
    copy_text(route->destination, sizeof(route->destination), "KBFI");
    copy_text(route->source_path, sizeof(route->source_path), "fallback");
    route->point_count = 2;
    set_route_point(&route->points[0], "KSEA", "AIRPORT", 47.448900, -122.309400);
    set_route_point(&route->points[1], "KBFI", "AIRPORT", 47.540100, -122.309400);
}

static void init_planned_route(SimDataCenter *center)
{
    if (center == NULL)
    {
        return;
    }

    memset(&center->planned_route, 0, sizeof(center->planned_route));
    center->planned_route.source = SIM_ROUTE_SOURCE_NONE;
    center->route_initialized = 0;
    center->route_revision = 0;

    printf("SimDataCenter route: empty at startup; waiting for FMC input.\n");
    fflush(stdout);
}

static void integrate_nd_position(SimDataCenter *center, float delta_time)
{
    if (center == NULL || delta_time <= 0.0f)
    {
        return;
    }

    const float distance_nm = center->snapshot.ground_speed * delta_time / 3600.0f;
    const float track_rad = center->snapshot.track * SIM_DEG_TO_RAD;
    const float latitude_rad = (float)center->nd_latitude * SIM_DEG_TO_RAD;
    const float longitude_scale = cosf(latitude_rad);

    center->nd_latitude += (double)(cosf(track_rad) * distance_nm / 60.0f);
    if (fabsf(longitude_scale) > 0.001f)
    {
        center->nd_longitude += (double)(sinf(track_rad) * distance_nm / (60.0f * longitude_scale));
    }
}

static int sim_route_next_valid_point(const SimPlannedRoute *route, int after_index)
{
    if (route == NULL)
    {
        return -1;
    }
    for (int i = after_index + 1; i < route->point_count; ++i)
    {
        if (route->points[i].has_position &&
            sim_valid_route_position(route->points[i].latitude, route->points[i].longitude))
        {
            return i;
        }
    }
    return -1;
}

static int sim_route_same_geometry(const SimPlannedRoute *left, const SimPlannedRoute *right)
{
    if (left == NULL || right == NULL ||
        left->valid != right->valid ||
        left->point_count != right->point_count ||
        strcmp(left->origin, right->origin) != 0 ||
        strcmp(left->destination, right->destination) != 0)
    {
        return 0;
    }

    for (int i = 0; i < left->point_count; ++i)
    {
        const SimRoutePoint *left_point = &left->points[i];
        const SimRoutePoint *right_point = &right->points[i];
        if (strcmp(left_point->ident, right_point->ident) != 0 ||
            left_point->has_position != right_point->has_position ||
            (left_point->has_position &&
             (fabs(left_point->latitude - right_point->latitude) > 0.0000001 ||
              fabs(left_point->longitude - right_point->longitude) > 0.0000001)))
        {
            return 0;
        }
    }

    return 1;
}

// 把本地 PFD 样本写入统一快照
static void apply_pfd_sample(SimDataCenter *center)
{
    if (center == NULL || center->store.pfd_count <= 0)
    {
        return;
    }

    const int index = sample_index_from_time(center->sim_time, center->store.pfd_count);
    const SimPfdSample *sample = &center->store.pfd_samples[index];
    SimSnapshot *snapshot = &center->snapshot;

    snapshot->has_pfd = 1;
    snapshot->pfd_frame_index = index;
    snapshot->current_frame = index;
    snapshot->airspeed = sample->airspeed;
    snapshot->airspeed_target = sample->airspeed_target;
    snapshot->altitude = sample->altitude;
    snapshot->altitude_target = sample->altitude_target;
    snapshot->agl_altitude = sample->agl_altitude;
    snapshot->pitch = sample->pitch;
    snapshot->roll = sample->roll;
    snapshot->vertical_speed = sample->vertical_speed;
    snapshot->heading = normalize_heading(sample->heading);
    snapshot->heading_target = snapshot->heading;
    snapshot->throttle = clamp_float(sample->throttle, 0.0f, 100.0f);
    snapshot->yaw = sample->yaw;
}

static void apply_nd_frame(SimDataCenter *center, float delta_time)
{
    if (center == NULL || center->store.nd_count <= 0)
    {
        return;
    }

    const int index = nd_index_from_time(&center->store, center->sim_time);
    const SimNdFrame *frame = &center->store.nd_frames[index];
    SimSnapshot *snapshot = &center->snapshot;

    snapshot->has_nd = 1;
    snapshot->nd_frame_index = index;

    if ((frame->fields & SIM_ND_FIELD_GROUND_SPEED) != 0)
    {
        snapshot->ground_speed = frame->ground_speed;
    }
    if ((frame->fields & SIM_ND_FIELD_TRUE_AIR_SPEED) != 0)
    {
        snapshot->true_air_speed = frame->true_air_speed;
    }
    if ((frame->fields & SIM_ND_FIELD_TRACK) != 0)
    {
        snapshot->track = normalize_heading(frame->track);
    }
    else if ((frame->fields & SIM_ND_FIELD_HEADING) != 0)
    {
        snapshot->track = normalize_heading(frame->heading);
    }

    if ((frame->fields & SIM_ND_FIELD_HEADING) != 0)
    {
        snapshot->heading = normalize_heading(frame->heading);
        snapshot->heading_target = snapshot->heading;
    }

    if ((frame->fields & SIM_ND_FIELD_LATITUDE) != 0 && (frame->fields & SIM_ND_FIELD_LONGITUDE) != 0)
    {
        center->nd_latitude = frame->latitude;
        center->nd_longitude = frame->longitude;
        center->nd_position_initialized = 1;
        center->demo_route_origin_initialized = 0;
    }
    else
    {
        if (!center->nd_position_initialized)
        {
            center->nd_latitude = snapshot->latitude;
            center->nd_longitude = snapshot->longitude;
            center->nd_position_initialized = 1;
        }
        integrate_nd_position(center, delta_time);
    }

    snapshot->latitude = center->nd_latitude;
    snapshot->longitude = center->nd_longitude;

    if (snapshot->current_frame < 0)
    {
        snapshot->current_frame = index;
    }
}

static void apply_eicas_upper_frame(SimDataCenter *center)
{
    if (center == NULL || center->store.eicas_upper_count <= 0)
    {
        return;
    }

    const int index = sample_index_from_time(center->sim_time, center->store.eicas_upper_count);
    const SimEicasUpperFrame *frame = &center->store.eicas_upper_frames[index];
    SimSnapshot *snapshot = &center->snapshot;

    snapshot->has_eicas_upper = 1;
    snapshot->eicas_upper_frame_index = index;
    snapshot->total_air_temperature = frame->total_air_temperature;
    snapshot->n1_left = frame->n1_left;
    snapshot->n1_right = frame->n1_right;
    snapshot->egt_left = frame->egt_left;
    snapshot->egt_right = frame->egt_right;
    snapshot->fuel_flow_left = frame->fuel_flow_left_display * SIM_EICAS_UPPER_FF_SCALE;
    snapshot->fuel_flow_right = frame->fuel_flow_right_display * SIM_EICAS_UPPER_FF_SCALE;
    snapshot->fuel_center_quantity = frame->fuel_center_quantity;
    snapshot->fuel_left_quantity = frame->fuel_left_quantity;
    snapshot->fuel_right_quantity = frame->fuel_right_quantity;
    snapshot->fuel_quantity = (frame->fuel_center_quantity + frame->fuel_left_quantity + frame->fuel_right_quantity) /
                              (SIM_FUEL_CENTER_DISPLAY_SCALE + SIM_FUEL_SIDE_DISPLAY_SCALE * 2.0f);

    if (snapshot->current_frame < 0)
    {
        snapshot->current_frame = index;
    }
}

static void apply_eicas_lower_frame(SimDataCenter *center)
{
    if (center == NULL || center->store.eicas_lower_count <= 0)
    {
        return;
    }

    const int index = sample_index_from_time(center->sim_time, center->store.eicas_lower_count);
    const SimEicasLowerFrame *frame = &center->store.eicas_lower_frames[index];
    SimSnapshot *snapshot = &center->snapshot;

    snapshot->has_eicas_lower = 1;
    snapshot->eicas_lower_frame_index = index;
    snapshot->n2_left = frame->n2_left;
    snapshot->n2_right = frame->n2_right;
    snapshot->lower_fuel_flow_left = frame->fuel_flow_left_display * SIM_EICAS_LOWER_FF_SCALE;
    snapshot->lower_fuel_flow_right = frame->fuel_flow_right_display * SIM_EICAS_LOWER_FF_SCALE;
    if (!snapshot->has_eicas_upper)
    {
        snapshot->fuel_flow_left = snapshot->lower_fuel_flow_left;
        snapshot->fuel_flow_right = snapshot->lower_fuel_flow_right;
    }
    snapshot->oil_pressure_left = frame->oil_pressure_left;
    snapshot->oil_pressure_right = frame->oil_pressure_right;
    snapshot->oil_temperature_left = frame->oil_temperature_left;
    snapshot->oil_temperature_right = frame->oil_temperature_right;
    snapshot->oil_quantity_left = frame->oil_quantity_left;
    snapshot->oil_quantity_right = frame->oil_quantity_right;
    snapshot->vibration_left = frame->vibration_left;
    snapshot->vibration_right = frame->vibration_right;

    if (snapshot->current_frame < 0)
    {
        snapshot->current_frame = index;
    }
}

// 按本地样本重建统一快照
static void rebuild_snapshot(SimDataCenter *center, float delta_time)
{
    if (center == NULL)
    {
        return;
    }

    init_snapshot_defaults(&center->snapshot);
    center->snapshot.sim_time = center->sim_time;
    center->snapshot.delta_time = center->delta_time;
    center->snapshot.playback_speed = center->playback_speed;

    apply_pfd_sample(center);
    apply_nd_frame(center, delta_time);
    apply_eicas_upper_frame(center);
    apply_eicas_lower_frame(center);
    update_warnings(&center->snapshot);
    center->snapshot.engine_left_running = center->snapshot.n1_left > 20.0f;
    center->snapshot.engine_right_running = center->snapshot.n1_right > 20.0f;
    center->snapshot.source = center->initialized ? SIM_SNAPSHOT_SOURCE_DATA_FILES : SIM_SNAPSHOT_SOURCE_NONE;
    center->snapshot.data_valid = center->initialized && center->snapshot.has_pfd && center->snapshot.has_nd &&
                                  isfinite(center->snapshot.altitude) && isfinite(center->snapshot.ground_speed) &&
                                  isfinite(center->snapshot.latitude) && isfinite(center->snapshot.longitude);
    center->snapshot.updated_frame = center->snapshot.current_frame;
    center->snapshot.frame_id = center->snapshot.current_frame;
    center->snapshot.timestamp = center->snapshot.sim_time;
    center->snapshot.fallback_active = center->snapshot.source == SIM_SNAPSHOT_SOURCE_DATA_FILES;
    sim_data_center_update_flight_phase(center);
    alert_manager_update(&center->alert_manager, &center->snapshot);
    alert_manager_append_sim_warnings(alert_manager_snapshot(&center->alert_manager), &center->snapshot);
}
// 把实时飞行帧转换成全系统统一快照
static void rebuild_xplane_snapshot_from_frame(SimDataCenter *center, const SimXPlaneLiveFrame *frame)
{
    SimSnapshot previous_snapshot;
    SimSnapshot *snapshot;

    if (center == NULL || frame == NULL)
    {
        return;
    }

    previous_snapshot = center->snapshot;
    snapshot = &center->snapshot;
    init_snapshot_defaults(snapshot);
    snapshot->source = SIM_SNAPSHOT_SOURCE_XPLANE;
    snapshot->data_valid = frame->valid && frame->connected && !frame->timed_out;
    snapshot->has_pfd = 1;
    snapshot->has_nd = 1;
    snapshot->has_eicas_upper = 1;
    snapshot->has_eicas_lower = 1;
    snapshot->sim_time = frame->timestamp;
    snapshot->delta_time = frame->delta_time;
    snapshot->timestamp = frame->timestamp;
    snapshot->last_valid_timestamp = frame->last_valid_timestamp;
    snapshot->frame_id = frame->frame_id;
    snapshot->current_frame = frame->frame_id;
    snapshot->updated_frame = frame->frame_id;
    snapshot->pfd_frame_index = frame->frame_id;
    snapshot->nd_frame_index = frame->frame_id;
    snapshot->eicas_upper_frame_index = frame->frame_id;
    snapshot->eicas_lower_frame_index = frame->frame_id;
    snapshot->timed_out = frame->timed_out;
    snapshot->xplane_connected = frame->connected;
    snapshot->fallback_active = 0;
    snapshot->last_valid_xplane_timestamp = frame->last_valid_timestamp;
    snapshot->playback_speed = center->playback_speed;

    snapshot->latitude = frame->latitude;
    snapshot->longitude = frame->longitude;
    snapshot->altitude = frame->altitude;
    snapshot->altitude_target = frame->altitude_target;
    snapshot->agl_altitude = frame->agl_altitude;
    snapshot->airspeed = frame->airspeed;
    snapshot->airspeed_target = frame->airspeed_target;
    snapshot->true_air_speed = frame->true_air_speed;
    snapshot->ground_speed = frame->ground_speed;
    snapshot->heading = normalize_heading(frame->heading);
    snapshot->heading_target = normalize_heading(frame->heading_target);
    snapshot->track = normalize_heading(frame->track);
    snapshot->pitch = frame->pitch;
    snapshot->roll = frame->roll;
    snapshot->yaw = frame->yaw;
    snapshot->vertical_speed = frame->vertical_speed;
    snapshot->throttle = clamp_float(frame->throttle, 0.0f, 100.0f);

    snapshot->total_air_temperature = frame->total_air_temperature;
    snapshot->n1_left = frame->n1_left;
    snapshot->n1_right = frame->n1_right;
    snapshot->engine_left_running = frame->engine_left_running;
    snapshot->engine_right_running = frame->engine_right_running;
    snapshot->n2_left = frame->n2_left;
    snapshot->n2_right = frame->n2_right;
    snapshot->egt_left = frame->egt_left;
    snapshot->egt_right = frame->egt_right;
    snapshot->fuel_flow_left = frame->fuel_flow_left;
    snapshot->fuel_flow_right = frame->fuel_flow_right;
    snapshot->lower_fuel_flow_left = frame->fuel_flow_left;
    snapshot->lower_fuel_flow_right = frame->fuel_flow_right;
    snapshot->oil_pressure_left = frame->oil_pressure_left;
    snapshot->oil_pressure_right = frame->oil_pressure_right;
    snapshot->oil_temperature_left = frame->oil_temperature_left;
    snapshot->oil_temperature_right = frame->oil_temperature_right;
    snapshot->oil_quantity_left = frame->oil_quantity_left;
    snapshot->oil_quantity_right = frame->oil_quantity_right;
    snapshot->vibration_left = frame->vibration_left;
    snapshot->vibration_right = frame->vibration_right;
    snapshot->fuel_quantity = frame->fuel_quantity;
    snapshot->fuel_left_quantity = frame->fuel_left_quantity;
    snapshot->fuel_center_quantity = frame->fuel_center_quantity;
    snapshot->fuel_right_quantity = frame->fuel_right_quantity;
    snapshot->hydraulic_pressure = previous_snapshot.hydraulic_pressure;
    snapshot->cabin_pressure = previous_snapshot.cabin_pressure;
    snapshot->battery_voltage = previous_snapshot.battery_voltage;
    snapshot->gear_down = frame->gear_down;
    snapshot->flaps_level = frame->flaps_level;
    snapshot->parking_brake_on = frame->parking_brake_on;

    /* 同步 X-Plane 原生报警状态（通过 getDREF 获取）。
     * 这些布尔状态用于 cockpit_alarm 直接驱动 MASTER WARNING / MASTER CAUTION 灯。 */
    snapshot->xplane_master_warning = frame->xplane_master_warning;
    snapshot->xplane_master_caution = frame->xplane_master_caution;
    snapshot->xplane_engine_fire = frame->xplane_engine_fire;
    snapshot->xplane_stall_warning = frame->xplane_stall_warning;
    snapshot->xplane_overspeed_warning = frame->xplane_overspeed_warning;

    update_warnings(snapshot);
    sim_data_center_update_flight_phase(center);
    alert_manager_update(&center->alert_manager, &center->snapshot);
    alert_manager_append_sim_warnings(alert_manager_snapshot(&center->alert_manager), &center->snapshot);
}

int sim_data_center_init(SimDataCenter *center)
{
    if (center == NULL)
    {
        return 0;
    }

    memset(center, 0, sizeof(*center));
    alert_manager_init(&center->alert_manager);
    center->playback_speed = 1.0f;
    center->nd_latitude = 39.904200;
    center->nd_longitude = 116.407400;
    center->nd_position_initialized = 1;
    center->initialized = sim_data_loader_load_all(&center->store);
    init_planned_route(center);
    rebuild_snapshot(center, 0.0f);

    if (center->initialized)
    {
        printf("SimDataCenter: initialized, unified sim_time active.\n");
    }
    else
    {
        printf("SimDataCenter: no usable assets loaded, modules will use fallback.\n");
    }
    fflush(stdout);
    return center->initialized;
}

void sim_data_center_destroy(SimDataCenter *center)
{
    (void)center;
}

void sim_data_center_update(SimDataCenter *center, float delta_time)
{
    if (center == NULL || !center->initialized)
    {
        return;
    }

    if (delta_time < 0.0f)
    {
        delta_time = 0.0f;
    }
    if (delta_time > 0.1f)
    {
        delta_time = 0.1f;
    }

    center->delta_time = delta_time * center->playback_speed;
    center->sim_time += center->delta_time;
    rebuild_snapshot(center, center->delta_time);
}

void sim_data_center_set_playback_speed(SimDataCenter *center, float playback_speed)
{
    if (center == NULL)
    {
        return;
    }

    center->playback_speed = clamp_float(playback_speed, 0.0f, 16.0f);
    center->snapshot.playback_speed = center->playback_speed;
}

void sim_data_center_set_position(SimDataCenter *center, double latitude, double longitude)
{
    if (center == NULL ||
        latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0)
    {
        return;
    }

    center->nd_latitude = latitude;
    center->nd_longitude = longitude;
    center->nd_position_initialized = 1;
    center->snapshot.latitude = latitude;
    center->snapshot.longitude = longitude;
    center->demo_route_origin_initialized = 0;
}

static void sim_data_center_log_source_change(SimDataCenter *center, const char *reason)
{
    const SimSnapshotSource source = center != NULL ? center->snapshot.source : SIM_SNAPSHOT_SOURCE_NONE;

    if (center == NULL)
    {
        return;
    }

    if (!center->source_log_initialized || center->last_logged_source != source)
    {
        printf("SimDataCenter source: %s frame_id=%d valid=%d fallback=%d xplane_connected=%d timeout=%d reason=%s\n",
               sim_snapshot_source_name(source),
               center->snapshot.frame_id,
               center->snapshot.data_valid,
               center->snapshot.fallback_active,
               center->snapshot.xplane_connected,
               center->snapshot.timed_out,
               reason != NULL ? reason : "update");
        fflush(stdout);
        center->source_log_initialized = 1;
        center->last_logged_source = source;
    }
}

// X-Plane 不可用时重建本地回退快照
static int sim_data_center_rebuild_fallback_snapshot(
    SimDataCenter *center,
    const SimXPlaneLiveFrame *frame,
    const char *reason)
{
    float delta_time;

    if (center == NULL || !center->initialized)
    {
        return 0;
    }

    delta_time = frame != NULL ? frame->delta_time : center->delta_time;
    if (delta_time < 0.0f)
    {
        delta_time = 0.0f;
    }
    if (delta_time > 0.1f)
    {
        delta_time = 0.1f;
    }

    center->delta_time = delta_time * center->playback_speed;
    center->sim_time += center->delta_time;
    rebuild_snapshot(center, center->delta_time);
    center->snapshot.xplane_connected = frame != NULL ? frame->connected : 0;
    center->snapshot.timed_out = frame != NULL ? frame->timed_out : 0;
    center->snapshot.last_valid_xplane_timestamp = frame != NULL ? frame->last_valid_timestamp : center->snapshot.last_valid_xplane_timestamp;
    center->snapshot.last_valid_timestamp = center->snapshot.last_valid_xplane_timestamp;
    center->snapshot.fallback_active = center->snapshot.source == SIM_SNAPSHOT_SOURCE_DATA_FILES;
    sim_data_center_log_source_change(center, reason);
    return center->snapshot.data_valid;
}
// 让统一数据中心接收 X-Plane 实时数据
int sim_data_center_apply_xplane_live_frame(SimDataCenter *center, const SimXPlaneLiveFrame *frame)
{
    int xplane_ready;

    if (center == NULL || frame == NULL || !center->initialized)
    {
        return 0;
    }

    xplane_ready = frame->valid && frame->connected && !frame->timed_out;
    if (xplane_ready)
    {
        center->xplane_recovery_frames++;
        if (center->snapshot.source != SIM_SNAPSHOT_SOURCE_XPLANE &&
            center->xplane_recovery_frames < SIM_XPLANE_RECOVERY_STABLE_FRAMES)
        {
            return sim_data_center_rebuild_fallback_snapshot(center, frame, "xplane_recovering");
        }

        rebuild_xplane_snapshot_from_frame(center, frame);
        center->sim_time = frame->timestamp;
        center->delta_time = frame->delta_time;
        center->nd_latitude = frame->latitude;
        center->nd_longitude = frame->longitude;
        center->nd_position_initialized = 1;
        center->demo_route_origin_initialized = 0;
        sim_data_center_log_source_change(center, "xplane_valid");
        return 1;
    }

    center->xplane_recovery_frames = 0;
    center->snapshot.xplane_connected = frame->connected;
    center->snapshot.timed_out = frame->timed_out;
    center->snapshot.last_valid_xplane_timestamp = frame->last_valid_timestamp;
    center->snapshot.last_valid_timestamp = frame->last_valid_timestamp;

    if (!frame->connected || frame->timed_out || center->snapshot.source != SIM_SNAPSHOT_SOURCE_XPLANE)
    {
        return sim_data_center_rebuild_fallback_snapshot(center, frame, !frame->connected ? "xplane_disconnected" : (frame->timed_out ? "xplane_timeout" : "xplane_unavailable"));
    }

    sim_data_center_log_source_change(center, "xplane_transient_invalid");
    return 0;
}

void sim_data_center_set_route(SimDataCenter *center, const SimPlannedRoute *route)
{
    if (center == NULL || route == NULL || !route->valid)
    {
        return;
    }

    const int route_geometry_changed = !center->route_initialized ||
                                       !sim_route_same_geometry(&center->planned_route, route);

    center->planned_route = *route;
    center->route_initialized = 1;
    center->route_revision++;
    if (route_geometry_changed)
    {
        center->demo_route_origin_initialized = 0;
        if (center->snapshot.source == SIM_SNAPSHOT_SOURCE_DATA_FILES)
        {
            const int first_index = sim_route_next_valid_point(&center->planned_route, -1);
            if (first_index >= 0)
            {
                center->nd_latitude = center->planned_route.points[first_index].latitude;
                center->nd_longitude = center->planned_route.points[first_index].longitude;
                center->nd_position_initialized = 1;
                center->snapshot.latitude = center->nd_latitude;
                center->snapshot.longitude = center->nd_longitude;
                center->demo_route_origin_initialized = 1;
                printf("SimDataCenter route: DATA_FILES demo position initialized once at %s lat=%.6f lon=%.6f; subsequent movement follows nd.dat track.\n",
                       center->planned_route.points[first_index].ident,
                       center->nd_latitude,
                       center->nd_longitude);
            }
        }
    }
    printf("SimDataCenter route: committed revision=%d origin=%s destination=%s points=%d.\n",
           center->route_revision,
           center->planned_route.origin,
           center->planned_route.destination,
           center->planned_route.point_count);
    sim_data_center_log_route_diagnostics(center);
}

void sim_data_center_clear_route(SimDataCenter *center)
{
    if (center == NULL)
    {
        return;
    }

    memset(&center->planned_route, 0, sizeof(center->planned_route));
    center->route_initialized = 0;
    center->route_revision++;
    center->demo_route_origin_initialized = 0;
    printf("SimDataCenter route: cleared revision=%d.\n", center->route_revision);
}

int sim_data_center_route_revision(const SimDataCenter *center)
{
    return center != NULL ? center->route_revision : 0;
}
// 给 PFD、ND、EICAS 提供统一数据--最新的统一快照
const SimSnapshot *sim_data_center_snapshot(const SimDataCenter *center)
{
    if (center == NULL || !center->initialized)
    {
        return NULL;
    }
    return &center->snapshot;
}

const AlertSnapshot *sim_data_center_alerts(const SimDataCenter *center)
{
    return center != NULL ? alert_manager_snapshot(&center->alert_manager) : NULL;
}

const SimPlannedRoute *sim_data_center_route(const SimDataCenter *center)
{
    if (center == NULL || !center->route_initialized || !center->planned_route.valid)
    {
        return NULL;
    }
    return &center->planned_route;
}

int sim_data_center_is_ready(const SimDataCenter *center)
{
    return center != NULL && center->initialized;
}

int sim_data_center_has_route(const SimDataCenter *center)
{
    return center != NULL && center->route_initialized && center->planned_route.valid;
}

int sim_data_center_has_pfd_data(const SimDataCenter *center)
{
    return center != NULL && center->store.pfd_count > 0;
}

int sim_data_center_has_nd_data(const SimDataCenter *center)
{
    return center != NULL && center->store.nd_count > 0;
}

int sim_data_center_has_nd_position_data(const SimDataCenter *center)
{
    if (center == NULL)
    {
        return 0;
    }

    for (int i = 0; i < center->store.nd_count; ++i)
    {
        const SimNdFrame *frame = &center->store.nd_frames[i];
        if ((frame->fields & SIM_ND_FIELD_LATITUDE) != 0 &&
            (frame->fields & SIM_ND_FIELD_LONGITUDE) != 0)
        {
            return 1;
        }
    }
    return 0;
}

int sim_data_center_has_eicas_upper_data(const SimDataCenter *center)
{
    return center != NULL && center->store.eicas_upper_count > 0;
}

int sim_data_center_has_eicas_lower_data(const SimDataCenter *center)
{
    return center != NULL && center->store.eicas_lower_count > 0;
}

int sim_data_center_has_eicas_data(const SimDataCenter *center)
{
    return sim_data_center_has_eicas_upper_data(center) || sim_data_center_has_eicas_lower_data(center);
}

void sim_data_center_acknowledge_alert(SimDataCenter *center, AlertType type)
{
    if (center != NULL)
    {
        alert_manager_acknowledge(&center->alert_manager, type);
    }
}

void sim_data_center_set_demo_alert(SimDataCenter *center, AlertType type, int active)
{
    const AlertSnapshot *alerts;

    if (center == NULL)
    {
        return;
    }
    alert_manager_set_demo(&center->alert_manager, type, active);
    alert_manager_update(&center->alert_manager, &center->snapshot);
    alerts = alert_manager_snapshot(&center->alert_manager);
    printf("Alert demo: %s=%d active=%d revision=%d.\n", alert_type_name(type), active != 0,
           alerts->active_count, alerts->revision);
}

void sim_data_center_clear_demo_alerts(SimDataCenter *center)
{
    const AlertSnapshot *alerts;

    if (center == NULL)
    {
        return;
    }
    alert_manager_clear_demo(&center->alert_manager);
    alert_manager_update(&center->alert_manager, &center->snapshot);
    alerts = alert_manager_snapshot(&center->alert_manager);
    printf("Alert demo: cleared active=%d revision=%d.\n", alerts->active_count, alerts->revision);
}
