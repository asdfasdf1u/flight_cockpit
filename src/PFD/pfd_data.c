#include "pfd_data.h"

#include <math.h>
#include <stdio.h>

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

void pfd_data_init(PFD_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->airspeed = 245.0f;
    data->altitude = 12000.0f;
    data->vertical_speed = 0.0f;
    data->pitch = 0.0f;
    data->roll = 0.0f;
    data->heading = 90.0f;
    data->throttle = 65.0f;
    data->autopilot_on = 1;
    snprintf(data->flight_mode, sizeof(data->flight_mode), "%s", "LNAV VNAV");
    data->simulation_time = 0.0f;
}

void pfd_data_update_mock(PFD_Data *data, float delta_time)
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
    data->airspeed = 250.0f + 28.0f * sinf(t * 0.55f) + 6.0f * cosf(t * 1.35f);
    data->altitude = 12000.0f + 850.0f * sinf(t * 0.22f) + 120.0f * cosf(t * 0.70f);
    data->vertical_speed = 900.0f * cosf(t * 0.48f) + 180.0f * sinf(t * 1.25f);
    data->pitch = 4.5f * sinf(t * 0.62f);
    data->roll = 24.0f * sinf(t * 0.38f) + 4.0f * cosf(t * 0.90f);
    data->throttle = clamp_float(64.0f + 16.0f * sinf(t * 0.42f) + 5.0f * cosf(t * 1.10f), 0.0f, 100.0f);

    data->heading += (18.0f + 6.0f * sinf(t * 0.30f)) * delta_time;
    while (data->heading >= 360.0f)
    {
        data->heading -= 360.0f;
    }
    while (data->heading < 0.0f)
    {
        data->heading += 360.0f;
    }

    data->autopilot_on = ((int)(t / 12.0f) % 2) == 0;
    snprintf(data->flight_mode, sizeof(data->flight_mode), "%s",
             data->autopilot_on ? "LNAV VNAV" : "HDG HOLD");
}
