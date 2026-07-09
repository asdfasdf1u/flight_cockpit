#include "cabin_data.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void copy_text(char *dest, size_t dest_size, const char *src)
{
    if (dest == NULL || dest_size == 0)
    {
        return;
    }

    snprintf(dest, dest_size, "%s", src != NULL ? src : "");
}

static double lerp_double(double start, double end, float t)
{
    return start + (end - start) * (double)t;
}

static void cabin_data_update_current_position(Cabin_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->current_lat = lerp_double(data->origin_lat, data->destination_lat, data->progress);
    data->current_lon = lerp_double(data->origin_lon, data->destination_lon, data->progress);

    data->latitude = data->current_lat;
    data->longitude = data->current_lon;
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
    copy_text(data->origin_airport, sizeof(data->origin_airport), "北京首都国际机场");
    copy_text(data->destination_city, sizeof(data->destination_city), "北京大兴");
    copy_text(data->destination_airport, sizeof(data->destination_airport), "北京大兴国际机场");
    copy_text(data->current_city, sizeof(data->current_city), "北京城区");

    data->origin_lat = 40.080111;
    data->origin_lon = 116.584556;
    data->destination_lat = 39.509945;
    data->destination_lon = 116.410920;
    data->map_top_left_lat = 40.800000;
    data->map_top_left_lon = 115.250000;
    data->map_bottom_right_lat = 39.200000;
    data->map_bottom_right_lon = 117.450000;
    data->altitude = 5200.0f;
    data->ground_speed = 520.0f;
    data->progress = 0.28f;
    data->remaining_time_min = 28.0f;
    cabin_data_update_current_position(data);

    copy_text(data->weather, sizeof(data->weather), "晴");
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

    data->progress += delta_time * 0.012f;
    if (data->progress > 1.0f)
    {
        data->progress = 0.0f;
    }

    cabin_data_update_current_position(data);
    data->altitude = 5200.0f + 120.0f * sinf(data->progress * 6.2831853f);
    data->ground_speed = 520.0f + 12.0f * sinf(data->progress * 12.5663706f);
    data->remaining_time_min = (1.0f - data->progress) * 40.0f;

    if (data->api_weather_loaded)
    {
        return;
    }

    if (data->progress < 0.25f)
    {
        copy_text(data->current_city, sizeof(data->current_city), "首都机场附近");
    }
    else if (data->progress < 0.55f)
    {
        copy_text(data->current_city, sizeof(data->current_city), "北京城区");
    }
    else if (data->progress < 0.82f)
    {
        copy_text(data->current_city, sizeof(data->current_city), "北京南部");
    }
    else
    {
        copy_text(data->current_city, sizeof(data->current_city), "大兴机场附近");
    }
}
