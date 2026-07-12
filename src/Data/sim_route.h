#ifndef SIM_ROUTE_H
#define SIM_ROUTE_H

#define SIM_ROUTE_MAX_POINTS 64
#define SIM_ROUTE_TEXT_LEN 64
#define SIM_ROUTE_PATH_LEN 192

typedef enum SimRouteSource
{
    SIM_ROUTE_SOURCE_NONE = 0,
    SIM_ROUTE_SOURCE_FMC_FMS_FILE,
    SIM_ROUTE_SOURCE_FMC_FALLBACK
} SimRouteSource;

typedef struct SimRoutePoint
{
    char ident[SIM_ROUTE_TEXT_LEN];
    char type[SIM_ROUTE_TEXT_LEN];
    double latitude;
    double longitude;
    double altitude;
    int has_position;
} SimRoutePoint;

typedef struct SimPlannedRoute
{
    int valid;
    SimRouteSource source;
    char origin[SIM_ROUTE_TEXT_LEN];
    char destination[SIM_ROUTE_TEXT_LEN];
    char source_path[SIM_ROUTE_PATH_LEN];
    int loaded_from_file;
    int has_coordinates;
    int active_waypoint_index;
    int point_count;
    SimRoutePoint points[SIM_ROUTE_MAX_POINTS];
} SimPlannedRoute;

#endif
