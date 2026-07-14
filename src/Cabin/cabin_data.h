#ifndef CABIN_DATA_H
#define CABIN_DATA_H

#define CABIN_TEXT_LEN 64
#define CABIN_ERROR_LEN 160
#define CABIN_PLANNED_ROUTE_MAX_POINTS 64
#define CABIN_FLOWN_TRACK_MAX_POINTS 160

#include "../Data/alert_manager.h"
#include "../Data/sim_snapshot.h"

struct SimDataCenter;

enum
{
    CABIN_DATA_UPDATE_NONE = 0,
    CABIN_DATA_UPDATE_ROUTE = 1 << 0,
    CABIN_DATA_UPDATE_VALIDITY = 1 << 1
};

typedef enum Cabin_Place_Status
{
    CABIN_PLACE_EMPTY = 0,
    CABIN_PLACE_PENDING,
    CABIN_PLACE_VALID,
    CABIN_PLACE_FAILED
} Cabin_Place_Status;

typedef struct Cabin_Place
{
    Cabin_Place_Status status;
    double latitude;
    double longitude;
    int latitude_grid;
    int longitude_grid;
    int route_revision;
    float next_retry_sim_time;
    char source[CABIN_TEXT_LEN];
    char snapshot_source[CABIN_TEXT_LEN];
    char province[CABIN_TEXT_LEN];
    char city[CABIN_TEXT_LEN];
    char district[CABIN_TEXT_LEN];
} Cabin_Place;

typedef struct Cabin_Trajectory_Point
{
    char ident[CABIN_TEXT_LEN];
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
    double map_center_lat;
    double map_center_lon;
    int map_zoom;
    char map_cache_path[CABIN_ERROR_LEN];
    double latitude;
    double longitude;
    float altitude;
    float ground_speed;
    float true_air_speed;
    float vertical_speed;
    float heading;
    float track;
    int has_heading;
    int using_sim_data;
    int snapshot_valid;
    char data_source[CABIN_TEXT_LEN];
    int snapshot_frame;
    int frame_id;
    float timestamp;
    SimSnapshotSource snapshot_source;
    int fallback_active;
    int xplane_connected;
    int timed_out;
    float last_valid_xplane_timestamp;
    float snapshot_time;
    int engine_left_running;
    int engine_right_running;
    AlertSnapshot alerts;
    Cabin_Place current_place;
    Cabin_Place origin_place;
    Cabin_Place destination_place;
    char flight_phase[CABIN_TEXT_LEN];
    int route_valid;
    int route_revision;
    int route_point_count;
    int active_waypoint_index;
    char active_waypoint[CABIN_TEXT_LEN];
    float distance_to_active_nm;
    float distance_to_destination_nm;
    int planned_route_from_fmc;
    char planned_route_source[CABIN_TEXT_LEN];
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

    int crash_demo_active;
    unsigned int crash_demo_started_ticks;
} Cabin_Data;

void cabin_data_init(Cabin_Data *data);
int cabin_data_apply_sim_data_center(Cabin_Data *data, const struct SimDataCenter *center, float delta_time);
const char *cabin_place_display_name(const Cabin_Place *place);

#endif
