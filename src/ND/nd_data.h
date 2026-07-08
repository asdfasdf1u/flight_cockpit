#ifndef ND_DATA_H
#define ND_DATA_H

#define ND_MAX_NAV_POINTS 128
#define ND_NAME_LEN 32

typedef enum ND_PointType
{
    ND_POINT_WAYPOINT,
    ND_POINT_AIRPORT,
    ND_POINT_TOWER,
    ND_POINT_VOR,
    ND_POINT_NDB
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
} ND_Data;

void nd_data_init(ND_Data *data);
void nd_data_update_mock(ND_Data *data, float delta_time);
void nd_data_recalculate_nav_points(ND_Data *data);
void nd_data_set_range(ND_Data *data, float range_nm);

#endif
