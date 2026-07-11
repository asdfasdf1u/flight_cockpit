#include "cabin_data.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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

static double lerp_double(double start, double end, float t)
{
    return start + (end - start) * (double)t;
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
    char route_name[96];

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
    else
    {
        copy_text(route_name, sizeof(route_name), "beijing_chengdu");
    }

    if (route_name[0] == '\0')
    {
        copy_text(route_name, sizeof(route_name), "ROUTE");
    }

    snprintf(data->map_cache_path,
             sizeof(data->map_cache_path),
             "%s/cabin_map_%s.png",
             CABIN_MAP_CACHE_DIR,
             route_name);
}

static int cabin_data_point_in_map_bounds(const Cabin_Data *data, double latitude, double longitude)
{
    return data != NULL &&
           cabin_data_valid_geo(latitude, longitude) &&
           data->map_top_left_lat > data->map_bottom_right_lat &&
           data->map_bottom_right_lon > data->map_top_left_lon &&
           latitude <= data->map_top_left_lat &&
           latitude >= data->map_bottom_right_lat &&
           longitude >= data->map_top_left_lon &&
           longitude <= data->map_bottom_right_lon;
}

static void cabin_data_set_route_point(Cabin_Data *data, int index, const char *ident, double latitude, double longitude)
{
    if (data == NULL || index < 0 || index >= CABIN_PLANNED_ROUTE_MAX_POINTS)
    {
        return;
    }

    Cabin_Trajectory_Point *point = &data->planned_route[index];
    copy_text(point->ident, sizeof(point->ident), ident);
    point->latitude = latitude;
    point->longitude = longitude;
    point->sequence = (unsigned int)index;
    point->altitude = 0.0f;
    point->ground_speed = 0.0f;
}

static void cabin_data_init_planned_route(Cabin_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->planned_route_count = 7;
    cabin_data_set_route_point(data, 0, "ZBAA", 40.080111, 116.584556); /* Beijing Capital */
    cabin_data_set_route_point(data, 1, "SJW", 38.042800, 114.514900);  /* Shijiazhuang */
    cabin_data_set_route_point(data, 2, "TYN", 37.870600, 112.548900);  /* Taiyuan */
    cabin_data_set_route_point(data, 3, "XIY", 34.341600, 108.939800);  /* Xi'an */
    cabin_data_set_route_point(data, 4, "HZG", 33.067600, 107.023300);  /* Hanzhong */
    cabin_data_set_route_point(data, 5, "MIG", 31.467500, 104.679600);  /* Mianyang */
    cabin_data_set_route_point(data, 6, "ZUTF", 30.312520, 104.441284); /* Chengdu Tianfu */

    data->origin_lat = data->planned_route[0].latitude;
    data->origin_lon = data->planned_route[0].longitude;
    data->destination_lat = data->planned_route[data->planned_route_count - 1].latitude;
    data->destination_lon = data->planned_route[data->planned_route_count - 1].longitude;
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
}

static void cabin_data_interpolate_planned_route(const Cabin_Data *data, float progress, double *latitude, double *longitude)
{
    if (latitude == NULL || longitude == NULL)
    {
        return;
    }

    if (data == NULL || data->planned_route_count < 2)
    {
        *latitude = data != NULL ? lerp_double(data->origin_lat, data->destination_lat, progress) : 0.0;
        *longitude = data != NULL ? lerp_double(data->origin_lon, data->destination_lon, progress) : 0.0;
        return;
    }

    progress = clamp_float(progress, 0.0f, 1.0f);
    if (progress <= 0.0f)
    {
        *latitude = data->planned_route[0].latitude;
        *longitude = data->planned_route[0].longitude;
        return;
    }
    if (progress >= 1.0f)
    {
        const Cabin_Trajectory_Point *last = &data->planned_route[data->planned_route_count - 1];
        *latitude = last->latitude;
        *longitude = last->longitude;
        return;
    }

    double total_distance = 0.0;
    for (int i = 1; i < data->planned_route_count; ++i)
    {
        total_distance += cabin_data_route_distance(&data->planned_route[i - 1], &data->planned_route[i]);
    }

    if (total_distance <= 0.0)
    {
        const double segment_pos = (double)progress * (double)(data->planned_route_count - 1);
        int segment = (int)floor(segment_pos);
        if (segment >= data->planned_route_count - 1)
        {
            segment = data->planned_route_count - 2;
        }
        const float local_t = (float)(segment_pos - (double)segment);
        *latitude = lerp_double(data->planned_route[segment].latitude, data->planned_route[segment + 1].latitude, local_t);
        *longitude = lerp_double(data->planned_route[segment].longitude, data->planned_route[segment + 1].longitude, local_t);
        return;
    }

    const double target_distance = total_distance * (double)progress;
    double accumulated = 0.0;
    for (int i = 1; i < data->planned_route_count; ++i)
    {
        const Cabin_Trajectory_Point *from = &data->planned_route[i - 1];
        const Cabin_Trajectory_Point *to = &data->planned_route[i];
        const double segment_distance = cabin_data_route_distance(from, to);
        if (accumulated + segment_distance >= target_distance || i == data->planned_route_count - 1)
        {
            const float local_t = segment_distance > 0.0
                                      ? (float)((target_distance - accumulated) / segment_distance)
                                      : 0.0f;
            *latitude = lerp_double(from->latitude, to->latitude, clamp_float(local_t, 0.0f, 1.0f));
            *longitude = lerp_double(from->longitude, to->longitude, clamp_float(local_t, 0.0f, 1.0f));
            return;
        }
        accumulated += segment_distance;
    }

    const Cabin_Trajectory_Point *last = &data->planned_route[data->planned_route_count - 1];
    *latitude = last->latitude;
    *longitude = last->longitude;
}

static void cabin_data_update_current_position(Cabin_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    cabin_data_interpolate_planned_route(data, data->progress, &data->current_lat, &data->current_lon);

    data->latitude = data->current_lat;
    data->longitude = data->current_lon;
}

static void cabin_data_update_progress_fields(Cabin_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->progress = clamp_float(data->progress, 0.0f, 1.0f);
    cabin_data_update_current_position(data);
    data->altitude = 9200.0f + 300.0f * sinf(data->progress * 6.2831853f);
    data->ground_speed = 820.0f + 20.0f * sinf(data->progress * 12.5663706f);
    data->remaining_time_min = (1.0f - data->progress) * CABIN_ROUTE_TOTAL_TIME_MIN;
}

static void cabin_data_update_location_labels(Cabin_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    if (data->planned_route_from_fmc)
    {
        if (data->progress < 0.08f)
        {
            copy_text(data->current_city, sizeof(data->current_city), data->origin_airport);
            copy_text(data->current_district, sizeof(data->current_district), "ORIGIN");
            copy_text(data->current_town, sizeof(data->current_town), data->origin_airport);
        }
        else if (data->progress > 0.92f)
        {
            copy_text(data->current_city, sizeof(data->current_city), data->destination_airport);
            copy_text(data->current_district, sizeof(data->current_district), "DEST");
            copy_text(data->current_town, sizeof(data->current_town), data->destination_airport);
        }
        else
        {
            copy_text(data->current_city, sizeof(data->current_city), "ENROUTE");
            copy_text(data->current_district, sizeof(data->current_district), "FMC ROUTE");
            copy_text(data->current_town, sizeof(data->current_town), "SIM POSITION");
        }
        return;
    }

    if (data->progress < 0.30f)
    {
        copy_text(data->current_city, sizeof(data->current_city), "BEIJING");
        copy_text(data->current_district, sizeof(data->current_district), "SHUNYI");
        copy_text(data->current_town, sizeof(data->current_town), "BEIJING CAPITAL");
    }
    else if (data->progress < 0.72f)
    {
        copy_text(data->current_city, sizeof(data->current_city), "椋炶閫斾腑");
        copy_text(data->current_district, sizeof(data->current_district), "鏈煡鍖哄煙");
        copy_text(data->current_town, sizeof(data->current_town), "宸¤埅鑸");
    }
    else
    {
        copy_text(data->current_city, sizeof(data->current_city), "CHENGDU");
        copy_text(data->current_district, sizeof(data->current_district), "JIAN YANG");
        copy_text(data->current_town, sizeof(data->current_town), "CHENGDU TIANFU");
    }
}

static void cabin_data_compact_flown_track(Cabin_Data *data)
{
    if (data == NULL || data->flown_track_count < CABIN_FLOWN_TRACK_MAX_POINTS)
    {
        return;
    }

    Cabin_Trajectory_Point compact[CABIN_FLOWN_TRACK_MAX_POINTS];
    int compact_count = 0;

    compact[compact_count++] = data->flown_track[0];
    for (int i = 1; i < data->flown_track_count - 1 && compact_count < CABIN_FLOWN_TRACK_MAX_POINTS - 1; i += 2)
    {
        compact[compact_count++] = data->flown_track[i];
    }
    if (data->flown_track_count > 1 && compact_count < CABIN_FLOWN_TRACK_MAX_POINTS)
    {
        compact[compact_count++] = data->flown_track[data->flown_track_count - 1];
    }

    memcpy(data->flown_track, compact, sizeof(compact[0]) * (size_t)compact_count);
    data->flown_track_count = compact_count;
}

static void cabin_data_push_flown_track_point(Cabin_Data *data, double latitude, double longitude)
{
    if (data == NULL || !cabin_data_valid_geo(latitude, longitude))
    {
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
        cabin_data_compact_flown_track(data);
        if (data->flown_track_count >= CABIN_FLOWN_TRACK_MAX_POINTS)
        {
            data->flown_track_count = CABIN_FLOWN_TRACK_MAX_POINTS - 1;
        }
    }

    Cabin_Trajectory_Point *point = &data->flown_track[data->flown_track_count++];
    point->ident[0] = '\0';
    point->latitude = latitude;
    point->longitude = longitude;
    point->sequence = data->flown_track_next_sequence++;
    point->altitude = data->altitude;
    point->ground_speed = data->ground_speed;
    data->flown_track_last_progress = data->progress;
    data->flown_track_time_since_append = 0.0f;
}

static void cabin_data_reset_flown_track(Cabin_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->flown_track_count = 0;
    data->flown_track_next_sequence = 0;
    data->flown_track_last_progress = data->progress;
    data->flown_track_time_since_append = 0.0f;
    cabin_data_push_flown_track_point(data, data->origin_lat, data->origin_lon);
}

static void cabin_data_update_flown_track(Cabin_Data *data, float delta_time, int force_append)
{
    if (data == NULL)
    {
        return;
    }

    data->flown_track_time_since_append += delta_time;

    if (data->flown_track_count <= 0)
    {
        cabin_data_push_flown_track_point(data, data->origin_lat, data->origin_lon);
        cabin_data_push_flown_track_point(data, data->current_lat, data->current_lon);
        return;
    }

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

    copy_text(data->flight_no, sizeof(data->flight_no), "CA1888");
    copy_text(data->origin_city, sizeof(data->origin_city), "北京首都");
    copy_text(data->origin_airport, sizeof(data->origin_airport), "北京首都");
    copy_text(data->destination_city, sizeof(data->destination_city), "成都");
    copy_text(data->destination_airport, sizeof(data->destination_airport), "成都天府");
    copy_text(data->current_city, sizeof(data->current_city), "北京市");
    copy_text(data->current_district, sizeof(data->current_district), "顺义区");
    copy_text(data->current_town, sizeof(data->current_town), "北京首都机场");

    cabin_data_init_planned_route(data);
    data->altitude = 9200.0f;
    data->ground_speed = 820.0f;
    data->heading = 0.0f;
    data->track = 0.0f;
    data->has_heading = 0;
    data->using_sim_data = 0;
    data->planned_route_from_fmc = 0;
    copy_text(data->planned_route_source, sizeof(data->planned_route_source), "MOCK");
    cabin_data_fit_map_bounds_to_route(data);
    data->progress = 0.16f;
    cabin_data_update_progress_fields(data);
    cabin_data_update_location_labels(data);
    cabin_data_reset_flown_track(data);
    cabin_data_update_flown_track(data, 0.0f, 1);

    printf("Cabin Route: CA1888 Beijing Capital International Airport -> Chengdu Tianfu International Airport.\n");
    printf("Cabin Route: map bounds configured for a Beijing-Chengdu wide-area map.\n");

    copy_text(data->weather, sizeof(data->weather), "晴");
    copy_text(data->weather_city, sizeof(data->weather_city), "北京");
    copy_text(data->weather_adcode, sizeof(data->weather_adcode), "110000");
    data->temperature = 18.0f;
    data->humidity = 57.0f;
    copy_text(data->wind_direction, sizeof(data->wind_direction), "西南");
    copy_text(data->wind_power, sizeof(data->wind_power), "3级");
    copy_text(data->weather_source, sizeof(data->weather_source), "MOCK");
    copy_text(data->weather_report_time, sizeof(data->weather_report_time), "--");
    copy_text(data->api_error_message, sizeof(data->api_error_message), "未请求 API");
    copy_text(data->map_source, sizeof(data->map_source), "LOCAL");
    copy_text(data->api_map_error_message, sizeof(data->api_map_error_message), "未请求静态地图");
}

void cabin_data_update_mock(Cabin_Data *data, float delta_time)
{
    if (data == NULL)
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

    const float previous_progress = data->progress;
    data->progress += delta_time * CABIN_ROUTE_PROGRESS_RATE;
    if (data->progress > 1.0f)
    {
        data->progress = 0.0f;
    }

    cabin_data_update_progress_fields(data);
    if (data->progress < previous_progress)
    {
        cabin_data_reset_flown_track(data);
        cabin_data_update_flown_track(data, 0.0f, 1);
    }
    else
    {
        cabin_data_update_flown_track(data, delta_time, 0);
    }

    cabin_data_update_location_labels(data);
    return;
}

#if 0
    if (data->progress < 0.30f)
    {
        copy_text(data->current_city, sizeof(data->current_city), "北京市");
        copy_text(data->current_district, sizeof(data->current_district), "顺义区");
        copy_text(data->current_town, sizeof(data->current_town), "北京首都机场");
    }
    else if (data->progress < 0.72f)
    {
        copy_text(data->current_city, sizeof(data->current_city), "飞行途中");
        copy_text(data->current_district, sizeof(data->current_district), "未知区域");
        copy_text(data->current_town, sizeof(data->current_town), "巡航航段");
    }
    else
    {
        copy_text(data->current_city, sizeof(data->current_city), "成都市");
        copy_text(data->current_district, sizeof(data->current_district), "简阳市");
        copy_text(data->current_town, sizeof(data->current_town), "成都天府机场");
    }
#endif

int cabin_data_apply_planned_route(Cabin_Data *data, const SimPlannedRoute *route)
{
    if (data == NULL || route == NULL || route->point_count < 2)
    {
        return 0;
    }

    int copied_count = 0;
    for (int i = 0; i < route->point_count && copied_count < CABIN_PLANNED_ROUTE_MAX_POINTS; ++i)
    {
        const SimRoutePoint *source = &route->points[i];
        if (!source->has_position || !cabin_data_valid_geo(source->latitude, source->longitude))
        {
            continue;
        }

        Cabin_Trajectory_Point *target = &data->planned_route[copied_count];
        copy_text(target->ident, sizeof(target->ident), source->ident);
        target->latitude = source->latitude;
        target->longitude = source->longitude;
        target->sequence = (unsigned int)copied_count;
        target->altitude = (float)source->altitude;
        target->ground_speed = 0.0f;
        copied_count++;
    }

    if (copied_count < 2)
    {
        printf("Cabin Route: FMC planned_route has no usable coordinates, keeping mock Beijing-Chengdu route.\n");
        fflush(stdout);
        return 0;
    }

    data->planned_route_count = copied_count;
    data->origin_lat = data->planned_route[0].latitude;
    data->origin_lon = data->planned_route[0].longitude;
    data->destination_lat = data->planned_route[copied_count - 1].latitude;
    data->destination_lon = data->planned_route[copied_count - 1].longitude;

    copy_text(data->origin_city, sizeof(data->origin_city), route->origin[0] != '\0' ? route->origin : data->planned_route[0].ident);
    copy_text(data->origin_airport, sizeof(data->origin_airport), route->origin[0] != '\0' ? route->origin : data->planned_route[0].ident);
    copy_text(data->destination_city, sizeof(data->destination_city), route->destination[0] != '\0' ? route->destination : data->planned_route[copied_count - 1].ident);
    copy_text(data->destination_airport, sizeof(data->destination_airport), route->destination[0] != '\0' ? route->destination : data->planned_route[copied_count - 1].ident);
    copy_text(data->planned_route_source, sizeof(data->planned_route_source), "FMC");
    data->planned_route_from_fmc = 1;

    data->progress = 0.0f;
    cabin_data_update_progress_fields(data);
    cabin_data_update_location_labels(data);
    cabin_data_fit_map_bounds_to_route(data);
    cabin_data_reset_flown_track(data);
    cabin_data_update_flown_track(data, 0.0f, 1);

    printf("Cabin Route: using FMC planned_route %s -> %s (%d coordinate points).\n",
           data->origin_airport,
           data->destination_airport,
           data->planned_route_count);
    fflush(stdout);
    return 1;
}

void cabin_data_apply_sim_snapshot(Cabin_Data *data, const SimSnapshot *snapshot, float delta_time)
{
    static int printed_out_of_bounds_position = 0;

    if (data == NULL || snapshot == NULL)
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

    const float previous_progress = data->progress;
    int use_snapshot_position = 0;
    if (snapshot->has_nd && cabin_data_valid_geo(snapshot->latitude, snapshot->longitude))
    {
        if (cabin_data_point_in_map_bounds(data, snapshot->latitude, snapshot->longitude))
        {
            use_snapshot_position = 1;
            data->current_lat = snapshot->latitude;
            data->current_lon = snapshot->longitude;
            data->latitude = snapshot->latitude;
            data->longitude = snapshot->longitude;
        }
        else
        {
            if (!printed_out_of_bounds_position)
            {
                printf("Cabin SimData: position lat=%.6f lon=%.6f is outside current route/map bounds; using route progress simulation for Cabin map.\n",
                       snapshot->latitude,
                       snapshot->longitude);
                fflush(stdout);
                printed_out_of_bounds_position = 1;
            }
            data->progress += delta_time * CABIN_ROUTE_PROGRESS_RATE;
            if (data->progress > 1.0f)
            {
                data->progress = 0.0f;
            }
        }
    }

    if (snapshot->has_pfd)
    {
        data->altitude = snapshot->altitude;
    }
    if (snapshot->has_nd)
    {
        data->ground_speed = snapshot->ground_speed;
        data->track = snapshot->track;
        data->heading = snapshot->heading;
        data->has_heading = 1;
    }
    else if (snapshot->has_pfd)
    {
        data->ground_speed = snapshot->airspeed;
        data->heading = snapshot->heading;
        data->track = snapshot->heading;
        data->has_heading = 1;
    }

    data->using_sim_data = 1;
    if (use_snapshot_position)
    {
        data->progress = cabin_data_progress_for_position(data, data->current_lat, data->current_lon);
        data->remaining_time_min = (1.0f - data->progress) * CABIN_ROUTE_TOTAL_TIME_MIN;
    }
    else
    {
        data->progress = clamp_float(data->progress, 0.0f, 1.0f);
        cabin_data_update_current_position(data);
        data->remaining_time_min = (1.0f - data->progress) * CABIN_ROUTE_TOTAL_TIME_MIN;
    }
    cabin_data_update_location_labels(data);

    if (previous_progress > 0.95f && data->progress < 0.05f)
    {
        cabin_data_reset_flown_track(data);
        cabin_data_update_flown_track(data, 0.0f, 1);
    }
    else
    {
        cabin_data_update_flown_track(data, delta_time, 0);
    }
}

int cabin_data_route_points_within_map_bounds(const Cabin_Data *data)
{
    if (data == NULL || data->planned_route_count <= 0)
    {
        return 0;
    }

    for (int i = 0; i < data->planned_route_count; ++i)
    {
        if (!cabin_data_point_in_map_bounds(data,
                                            data->planned_route[i].latitude,
                                            data->planned_route[i].longitude))
        {
            return 0;
        }
    }
    return 1;
}

void cabin_data_print_route_map_summary(const Cabin_Data *data)
{
    Cabin_Route_Bounds bounds;

    if (data == NULL)
    {
        return;
    }

    printf("Cabin Route/Map: route source=%s.\n",
           data->planned_route_from_fmc ? "FMC" : "MOCK");
    printf("Cabin Route/Map: origin=%s destination=%s point_count=%d.\n",
           data->origin_airport[0] != '\0' ? data->origin_airport : "----",
           data->destination_airport[0] != '\0' ? data->destination_airport : "----",
           data->planned_route_count);

    if (cabin_data_compute_route_bounds(data, &bounds))
    {
        printf("Cabin Route/Map: route bounds min_lat=%.6f max_lat=%.6f min_lon=%.6f max_lon=%.6f.\n",
               bounds.min_lat,
               bounds.max_lat,
               bounds.min_lon,
               bounds.max_lon);
    }
    else
    {
        printf("Cabin Route/Map: route bounds unavailable because no valid route coordinates exist.\n");
    }

    printf("Cabin Route/Map: calculated map center lat=%.6f lon=%.6f selected zoom=%d.\n",
           data->map_center_lat,
           data->map_center_lon,
           data->map_zoom);
    printf("Cabin Route/Map: map cache path=%s.\n",
           data->map_cache_path[0] != '\0' ? data->map_cache_path : "none");
    printf("Cabin Route/Map: map source=%s.\n",
           data->map_source[0] != '\0' ? data->map_source : "UNKNOWN");
    printf("Cabin Route/Map: map bounds top_left=(%.6f, %.6f) bottom_right=(%.6f, %.6f).\n",
           data->map_top_left_lat,
           data->map_top_left_lon,
           data->map_bottom_right_lat,
           data->map_bottom_right_lon);
    printf("Cabin Route/Map: all route points in map bounds=%s.\n",
           cabin_data_route_points_within_map_bounds(data) ? "yes" : "no");
    fflush(stdout);
}
