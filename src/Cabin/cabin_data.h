#ifndef CABIN_DATA_H
#define CABIN_DATA_H

#define CABIN_TEXT_LEN 64
#define CABIN_ERROR_LEN 160
#define CABIN_PLANNED_ROUTE_MAX_POINTS 12
#define CABIN_FLOWN_TRACK_MAX_POINTS 160

typedef struct Cabin_Trajectory_Point
{
    double latitude;
    double longitude;
    unsigned int sequence;
    float altitude;
    float ground_speed;
} Cabin_Trajectory_Point;

typedef struct Cabin_Data
{
    char flight_no[CABIN_TEXT_LEN];
    char origin_city[CABIN_TEXT_LEN];
    char origin_airport[CABIN_TEXT_LEN];
    char destination_city[CABIN_TEXT_LEN];
    char destination_airport[CABIN_TEXT_LEN];
    char current_city[CABIN_TEXT_LEN];
    char current_district[CABIN_TEXT_LEN];
    char current_town[CABIN_TEXT_LEN];

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
    Cabin_Trajectory_Point planned_route[CABIN_PLANNED_ROUTE_MAX_POINTS];
    int planned_route_count;
    Cabin_Trajectory_Point flown_track[CABIN_FLOWN_TRACK_MAX_POINTS];
    int flown_track_count;
    unsigned int flown_track_next_sequence;
    float flown_track_last_progress;
    float flown_track_time_since_append;

    char weather[CABIN_TEXT_LEN];
    char weather_city[CABIN_TEXT_LEN];
    char weather_adcode[CABIN_TEXT_LEN];
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
