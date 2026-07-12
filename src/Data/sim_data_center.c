#include "sim_data_center.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SIM_DEG_TO_RAD 0.01745329251994329577f
#define SIM_EICAS_UPPER_FF_SCALE 500.0f
#define SIM_EICAS_LOWER_FF_SCALE 350.0f
#define SIM_FUEL_CENTER_DISPLAY_SCALE 60.9f
#define SIM_FUEL_SIDE_DISPLAY_SCALE 48.7f

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

static void init_snapshot_defaults(SimSnapshot *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->current_frame = -1;
    snapshot->playback_speed = 1.0f;

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

static void set_route_point(SimRoutePoint *point, const char *ident, const char *type, double latitude, double longitude)
{
    if (point == NULL)
    {
        return;
    }

    memset(point, 0, sizeof(*point));
    copy_text(point->ident, sizeof(point->ident), ident);
    copy_text(point->type, sizeof(point->type), type);
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

    if (!sim_data_loader_load_fms_route(&center->planned_route, "assets/KSEAKBFI.fms"))
    {
        init_fallback_route(&center->planned_route);
    }
    center->route_initialized = center->planned_route.valid;

    printf("SimDataCenter route: source=%s origin=%s destination=%s points=%d first=%s last=%s.\n",
           sim_data_center_route_source_name(center->planned_route.source),
           center->planned_route.origin,
           center->planned_route.destination,
           center->planned_route.point_count,
           center->planned_route.point_count > 0 ? center->planned_route.points[0].ident : "----",
           center->planned_route.point_count > 0 ? center->planned_route.points[center->planned_route.point_count - 1].ident : "----");
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

    if (!snapshot->has_pfd && (frame->fields & SIM_ND_FIELD_HEADING) != 0)
    {
        snapshot->heading = normalize_heading(frame->heading);
        snapshot->heading_target = snapshot->heading;
    }

    if ((frame->fields & SIM_ND_FIELD_LATITUDE) != 0 && (frame->fields & SIM_ND_FIELD_LONGITUDE) != 0)
    {
        center->nd_latitude = frame->latitude;
        center->nd_longitude = frame->longitude;
        center->nd_position_initialized = 1;
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
}

int sim_data_center_init(SimDataCenter *center)
{
    if (center == NULL)
    {
        return 0;
    }

    memset(center, 0, sizeof(*center));
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
}

void sim_data_center_set_route(SimDataCenter *center, const SimPlannedRoute *route)
{
    if (center == NULL || route == NULL || !route->valid)
    {
        return;
    }

    center->planned_route = *route;
    center->route_initialized = 1;
}

const SimSnapshot *sim_data_center_snapshot(const SimDataCenter *center)
{
    if (center == NULL || !center->initialized)
    {
        return NULL;
    }
    return &center->snapshot;
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
