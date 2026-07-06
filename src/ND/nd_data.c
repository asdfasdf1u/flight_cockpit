#include "nd_data.h"

#include <math.h>
#include <stdio.h>

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

static void set_waypoint(ND_Waypoint *waypoint, const char *name, float rel_x, float rel_y)
{
    if (waypoint == NULL)
    {
        return;
    }

    snprintf(waypoint->name, sizeof(waypoint->name), "%s", name);
    waypoint->rel_x = rel_x;
    waypoint->rel_y = rel_y;
    waypoint->distance_nm = sqrtf(rel_x * rel_x + rel_y * rel_y) * 80.0f;
    waypoint->bearing_deg = normalize_degrees(atan2f(rel_x, -rel_y) * 57.2957795f);
}

void nd_data_init(ND_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->latitude = 39.904200;
    data->longitude = 116.407400;
    data->heading = 82.0f;
    data->ground_speed = 438.0f;
    data->track = 85.0f;
    data->waypoint_count = 4;
    data->active_waypoint_index = 0;
    data->simulation_time = 0.0f;

    set_waypoint(&data->waypoints[0], "WPT01", 0.18f, -0.52f);
    set_waypoint(&data->waypoints[1], "WPT02", 0.46f, -0.30f);
    set_waypoint(&data->waypoints[2], "WPT03", 0.30f, 0.04f);
    set_waypoint(&data->waypoints[3], "WPT04", -0.18f, 0.32f);
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

    data->heading = normalize_degrees(data->heading + (7.5f + 2.5f * sinf(t * 0.35f)) * delta_time);
    data->track = normalize_degrees(data->heading + 4.0f * sinf(t * 0.62f));
    data->ground_speed = 438.0f + 18.0f * sinf(t * 0.28f) + 6.0f * cosf(t * 0.90f);

    data->latitude += (double)(0.000020f * cosf(data->track * 0.0174532925f) * delta_time);
    data->longitude += (double)(0.000030f * sinf(data->track * 0.0174532925f) * delta_time);

    for (int i = 0; i < data->waypoint_count; ++i)
    {
        ND_Waypoint *waypoint = &data->waypoints[i];
        waypoint->rel_x += 0.018f * sinf(t * 0.25f + (float)i) * delta_time;
        waypoint->rel_y += 0.014f * cosf(t * 0.22f + (float)i * 0.7f) * delta_time;

        if (waypoint->rel_x > 0.85f)
        {
            waypoint->rel_x = -0.85f;
        }
        else if (waypoint->rel_x < -0.85f)
        {
            waypoint->rel_x = 0.85f;
        }

        if (waypoint->rel_y > 0.72f)
        {
            waypoint->rel_y = -0.72f;
        }
        else if (waypoint->rel_y < -0.72f)
        {
            waypoint->rel_y = 0.72f;
        }

        waypoint->distance_nm = sqrtf(waypoint->rel_x * waypoint->rel_x + waypoint->rel_y * waypoint->rel_y) * 80.0f;
        waypoint->bearing_deg = normalize_degrees(atan2f(waypoint->rel_x, -waypoint->rel_y) * 57.2957795f);
    }

    if (data->waypoint_count > 0)
    {
        data->active_waypoint_index = ((int)(t / 10.0f)) % data->waypoint_count;
    }
}
