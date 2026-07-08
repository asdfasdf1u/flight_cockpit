#ifndef ND_DATA_H
#define ND_DATA_H

#define ND_MAX_NAV_POINTS 6144
#define ND_MAX_DATA_FRAMES 4096
#define ND_MAX_VISIBLE_FIX_POINTS 80
#define ND_MAX_VISIBLE_NAV_POINTS 28
#define ND_MAX_VISIBLE_AIRPORT_POINTS 18
#define ND_NAME_LEN 32

typedef enum ND_PointType
{
    ND_POINT_WAYPOINT,
    ND_POINT_AIRPORT,
    ND_POINT_TOWER,
    ND_POINT_VOR,
    ND_POINT_NDB,
    ND_POINT_ILS
} ND_PointType;

typedef struct ND_NavPoint
{
    char ident[ND_NAME_LEN];
    ND_PointType type;

    double latitude;
    double longitude;

    float distance_nm;
    float bearing_deg;

    int visible;
    int active;
} ND_NavPoint;

typedef struct ND_DataFrame
{
    float time_sec;
    double latitude;
    double longitude;
    float heading;
    float track;
    float ground_speed;
    float true_air_speed;
    float range_nm;
    float active_waypoint_distance_nm;
    float active_waypoint_eta_min;
    unsigned int fields;
} ND_DataFrame;

typedef struct ND_Data
{
    double latitude;
    double longitude;

    float heading;
    float track;
    float ground_speed;
    float true_air_speed;
    float range_nm;

    ND_NavPoint nav_points[ND_MAX_NAV_POINTS];
    int nav_point_count;

    int active_point_index;

    char active_waypoint_name[ND_NAME_LEN];
    float active_waypoint_distance_nm;
    float active_waypoint_bearing_deg;
    float active_waypoint_eta_min;

    float simulation_time;

    ND_DataFrame data_frames[ND_MAX_DATA_FRAMES];
    int data_frame_count;
    int data_frame_index;
    float data_frame_elapsed;
    float data_frame_step_sec;
    int data_file_loaded;
    int data_file_has_time;

    int mock_nav_point_count;
    int earth_fix_loaded;
    int earth_fix_count;
    int earth_nav_loaded;
    int earth_nav_count;
    int earth_nav_vor_count;
    int earth_nav_ndb_count;
    int earth_nav_ils_count;
    int apt_loaded;
    int apt_airport_count;
    int apt_tower_count;
} ND_Data;

void nd_data_init(ND_Data *data);
void nd_data_update_mock(ND_Data *data, float delta_time);
void nd_data_recalculate_nav_points(ND_Data *data);
void nd_data_set_range(ND_Data *data, float range_nm);

#endif
