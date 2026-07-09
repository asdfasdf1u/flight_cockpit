#include "cabin_data.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CABIN_ROUTE_TOTAL_TIME_MIN 165.0f
#define CABIN_ROUTE_PROGRESS_RATE 0.012f

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
    copy_text(data->current_city, sizeof(data->current_city), "北京");

    data->origin_lat = 40.080111;
    data->origin_lon = 116.584556;
    data->destination_lat = 30.312520;
    data->destination_lon = 104.441284;
    data->map_top_left_lat = 44.863010;
    data->map_top_left_lon = 88.010000;
    data->map_bottom_right_lat = 24.236116;
    data->map_bottom_right_lon = 133.010000;
    data->altitude = 9200.0f;
    data->ground_speed = 820.0f;
    data->progress = 0.16f;
    cabin_data_update_progress_fields(data);

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

    data->progress += delta_time * CABIN_ROUTE_PROGRESS_RATE;
    if (data->progress > 1.0f)
    {
        data->progress = 0.0f;
    }

    cabin_data_update_progress_fields(data);

    if (data->progress < 0.30f)
    {
        copy_text(data->current_city, sizeof(data->current_city), "北京");
    }
    else if (data->progress < 0.72f)
    {
        copy_text(data->current_city, sizeof(data->current_city), "飞行途中");
    }
    else
    {
        copy_text(data->current_city, sizeof(data->current_city), "成都");
    }
}
