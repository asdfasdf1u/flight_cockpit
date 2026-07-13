#ifndef ND_DATA_H
#define ND_DATA_H

#include "../Data/sim_route.h"
#include "../Util/xplaneConnect.h"

#define MAX_TOTAL_WAYPOINTS 240000
#define HASH_BUCKET_SIZE 1009
#define GRID_SIZE 1.0
#define DATA_ROOT_PATH "assets/"

#define ND_MAX_NAV_POINTS 6144
#define ND_MAX_DATA_FRAMES 4096
#define ND_MAX_VISIBLE_FIX_POINTS 80
#define ND_MAX_VISIBLE_NAV_POINTS 28
#define ND_MAX_VISIBLE_AIRPORT_POINTS 18
#define ND_MAX_ROUTE_POINTS SIM_ROUTE_MAX_POINTS
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

typedef enum ND_MapLayer
{
    ND_MAP_LAYER_WPT,
    ND_MAP_LAYER_ARPT,
    ND_MAP_LAYER_STA,
    ND_MAP_LAYER_COUNT
} ND_MapLayer;

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

typedef struct ND_RoutePoint
{
    char ident[ND_NAME_LEN];
    double latitude;
    double longitude;
    int has_position;
} ND_RoutePoint;

typedef struct NDData
{
    double latitude;
    double longitude;

    float ground_speed;
    float true_air_speed;
    float heading;
} NDData;

typedef struct WAYPOINT
{
    int num;
    double lat;
    double lon;
    char name[20];
    double distance;
} WAYPOINT;

typedef struct HashNode
{
    char grid_key[20];
    WAYPOINT *wp_list;
    int wp_count;
    int wp_capacity;
    struct HashNode *next;
} HashNode;

typedef struct WaypointHashTable
{
    HashNode **buckets;
    int bucket_size;
} WaypointHashTable;

typedef struct WAYPOINT_RESULT
{
    WAYPOINT *data;
    int index;
    int count;
} WAYPOINT_RESULT;

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

    int map_layer_visible[ND_MAP_LAYER_COUNT];
    int map_labels_visible;

    ND_RoutePoint route_points[ND_MAX_ROUTE_POINTS];
    int route_point_count;
    int route_segment_count;
    int route_active_index;
    int route_cached_revision;
    int route_valid;
} ND_Data;

extern int waypoint_total_count;
extern WaypointHashTable *wp_hash_table;
extern int wp_result_total;
extern WAYPOINT_RESULT *wp_result;

int getNDData(XPCSocket sock, NDData *data);
int load_all_nav_data(void);
void free_nav_data(void);
int filter_waypoint_within_148km_ht(double target_lat, double target_lon, float heading);

void nd_data_init(ND_Data *data);
void nd_data_update_mock(ND_Data *data, float delta_time);
void nd_data_recalculate_nav_points(ND_Data *data);
void nd_data_set_range(ND_Data *data, float range_nm);
int nd_data_get_map_layer_visible(const ND_Data *data, ND_MapLayer layer);
void nd_data_set_map_layer_visible(ND_Data *data, ND_MapLayer layer, int visible);
void nd_data_toggle_map_layer_visible(ND_Data *data, ND_MapLayer layer);
int nd_data_get_map_labels_visible(const ND_Data *data);
void nd_data_set_map_labels_visible(ND_Data *data, int visible);
void nd_data_toggle_map_labels_visible(ND_Data *data);
int nd_data_sync_planned_route(ND_Data *data, const SimPlannedRoute *route, int route_revision, int force_check);

#endif
