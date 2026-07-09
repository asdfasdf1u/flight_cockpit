#ifndef FMC_WAYPOINT_H
#define FMC_WAYPOINT_H

#define FMC_WAYPOINT_IDENT_LEN 12
#define FMC_WAYPOINT_TYPE_LEN 8
#define FMC_WAYPOINT_REGION_LEN 8
#define FMC_WAYPOINT_MAX_MATCHES 6

typedef struct FMC_Waypoint
{
    char ident[FMC_WAYPOINT_IDENT_LEN];
    char type[FMC_WAYPOINT_TYPE_LEN];
    char region[FMC_WAYPOINT_REGION_LEN];
    double latitude;
    double longitude;
} FMC_Waypoint;

typedef struct FMC_WaypointMatchList
{
    FMC_Waypoint items[FMC_WAYPOINT_MAX_MATCHES];
    int count;
} FMC_WaypointMatchList;

int fmc_waypoint_index_load(const char *path);
int fmc_waypoint_index_count(void);
int fmc_waypoint_find_exact(const char *ident, FMC_Waypoint *waypoint);
int fmc_waypoint_search(const char *query, FMC_WaypointMatchList *matches);

#endif
