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

static void update_waypoint_metrics(ND_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    for (int i = 0; i < data->waypoint_count; ++i)
    {
        ND_Waypoint *waypoint = &data->waypoints[i];
        const float lateral_nm = waypoint->rel_x * data->range_nm * 0.70f;
        const float forward_nm = waypoint->rel_y * data->range_nm;
        const float relative_bearing = atan2f(lateral_nm, forward_nm) * ND_RAD_TO_DEG;

        waypoint->distance_nm = sqrtf(lateral_nm * lateral_nm + forward_nm * forward_nm);
        waypoint->bearing_deg = normalize_degrees(data->track + relative_bearing);
    }
}

static void update_active_waypoint_display(ND_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    if (data->active_waypoint_index >= 0 && data->active_waypoint_index < data->waypoint_count)
    {
        const ND_Waypoint *active = &data->waypoints[data->active_waypoint_index];
        snprintf(data->active_waypoint_name, sizeof(data->active_waypoint_name), "%s", active->name);
    }

    if (data->ground_speed > 1.0f)
    {
        data->active_waypoint_eta_min = data->active_waypoint_distance_nm / data->ground_speed * 60.0f;
    }
    else
    {
        data->active_waypoint_eta_min = 0.0f;
    }
}

static void set_waypoint(ND_Waypoint *waypoint, const char *name, float rel_x, float rel_y)
{
    if (waypoint == NULL)
    {
        return;
    }

    snprintf(waypoint->name, sizeof(waypoint->name), "%s", name);
    waypoint->rel_x = rel_x;
    waypoint->rel_y = rel_y;
    waypoint->distance_nm = 0.0f;
    waypoint->bearing_deg = 0.0f;
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
    data->range_nm = 40.0f;
    data->active_waypoint_distance_nm = 1.0f;
    data->active_waypoint_eta_min = 0.3f;
    data->waypoint_count = 4;
    data->active_waypoint_index = -1;
    data->simulation_time = 0.0f;
    snprintf(data->active_waypoint_name, sizeof(data->active_waypoint_name), "RWY02L");

    set_waypoint(&data->waypoints[0], "WPT01", 0.00f, 0.90f);
    set_waypoint(&data->waypoints[1], "WPT02", 0.18f, 0.78f);
    set_waypoint(&data->waypoints[2], "WPT03", 0.36f, 0.65f);
    set_waypoint(&data->waypoints[3], "WPT04", -0.30f, 0.98f);

    update_waypoint_metrics(data);
    update_active_waypoint_display(data);
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
    data->active_waypoint_distance_nm = 1.0f + 0.03f * sinf(t * 0.60f);

    const float distance_nm = data->ground_speed * delta_time / 3600.0f;
    const float track_rad = data->track * ND_DEG_TO_RAD;
    const float latitude_rad = (float)data->latitude * ND_DEG_TO_RAD;
    const float longitude_scale = cosf(latitude_rad);

    data->latitude += (double)(cosf(track_rad) * distance_nm / 60.0f);
    if (fabsf(longitude_scale) > 0.001f)
    {
        data->longitude += (double)(sinf(track_rad) * distance_nm / (60.0f * longitude_scale));
    }

    update_waypoint_metrics(data);
    update_active_waypoint_display(data);
}
