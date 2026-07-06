#ifndef ND_DATA_H
#define ND_DATA_H

#define ND_MAX_WAYPOINTS 8
#define ND_NAME_LEN 32

typedef struct ND_Waypoint
{
    char name[ND_NAME_LEN];
    float rel_x;
    float rel_y;
    float distance_nm;
    float bearing_deg;
} ND_Waypoint;

typedef struct ND_Data
{
    double latitude;
    double longitude;

    float heading;
    float ground_speed;
    float track;

    ND_Waypoint waypoints[ND_MAX_WAYPOINTS];
    int waypoint_count;

    int active_waypoint_index;
    float simulation_time;
} ND_Data;

void nd_data_init(ND_Data *data);
void nd_data_update_mock(ND_Data *data, float delta_time);

#endif
