#ifndef CABIN_DATA_H
#define CABIN_DATA_H

#define CABIN_TEXT_LEN 64
#define CABIN_ERROR_LEN 160

typedef struct Cabin_Data
{
    char flight_no[CABIN_TEXT_LEN];
    char origin_city[CABIN_TEXT_LEN];
    char origin_airport[CABIN_TEXT_LEN];
    char destination_city[CABIN_TEXT_LEN];
    char destination_airport[CABIN_TEXT_LEN];
    char current_city[CABIN_TEXT_LEN];

    double origin_lat;
    double origin_lon;
    double destination_lat;
    double destination_lon;
    double current_lat;
    double current_lon;
    double map_top_left_lat;
    double map_top_left_lon;
    double map_bottom_right_lat;
    double map_bottom_right_lon;
    double latitude;
    double longitude;
    float altitude;
    float ground_speed;
    float progress;
    float remaining_time_min;

    char weather[CABIN_TEXT_LEN];
    float temperature;
    float humidity;
    char wind_direction[CABIN_TEXT_LEN];
    char wind_power[CABIN_TEXT_LEN];
    char weather_source[CABIN_TEXT_LEN];
    char weather_report_time[CABIN_TEXT_LEN];
    int api_weather_loaded;
    int api_weather_failed;
    char api_error_message[CABIN_ERROR_LEN];

    char map_source[CABIN_TEXT_LEN];
    int api_map_loaded;
    int api_map_failed;
    char api_map_error_message[CABIN_ERROR_LEN];
} Cabin_Data;

void cabin_data_init(Cabin_Data *data);
void cabin_data_update_mock(Cabin_Data *data, float delta_time);

#endif
