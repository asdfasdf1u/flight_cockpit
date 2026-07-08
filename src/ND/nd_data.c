#include "nd_data.h"

#include <math.h>
#include <stdio.h>

#define ND_DEG_TO_RAD 0.01745329251994329577f
#define ND_RAD_TO_DEG 57.295779513082320876f

static float normalize_degrees(float degrees)
{
    while (degrees >= 360.0f)
    {
        degrees -= 360.0f;
    }

    while (degrees < 0.0f)
    {
        degrees += 360.0f;
    }

    return degrees;
}

static void set_nav_point(
    ND_NavPoint *point,
    const char *ident,
    ND_PointType type,
    double latitude,
    double longitude,
    int active)
{
    if (point == NULL)
    {
        return;
    }

    snprintf(point->ident, sizeof(point->ident), "%s", ident);
    point->type = type;
    point->latitude = latitude;
    point->longitude = longitude;
    point->distance_nm = 0.0f;
    point->bearing_deg = 0.0f;
    point->visible = 1;
    point->active = active;
}

static void update_active_waypoint_info(ND_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->active_waypoint_name[0] = '\0';
    data->active_waypoint_distance_nm = 0.0f;
    data->active_waypoint_bearing_deg = 0.0f;
    data->active_waypoint_eta_min = 0.0f;

    if (data->active_point_index < 0 || data->active_point_index >= data->nav_point_count)
    {
        snprintf(data->active_waypoint_name, sizeof(data->active_waypoint_name), "----");
        return;
    }

    const ND_NavPoint *active = &data->nav_points[data->active_point_index];
    snprintf(data->active_waypoint_name, sizeof(data->active_waypoint_name), "%s", active->ident);
    data->active_waypoint_distance_nm = active->distance_nm;
    data->active_waypoint_bearing_deg = active->bearing_deg;

    if (data->ground_speed > 1.0f)
    {
        data->active_waypoint_eta_min = active->distance_nm / data->ground_speed * 60.0f;
    }
}

void nd_data_recalculate_nav_points(ND_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    const float aircraft_lat_rad = (float)data->latitude * ND_DEG_TO_RAD;
    const float lon_scale = cosf(aircraft_lat_rad);

    for (int i = 0; i < data->nav_point_count; ++i)
    {
        ND_NavPoint *point = &data->nav_points[i];
        const double delta_lat = point->latitude - data->latitude;
        const double delta_lon = point->longitude - data->longitude;
        const float north_nm = (float)(delta_lat * 60.0);
        const float east_nm = (float)(delta_lon * 60.0 * (double)lon_scale);
        float bearing = atan2f(east_nm, north_nm) * ND_RAD_TO_DEG;

        point->distance_nm = sqrtf(north_nm * north_nm + east_nm * east_nm);
        point->bearing_deg = normalize_degrees(bearing);
        point->active = i == data->active_point_index;
    }

    update_active_waypoint_info(data);
}

void nd_data_set_range(ND_Data *data, float range_nm)
{
    if (data == NULL)
    {
        return;
    }

    if (range_nm < 5.0f)
    {
        range_nm = 5.0f;
    }

    data->range_nm = range_nm;
}

void nd_data_init(ND_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->latitude = 39.904200;
    data->longitude = 116.407400;
    data->heading = 2.0f;
    data->track = 3.0f;
    data->ground_speed = 262.0f;
    data->true_air_speed = 262.0f;
    data->range_nm = 60.0f;
    data->nav_point_count = 8;
    data->active_point_index = 0;
    data->simulation_time = 0.0f;

    set_nav_point(&data->nav_points[0], "WPT01", ND_POINT_WAYPOINT, 40.664200, 116.427400, 1);
    set_nav_point(&data->nav_points[1], "WPT02", ND_POINT_WAYPOINT, 40.524200, 116.667400, 0);
    set_nav_point(&data->nav_points[2], "WPT03", ND_POINT_WAYPOINT, 40.374200, 116.887400, 0);
    set_nav_point(&data->nav_points[3], "WPT04", ND_POINT_WAYPOINT, 40.604200, 116.027400, 0);
    set_nav_point(&data->nav_points[4], "ZBAA", ND_POINT_AIRPORT, 40.080100, 116.584600, 0);
    set_nav_point(&data->nav_points[5], "TWR01", ND_POINT_TOWER, 40.075000, 116.592000, 0);
    set_nav_point(&data->nav_points[6], "VOR01", ND_POINT_VOR, 40.324200, 116.157400, 0);
    set_nav_point(&data->nav_points[7], "NDB01", ND_POINT_NDB, 40.024200, 116.087400, 0);

    nd_data_recalculate_nav_points(data);
}

void nd_data_update_mock(ND_Data *data, float delta_time)
{
    if (data == NULL)
    {
        return;
    }

    if (delta_time < 0.0f)
    {
        delta_time = 0.0f;
    }

    data->simulation_time += delta_time;
    const float t = data->simulation_time;

    data->heading = normalize_degrees(2.0f + 0.8f * sinf(t * 0.12f));
    data->track = normalize_degrees(data->heading + 1.0f + 0.4f * sinf(t * 0.20f));
    data->ground_speed = 262.0f + 2.0f * sinf(t * 0.35f) + 0.8f * cosf(t * 0.70f);
    data->true_air_speed = 262.0f + 1.5f * sinf(t * 0.28f + 0.8f);

    const float distance_nm = data->ground_speed * delta_time / 3600.0f;
    const float track_rad = data->track * ND_DEG_TO_RAD;
    const float latitude_rad = (float)data->latitude * ND_DEG_TO_RAD;
    const float longitude_scale = cosf(latitude_rad);

    data->latitude += (double)(cosf(track_rad) * distance_nm / 60.0f);
    if (fabsf(longitude_scale) > 0.001f)
    {
        data->longitude += (double)(sinf(track_rad) * distance_nm / (60.0f * longitude_scale));
    }

    nd_data_recalculate_nav_points(data);
}
