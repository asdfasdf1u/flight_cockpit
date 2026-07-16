#include "cabin_data.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../Data/sim_data_center.h"

#define CABIN_ROUTE_TOTAL_TIME_MIN 165.0f
#define CABIN_ROUTE_PROGRESS_RATE 0.012f
#define CABIN_TRAJECTORY_PROGRESS_THRESHOLD 0.006f
#define CABIN_TRAJECTORY_UPDATE_INTERVAL 1.2f
#define CABIN_TRAJECTORY_DUPLICATE_EPSILON 0.000001
#define CABIN_PI 3.14159265358979323846
#define CABIN_MAP_CACHE_DIR "assets/cache"
#define CABIN_MAP_MIN_LAT_SPAN 0.18
#define CABIN_MAP_MIN_LON_SPAN 0.18
#define CABIN_MAP_ROUTE_MARGIN_RATIO 0.30
#define CABIN_MAP_RECENTER_LAT_THRESHOLD 0.08
#define CABIN_MAP_RECENTER_LON_THRESHOLD 0.10

typedef struct Cabin_Route_Bounds
{
    double min_lat;
    double max_lat;
    double min_lon;
    double max_lon;
    int count;
} Cabin_Route_Bounds;

static void copy_text(char *dest, size_t dest_size, const char *src)
{
    if (dest == NULL || dest_size == 0)
    {
        return;
    }

    snprintf(dest, dest_size, "%s", src != NULL ? src : "");
}

const char *cabin_place_display_name(const Cabin_Place *place)
{
    if (place == NULL || place->status != CABIN_PLACE_VALID)
    {
        return "";
    }
    if (place->city[0] != '\0')
    {
        return place->city;
    }
    if (place->province[0] != '\0')
    {
        return place->province;
    }
    return place->district;
}

const char *cabin_place_street_or_town(const Cabin_Place *place)
{
    if (place == NULL || place->status != CABIN_PLACE_VALID)
    {
        return "";
    }
    if (place->township[0] != '\0')
    {
        return place->township;
    }
    if (place->street[0] != '\0')
    {
        return place->street;
    }
    return place->formatted_address;
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

static int cabin_data_valid_geo(double latitude, double longitude)
{
    return isfinite(latitude) && isfinite(longitude) &&
           latitude >= -90.0 && latitude <= 90.0 &&
           longitude >= -180.0 && longitude <= 180.0;
}

static int cabin_data_compute_route_bounds(const Cabin_Data *data, Cabin_Route_Bounds *bounds)
{
    if (data == NULL || bounds == NULL)
    {
        return 0;
    }

    memset(bounds, 0, sizeof(*bounds));
    for (int i = 0; i < data->planned_route_count; ++i)
    {
        const double lat = data->planned_route[i].latitude;
        const double lon = data->planned_route[i].longitude;
        if (!cabin_data_valid_geo(lat, lon))
        {
            continue;
        }

        if (bounds->count == 0)
        {
            bounds->min_lat = lat;
            bounds->max_lat = lat;
            bounds->min_lon = lon;
            bounds->max_lon = lon;
        }
        else
        {
            if (lat < bounds->min_lat)
            {
                bounds->min_lat = lat;
            }
            if (lat > bounds->max_lat)
            {
                bounds->max_lat = lat;
            }
            if (lon < bounds->min_lon)
            {
                bounds->min_lon = lon;
            }
            if (lon > bounds->max_lon)
            {
                bounds->max_lon = lon;
            }
        }
        bounds->count++;
    }

    return bounds->count > 0;
}

static int cabin_data_select_map_zoom(double lat_span, double lon_span)
{
    const double span = lat_span > lon_span ? lat_span : lon_span;

    if (span <= 0.20)
    {
        return 11;
    }
    if (span <= 0.50)
    {
        return 10;
    }
    if (span <= 1.20)
    {
        return 9;
    }
    if (span <= 2.50)
    {
        return 8;
    }
    if (span <= 5.00)
    {
        return 7;
    }
    if (span <= 10.00)
    {
        return 6;
    }
    if (span <= 18.00)
    {
        return 5;
    }
    return 4;
}

static void cabin_data_append_safe_cache_part(char *dest, size_t dest_size, const char *text)
{
    size_t used = dest != NULL ? strlen(dest) : 0;
    int wrote = 0;

    if (dest == NULL || dest_size == 0 || text == NULL)
    {
        return;
    }

    for (int i = 0; text[i] != '\0' && used + 1 < dest_size; ++i)
    {
        unsigned char ch = (unsigned char)text[i];
        if (isalnum(ch))
        {
            dest[used++] = (char)toupper(ch);
            wrote = 1;
        }
        else if ((ch == '_' || ch == '-' || ch == ' ') && wrote && used + 1 < dest_size)
        {
            if (dest[used - 1] != '_')
            {
                dest[used++] = '_';
            }
        }
    }

    while (used > 0 && dest[used - 1] == '_')
    {
        --used;
    }
    dest[used] = '\0';
}

static void cabin_data_build_map_cache_path(Cabin_Data *data)
{
    char route_name[64];
    const int center_lat_units = (int)(fabs(data != NULL ? data->map_center_lat : 0.0) * 10000.0 + 0.5);
    const int center_lon_units = (int)(fabs(data != NULL ? data->map_center_lon : 0.0) * 10000.0 + 0.5);

    if (data == NULL)
    {
        return;
    }

    route_name[0] = '\0';
    if (data->planned_route_from_fmc)
    {
        cabin_data_append_safe_cache_part(route_name, sizeof(route_name), data->origin_airport);
        if (route_name[0] != '\0')
        {
            strncat(route_name, "_", sizeof(route_name) - strlen(route_name) - 1);
        }
        cabin_data_append_safe_cache_part(route_name, sizeof(route_name), data->destination_airport);
    }
    else if (cabin_data_valid_geo(data->map_center_lat, data->map_center_lon))
    {
        const int lat_tenths = (int)(fabs(data->map_center_lat) * 10.0 + 0.5);
        const int lon_tenths = (int)(fabs(data->map_center_lon) * 10.0 + 0.5);
        snprintf(route_name,
                 sizeof(route_name),
                 "position_%c%04d_%c%04d",
                 data->map_center_lat >= 0.0 ? 'N' : 'S',
                 lat_tenths,
                 data->map_center_lon >= 0.0 ? 'E' : 'W',
                 lon_tenths);
    }
    else
    {
        copy_text(route_name, sizeof(route_name), "position");
    }

    if (route_name[0] == '\0')
    {
        copy_text(route_name, sizeof(route_name), "ROUTE");
    }

    snprintf(data->map_cache_path,
             sizeof(data->map_cache_path),
             "%s/cabin_map_%s_c%c%06d_%c%07d_z%02d_%dx%d.png",
             CABIN_MAP_CACHE_DIR,
             route_name,
             data->map_center_lat >= 0.0 ? 'N' : 'S',
             center_lat_units,
             data->map_center_lon >= 0.0 ? 'E' : 'W',
             center_lon_units,
             data->map_zoom,
             CABIN_MAP_STATIC_WIDTH,
             CABIN_MAP_STATIC_HEIGHT);
}

void cabin_data_request_map_refresh(Cabin_Data *data)
{
    if (data == NULL || data->map_zoom < data->map_min_zoom || data->map_zoom > data->map_max_zoom)
    {
        return;
    }
    data->map_refresh_requested = 1;
    data->map_request_revision++;
}

int cabin_data_request_map_zoom(Cabin_Data *data, int delta)
{
    int next_zoom;

    if (data == NULL || delta == 0 || !data->map_api_zoom_enabled ||
        data->map_zoom < data->map_min_zoom || data->map_zoom > data->map_max_zoom)
    {
        return 0;
    }

    next_zoom = data->map_zoom + delta;
    if (next_zoom < data->map_min_zoom)
    {
        next_zoom = data->map_min_zoom;
    }
    if (next_zoom > data->map_max_zoom)
    {
        next_zoom = data->map_max_zoom;
    }
    if (next_zoom == data->map_zoom)
    {
        return 0;
    }

    data->map_zoom = next_zoom;
    data->map_zoom_change_pending = 1;
    cabin_data_build_map_cache_path(data);
    cabin_data_request_map_refresh(data);
    return 1;
}

void cabin_data_commit_map_view(Cabin_Data *data, int uses_web_mercator)
{
    if (data == NULL)
    {
        return;
    }

    data->map_loaded_zoom = data->map_zoom;
    data->map_uses_web_mercator = uses_web_mercator ? 1 : 0;
    data->map_display_top_left_lat = data->map_top_left_lat;
    data->map_display_top_left_lon = data->map_top_left_lon;
    data->map_display_bottom_right_lat = data->map_bottom_right_lat;
    data->map_display_bottom_right_lon = data->map_bottom_right_lon;
    data->map_display_center_lat = data->map_center_lat;
    data->map_display_center_lon = data->map_center_lon;
    data->map_zoom_change_pending = 0;
}

void cabin_data_revert_requested_map_zoom(Cabin_Data *data)
{
    if (data == NULL || data->map_loaded_zoom < data->map_min_zoom || data->map_loaded_zoom > data->map_max_zoom)
    {
        return;
    }

    data->map_zoom = data->map_loaded_zoom;
    data->map_zoom_change_pending = 0;
    data->map_refresh_requested = 0;
    cabin_data_build_map_cache_path(data);
}

static double cabin_data_route_distance(const Cabin_Trajectory_Point *a, const Cabin_Trajectory_Point *b)
{
    if (a == NULL || b == NULL)
    {
        return 0.0;
    }

    const double lat1 = a->latitude * CABIN_PI / 180.0;
    const double lat2 = b->latitude * CABIN_PI / 180.0;
    const double dlat = lat2 - lat1;
    const double dlon = (b->longitude - a->longitude) * CABIN_PI / 180.0;
    const double avg_lat = (lat1 + lat2) * 0.5;
    const double x = dlon * cos(avg_lat);
    return sqrt(x * x + dlat * dlat);
}

static float cabin_data_progress_for_position(const Cabin_Data *data, double latitude, double longitude)
{
    if (data == NULL || data->planned_route_count < 2 || !cabin_data_valid_geo(latitude, longitude))
    {
        return data != NULL ? data->progress : 0.0f;
    }

    double total_distance = 0.0;
    for (int i = 1; i < data->planned_route_count; ++i)
    {
        total_distance += cabin_data_route_distance(&data->planned_route[i - 1], &data->planned_route[i]);
    }

    if (total_distance <= 0.0)
    {
        return data->progress;
    }

    double best_distance_sq = 1.0e30;
    double best_route_distance = 0.0;
    double accumulated = 0.0;
    for (int i = 1; i < data->planned_route_count; ++i)
    {
        const Cabin_Trajectory_Point *from = &data->planned_route[i - 1];
        const Cabin_Trajectory_Point *to = &data->planned_route[i];
        const double avg_lat_rad = (from->latitude + to->latitude) * 0.5 * CABIN_PI / 180.0;
        const double lon_scale = cos(avg_lat_rad);
        const double x1 = from->longitude * lon_scale;
        const double y1 = from->latitude;
        const double x2 = to->longitude * lon_scale;
        const double y2 = to->latitude;
        const double xp = longitude * lon_scale;
        const double yp = latitude;
        const double dx = x2 - x1;
        const double dy = y2 - y1;
        const double len_sq = dx * dx + dy * dy;
        double local_t = 0.0;
        if (len_sq > 0.0)
        {
            local_t = ((xp - x1) * dx + (yp - y1) * dy) / len_sq;
            if (local_t < 0.0)
            {
                local_t = 0.0;
            }
            else if (local_t > 1.0)
            {
                local_t = 1.0;
            }
        }

        const double nearest_x = x1 + dx * local_t;
        const double nearest_y = y1 + dy * local_t;
        const double dist_x = xp - nearest_x;
        const double dist_y = yp - nearest_y;
        const double distance_sq = dist_x * dist_x + dist_y * dist_y;
        const double segment_distance = cabin_data_route_distance(from, to);
        if (distance_sq < best_distance_sq)
        {
            best_distance_sq = distance_sq;
            best_route_distance = accumulated + segment_distance * local_t;
        }
        accumulated += segment_distance;
    }

    return clamp_float((float)(best_route_distance / total_distance), 0.0f, 1.0f);
}

static void cabin_data_fit_map_bounds_to_route(Cabin_Data *data)
{
    Cabin_Route_Bounds bounds;

    if (data == NULL || !cabin_data_compute_route_bounds(data, &bounds))
    {
        return;
    }

    double lat_span = bounds.max_lat - bounds.min_lat;
    double lon_span = bounds.max_lon - bounds.min_lon;

    if (lat_span < CABIN_MAP_MIN_LAT_SPAN)
    {
        lat_span = CABIN_MAP_MIN_LAT_SPAN;
    }
    if (lon_span < CABIN_MAP_MIN_LON_SPAN)
    {
        lon_span = CABIN_MAP_MIN_LON_SPAN;
    }

    double lat_margin = lat_span * CABIN_MAP_ROUTE_MARGIN_RATIO;
    double lon_margin = lon_span * CABIN_MAP_ROUTE_MARGIN_RATIO;
    if (lat_margin < 0.08)
    {
        lat_margin = 0.08;
    }
    if (lon_margin < 0.08)
    {
        lon_margin = 0.08;
    }

    data->map_center_lat = (bounds.min_lat + bounds.max_lat) * 0.5;
    data->map_center_lon = (bounds.min_lon + bounds.max_lon) * 0.5;
    data->map_zoom = cabin_data_select_map_zoom(bounds.max_lat - bounds.min_lat, bounds.max_lon - bounds.min_lon);
    data->map_top_left_lat = bounds.max_lat + lat_margin;
    data->map_bottom_right_lat = bounds.min_lat - lat_margin;
    data->map_top_left_lon = bounds.min_lon - lon_margin;
    data->map_bottom_right_lon = bounds.max_lon + lon_margin;
    cabin_data_build_map_cache_path(data);
    cabin_data_request_map_refresh(data);
}

static void cabin_data_fit_map_bounds_to_position(Cabin_Data *data)
{
    const double latitude_span = 0.36;
    const double longitude_span = 0.48;

    if (data == NULL || !cabin_data_valid_geo(data->latitude, data->longitude))
    {
        return;
    }

    if (data->map_zoom >= data->map_min_zoom && data->map_zoom <= data->map_max_zoom &&
        fabs(data->latitude - data->map_center_lat) < CABIN_MAP_RECENTER_LAT_THRESHOLD &&
        fabs(data->longitude - data->map_center_lon) < CABIN_MAP_RECENTER_LON_THRESHOLD)
    {
        return;
    }

    data->map_center_lat = data->latitude;
    data->map_center_lon = data->longitude;
    data->map_top_left_lat = data->latitude + latitude_span * 0.5;
    data->map_bottom_right_lat = data->latitude - latitude_span * 0.5;
    data->map_top_left_lon = data->longitude - longitude_span * 0.5;
    data->map_bottom_right_lon = data->longitude + longitude_span * 0.5;
    if (data->map_zoom < data->map_min_zoom || data->map_zoom > data->map_max_zoom)
    {
        data->map_zoom = CABIN_MAP_DEFAULT_POSITION_ZOOM;
    }
    cabin_data_build_map_cache_path(data);
    cabin_data_request_map_refresh(data);
}


static void cabin_data_push_flown_track_point(Cabin_Data *data, double latitude, double longitude)
{
    if (data == NULL || !cabin_data_valid_geo(latitude, longitude))
    {
        return;
    }

    if (data->flown_track_seed_is_default && !data->flown_track_has_real_point)
    {
        Cabin_Trajectory_Point *point = &data->flown_track[0];
        memset(point, 0, sizeof(*point));
        point->latitude = latitude;
        point->longitude = longitude;
        point->sequence = 0;
        point->altitude = data->altitude;
        point->ground_speed = data->ground_speed;
        data->flown_track_count = 1;
        data->flown_track_next_sequence = 1;
        data->flown_track_last_progress = data->progress;
        data->flown_track_time_since_append = 0.0f;
        data->flown_track_seed_is_default = 0;
        data->flown_track_has_real_point = 1;
        return;
    }

    if (data->flown_track_count > 0)
    {
        const Cabin_Trajectory_Point *last = &data->flown_track[data->flown_track_count - 1];
        if (fabs(last->latitude - latitude) < CABIN_TRAJECTORY_DUPLICATE_EPSILON &&
            fabs(last->longitude - longitude) < CABIN_TRAJECTORY_DUPLICATE_EPSILON)
        {
            return;
        }
    }

    if (data->flown_track_count >= CABIN_FLOWN_TRACK_MAX_POINTS)
    {
        memmove(&data->flown_track[0],
                &data->flown_track[1],
                sizeof(data->flown_track[0]) * (CABIN_FLOWN_TRACK_MAX_POINTS - 1));
        data->flown_track_count = CABIN_FLOWN_TRACK_MAX_POINTS - 1;
    }

    Cabin_Trajectory_Point *point = &data->flown_track[data->flown_track_count++];
    point->ident[0] = '\0';
    point->latitude = latitude;
    point->longitude = longitude;
    point->sequence = data->flown_track_next_sequence++;
    point->altitude = data->altitude;
    point->ground_speed = data->ground_speed;
    data->flown_track_has_real_point = 1;
    data->flown_track_last_progress = data->progress;
    data->flown_track_time_since_append = 0.0f;
}


static void cabin_data_update_flown_track(Cabin_Data *data, float delta_time, int force_append)
{
    if (data == NULL)
    {
        return;
    }

    data->flown_track_time_since_append += delta_time;

    const float progress_delta = fabsf(data->progress - data->flown_track_last_progress);
    if (force_append ||
        progress_delta >= CABIN_TRAJECTORY_PROGRESS_THRESHOLD ||
        data->flown_track_time_since_append >= CABIN_TRAJECTORY_UPDATE_INTERVAL)
    {
        cabin_data_push_flown_track_point(data, data->current_lat, data->current_lon);
    }
}

void cabin_data_init(Cabin_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    memset(data, 0, sizeof(*data));

    /* Cabin starts in a safe state until the read-only SimDataCenter view is applied. */
    copy_text(data->flight_no, sizeof(data->flight_no), "----");
    copy_text(data->origin_city, sizeof(data->origin_city), "----");
    copy_text(data->origin_airport, sizeof(data->origin_airport), "----");
    copy_text(data->destination_city, sizeof(data->destination_city), "----");
    copy_text(data->destination_airport, sizeof(data->destination_airport), "----");
    copy_text(data->current_city, sizeof(data->current_city), "DATA UNAVAILABLE");
    copy_text(data->current_district, sizeof(data->current_district), "----");
    copy_text(data->current_town, sizeof(data->current_town), "----");
    copy_text(data->data_source, sizeof(data->data_source), "NONE");
    copy_text(data->flight_phase, sizeof(data->flight_phase), "UNKNOWN");
    copy_text(data->planned_route_source, sizeof(data->planned_route_source), "NONE");
    copy_text(data->weather, sizeof(data->weather), "--");
    copy_text(data->weather_city, sizeof(data->weather_city), "--");
    copy_text(data->wind_direction, sizeof(data->wind_direction), "--");
    copy_text(data->wind_power, sizeof(data->wind_power), "--");
    copy_text(data->weather_source, sizeof(data->weather_source), "NONE");
    copy_text(data->map_source, sizeof(data->map_source), "LOCAL");
    data->map_min_zoom = CABIN_MAP_MIN_ZOOM;
    data->map_max_zoom = CABIN_MAP_MAX_ZOOM;
    data->map_api_zoom_enabled = 1;
    {
        const char *local_zoom_only = getenv("CABIN_MAP_LOCAL_ZOOM_ONLY");
        if (local_zoom_only != NULL && strcmp(local_zoom_only, "1") == 0)
        {
            data->map_api_zoom_enabled = 0;
        }
    }
    data->snapshot_frame = -1;
    data->frame_id = -1;
    data->timestamp = 0.0f;
    data->snapshot_source = SIM_SNAPSHOT_SOURCE_NONE;
    data->fallback_active = 0;
    data->xplane_connected = 0;
    data->timed_out = 0;
    data->last_valid_xplane_timestamp = 0.0f;
    data->route_revision = -1;
    data->active_waypoint_index = -1;
    data->flown_track_count = 1;
    data->flown_track[0].latitude = 36.07;
    data->flown_track[0].longitude = 120.38;
    data->flown_track[0].sequence = 0;
    data->flown_track_next_sequence = 1;
    data->flown_track_seed_is_default = 1;
    data->flown_track_has_real_point = 0;
    return;

}

static float cabin_data_distance_nm(double latitude_a, double longitude_a, double latitude_b, double longitude_b)
{
    const double latitude_a_rad = latitude_a * CABIN_PI / 180.0;
    const double latitude_b_rad = latitude_b * CABIN_PI / 180.0;
    const double delta_latitude = latitude_b_rad - latitude_a_rad;
    const double delta_longitude = (longitude_b - longitude_a) * CABIN_PI / 180.0;
    const double a = sin(delta_latitude * 0.5) * sin(delta_latitude * 0.5) +
                     cos(latitude_a_rad) * cos(latitude_b_rad) *
                         sin(delta_longitude * 0.5) * sin(delta_longitude * 0.5);
    return (float)(3440.065 * 2.0 * atan2(sqrt(a), sqrt(1.0 - a)));
}

static void cabin_data_clear_route_view(Cabin_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->route_valid = 0;
    data->route_point_count = 0;
    data->planned_route_count = 0;
    data->planned_route_from_fmc = 0;
    data->active_waypoint_index = -1;
    data->active_waypoint[0] = '\0';
    data->distance_to_active_nm = 0.0f;
    data->distance_to_destination_nm = 0.0f;
    data->origin_lat = 0.0;
    data->origin_lon = 0.0;
    data->destination_lat = 0.0;
    data->destination_lon = 0.0;
    data->map_top_left_lat = 0.0;
    data->map_top_left_lon = 0.0;
    data->map_bottom_right_lat = 0.0;
    data->map_bottom_right_lon = 0.0;
    data->map_center_lat = 0.0;
    data->map_center_lon = 0.0;
    data->map_zoom = 0;
    copy_text(data->origin_city, sizeof(data->origin_city), "----");
    copy_text(data->origin_airport, sizeof(data->origin_airport), "----");
    copy_text(data->destination_city, sizeof(data->destination_city), "----");
    copy_text(data->destination_airport, sizeof(data->destination_airport), "----");
    copy_text(data->planned_route_source, sizeof(data->planned_route_source), "NONE");
    memset(data->planned_route, 0, sizeof(data->planned_route));
    memset(&data->origin_place, 0, sizeof(data->origin_place));
    memset(&data->destination_place, 0, sizeof(data->destination_place));
}

static const SimRoutePoint *cabin_data_find_route_endpoint(const SimPlannedRoute *route, const char *ident)
{
    if (route == NULL || ident == NULL || ident[0] == '\0')
    {
        return NULL;
    }

    for (int i = 0; i < route->point_count; ++i)
    {
        const SimRoutePoint *point = &route->points[i];
        if (point->has_position && cabin_data_valid_geo(point->latitude, point->longitude) &&
            strcmp(point->ident, ident) == 0 && strcmp(point->type, "AIRPORT") == 0)
        {
            return point;
        }
    }
    for (int i = 0; i < route->point_count; ++i)
    {
        const SimRoutePoint *point = &route->points[i];
        if (point->has_position && cabin_data_valid_geo(point->latitude, point->longitude) && strcmp(point->ident, ident) == 0)
        {
            return point;
        }
    }
    return NULL;
}

static void cabin_data_apply_route_view(Cabin_Data *data, const SimPlannedRoute *route, int route_revision)
{
    int copied_points = 0;
    const SimRoutePoint *origin_point;
    const SimRoutePoint *destination_point;

    cabin_data_clear_route_view(data);
    data->route_revision = route_revision;
    if (route == NULL || !route->valid)
    {
        return;
    }

    data->route_valid = 1;
    data->route_point_count = route->point_count;
    data->active_waypoint_index = route->active_waypoint_index;
    data->planned_route_from_fmc = 1;
    copy_text(data->origin_city, sizeof(data->origin_city), route->origin);
    copy_text(data->origin_airport, sizeof(data->origin_airport), route->origin);
    copy_text(data->destination_city, sizeof(data->destination_city), route->destination);
    copy_text(data->destination_airport, sizeof(data->destination_airport), route->destination);
    copy_text(data->planned_route_source, sizeof(data->planned_route_source), sim_data_center_route_source_name(route->source));
    origin_point = cabin_data_find_route_endpoint(route, route->origin);
    destination_point = cabin_data_find_route_endpoint(route, route->destination);

    for (int i = 0; i < route->point_count && copied_points < CABIN_PLANNED_ROUTE_MAX_POINTS; ++i)
    {
        const SimRoutePoint *source_point = &route->points[i];
        Cabin_Trajectory_Point *target_point;
        if (!source_point->has_position || !cabin_data_valid_geo(source_point->latitude, source_point->longitude))
        {
            continue;
        }

        target_point = &data->planned_route[copied_points++];
        copy_text(target_point->ident, sizeof(target_point->ident), source_point->ident);
        target_point->latitude = source_point->latitude;
        target_point->longitude = source_point->longitude;
        target_point->altitude = (float)source_point->altitude;
        target_point->sequence = (unsigned int)i;
    }
    data->planned_route_count = copied_points;

    if (route->active_waypoint_index >= 0 && route->active_waypoint_index < route->point_count)
    {
        copy_text(data->active_waypoint, sizeof(data->active_waypoint), route->points[route->active_waypoint_index].ident);
    }
    if (origin_point != NULL)
    {
        data->origin_lat = origin_point->latitude;
        data->origin_lon = origin_point->longitude;
    }
    if (destination_point != NULL)
    {
        data->destination_lat = destination_point->latitude;
        data->destination_lon = destination_point->longitude;
    }
    if (copied_points > 0)
    {
        cabin_data_fit_map_bounds_to_route(data);
    }
    printf("Cabin Route: revision=%d origin=%s lat=%.6f lon=%.6f endpoint=%s; destination=%s lat=%.6f lon=%.6f endpoint=%s.\n",
           route_revision, route->origin, data->origin_lat, data->origin_lon,
           origin_point != NULL ? "MATCHED" : "MISSING",
           route->destination, data->destination_lat, data->destination_lon,
           destination_point != NULL ? "MATCHED" : "MISSING");
}

int cabin_data_apply_sim_data_center(Cabin_Data *data, const struct SimDataCenter *center, float delta_time)
{
    const SimSnapshot *snapshot = sim_data_center_snapshot(center);
    const SimPlannedRoute *route = sim_data_center_route(center);
    const AlertSnapshot *alerts = sim_data_center_alerts(center);
    const int route_revision = sim_data_center_route_revision(center);
    const int previous_valid = data != NULL ? data->snapshot_valid : 0;
    const int previous_route_revision = data != NULL ? data->route_revision : -1;
    char previous_source[CABIN_TEXT_LEN];
    int changes = CABIN_DATA_UPDATE_NONE;

    if (data == NULL)
    {
        return CABIN_DATA_UPDATE_NONE;
    }

    copy_text(previous_source, sizeof(previous_source), data->data_source);
    if (alerts != NULL)
    {
        data->alerts = *alerts;
        const AlertState *crash = alert_snapshot_find(alerts, ALERT_TYPE_CRASH);
        data->crash_demo_active = crash != NULL && crash->active;
        data->crash_demo_started_ticks = crash != NULL ? (unsigned int)(crash->start_time * 1000.0f) : 0u;
    }
    else
    {
        memset(&data->alerts, 0, sizeof(data->alerts));
        data->crash_demo_active = 0;
        data->crash_demo_started_ticks = 0;
    }
    if (previous_route_revision != route_revision || (route == NULL) != !data->route_valid)
    {
        cabin_data_apply_route_view(data, route, route_revision);
        changes |= CABIN_DATA_UPDATE_ROUTE;
    }

    data->snapshot_valid = snapshot != NULL && snapshot->data_valid;
    data->frame_id = snapshot != NULL ? snapshot->frame_id : -1;
    data->timestamp = snapshot != NULL ? snapshot->timestamp : 0.0f;
    data->snapshot_source = snapshot != NULL ? snapshot->source : SIM_SNAPSHOT_SOURCE_NONE;
    data->fallback_active = snapshot != NULL ? snapshot->fallback_active : 0;
    data->xplane_connected = snapshot != NULL ? snapshot->xplane_connected : 0;
    data->timed_out = snapshot != NULL ? snapshot->timed_out : 0;
    data->last_valid_xplane_timestamp = snapshot != NULL ? snapshot->last_valid_xplane_timestamp : 0.0f;
    copy_text(data->data_source, sizeof(data->data_source),
              snapshot != NULL ? sim_snapshot_source_name(snapshot->source) : "NONE");
    copy_text(data->flight_phase, sizeof(data->flight_phase),
              snapshot != NULL ? sim_flight_phase_name(snapshot->flight_phase) : "UNKNOWN");
    data->snapshot_frame = data->frame_id;
    data->snapshot_time = snapshot != NULL ? snapshot->sim_time : 0.0f;

    if (!data->snapshot_valid)
    {
        data->using_sim_data = 0;
        data->altitude = 0.0f;
        data->ground_speed = 0.0f;
        data->true_air_speed = 0.0f;
        data->vertical_speed = 0.0f;
        data->heading = 0.0f;
        data->track = 0.0f;
        data->has_heading = 0;
        data->latitude = 0.0;
        data->longitude = 0.0;
        data->current_lat = 0.0;
        data->current_lon = 0.0;
        data->progress = 0.0f;
        data->remaining_time_min = 0.0f;
        copy_text(data->current_city, sizeof(data->current_city), "DATA UNAVAILABLE");
    }
    else
    {
        data->using_sim_data = 1;
        data->altitude = snapshot->altitude;
        data->ground_speed = snapshot->ground_speed;
        data->true_air_speed = snapshot->true_air_speed;
        data->vertical_speed = snapshot->vertical_speed;
        data->heading = snapshot->heading;
        data->track = snapshot->track;
        data->has_heading = 1;
        data->latitude = snapshot->latitude;
        data->longitude = snapshot->longitude;
        data->current_lat = snapshot->latitude;
        data->current_lon = snapshot->longitude;
        data->engine_left_running = snapshot->engine_left_running;
        data->engine_right_running = snapshot->engine_right_running;
        data->progress = cabin_data_progress_for_position(data, data->latitude, data->longitude);
        data->distance_to_destination_nm = data->planned_route_count > 0
                                               ? cabin_data_distance_nm(data->latitude, data->longitude, data->destination_lat, data->destination_lon)
                                               : 0.0f;
        data->distance_to_active_nm = data->active_waypoint[0] != '\0' && route != NULL &&
                                              route->active_waypoint_index >= 0 && route->active_waypoint_index < route->point_count &&
                                              route->points[route->active_waypoint_index].has_position
                                          ? cabin_data_distance_nm(data->latitude, data->longitude,
                                                                   route->points[route->active_waypoint_index].latitude,
                                                                   route->points[route->active_waypoint_index].longitude)
                                          : 0.0f;
        data->remaining_time_min = data->ground_speed > 1.0f ? data->distance_to_destination_nm * 60.0f / data->ground_speed : 0.0f;
        if (!data->route_valid)
        {
            cabin_data_fit_map_bounds_to_position(data);
        }
        if (snapshot->source == SIM_SNAPSHOT_SOURCE_XPLANE &&
            snapshot->xplane_connected && !snapshot->timed_out &&
            cabin_data_valid_geo(snapshot->latitude, snapshot->longitude))
        {
            cabin_data_update_flown_track(data, delta_time > 0.0f ? delta_time : 0.0f, 0);
        }
        if (data->current_place.status == CABIN_PLACE_VALID)
        {
            copy_text(data->current_city, sizeof(data->current_city), data->current_place.city);
            copy_text(data->current_district, sizeof(data->current_district), data->current_place.district);
            copy_text(data->current_town, sizeof(data->current_town), cabin_place_street_or_town(&data->current_place));
        }
        else
        {
            copy_text(data->current_city, sizeof(data->current_city), data->route_valid ? "EN ROUTE" : "POSITION AVAILABLE");
            copy_text(data->current_district, sizeof(data->current_district), data->flight_phase);
            copy_text(data->current_town, sizeof(data->current_town), data->active_waypoint[0] != '\0' ? data->active_waypoint : "----");
        }
    }

    if (previous_valid != data->snapshot_valid)
    {
        changes |= CABIN_DATA_UPDATE_VALIDITY;
    }
    if (changes != CABIN_DATA_UPDATE_NONE || strcmp(previous_source, data->data_source) != 0)
    {
        printf("Cabin Data: source=%s valid=%d frame=%d alt=%.0f gs=%.0f tas=%.0f vs=%.0f hdg=%.0f origin=%s destination=%s route_rev=%d phase=%s.\n",
               data->data_source, data->snapshot_valid, data->snapshot_frame, data->altitude, data->ground_speed,
               data->true_air_speed, data->vertical_speed, data->heading, data->origin_airport,
               data->destination_airport, data->route_revision, data->flight_phase);
        fflush(stdout);
    }
    return changes;
}
