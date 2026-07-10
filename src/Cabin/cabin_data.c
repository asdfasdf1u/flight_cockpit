#include "cabin_data.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CABIN_ROUTE_TOTAL_TIME_MIN 165.0f
#define CABIN_ROUTE_PROGRESS_RATE 0.012f
#define CABIN_TRAJECTORY_PROGRESS_THRESHOLD 0.006f
#define CABIN_TRAJECTORY_UPDATE_INTERVAL 1.2f
#define CABIN_TRAJECTORY_DUPLICATE_EPSILON 0.000001
#define CABIN_PI 3.14159265358979323846

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

static void cabin_data_set_route_point(Cabin_Data *data, int index, double latitude, double longitude)
{
    if (data == NULL || index < 0 || index >= CABIN_PLANNED_ROUTE_MAX_POINTS)
    {
        return;
    }

    Cabin_Trajectory_Point *point = &data->planned_route[index];
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
    cabin_data_set_route_point(data, 0, 40.080111, 116.584556); /* Beijing Capital */
    cabin_data_set_route_point(data, 1, 38.042800, 114.514900); /* Shijiazhuang */
    cabin_data_set_route_point(data, 2, 37.870600, 112.548900); /* Taiyuan */
    cabin_data_set_route_point(data, 3, 34.341600, 108.939800); /* Xi'an */
    cabin_data_set_route_point(data, 4, 33.067600, 107.023300); /* Hanzhong */
    cabin_data_set_route_point(data, 5, 31.467500, 104.679600); /* Mianyang */
    cabin_data_set_route_point(data, 6, 30.312520, 104.441284); /* Chengdu Tianfu */

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
    data->map_top_left_lat = 44.863010;
    data->map_top_left_lon = 88.010000;
    data->map_bottom_right_lat = 24.236116;
    data->map_bottom_right_lon = 133.010000;
    data->altitude = 9200.0f;
    data->ground_speed = 820.0f;
    data->progress = 0.16f;
    cabin_data_update_progress_fields(data);
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
}
