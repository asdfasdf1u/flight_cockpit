#ifndef SIM_DATA_CENTER_H
#define SIM_DATA_CENTER_H

#include "sim_data_loader.h"
#include "sim_snapshot.h"

typedef struct SimDataCenter
{
    SimDataStore store;
    SimSnapshot snapshot;
    SimPlannedRoute planned_route;

    float sim_time;
    float delta_time;
    float playback_speed;
    int initialized;
    int route_initialized;
    int route_revision;

    double nd_latitude;
    double nd_longitude;
    int nd_position_initialized;
} SimDataCenter;

int sim_data_center_init(SimDataCenter *center);
void sim_data_center_destroy(SimDataCenter *center);
void sim_data_center_update(SimDataCenter *center, float delta_time);
void sim_data_center_set_playback_speed(SimDataCenter *center, float playback_speed);
void sim_data_center_set_position(SimDataCenter *center, double latitude, double longitude);
void sim_data_center_set_route(SimDataCenter *center, const SimPlannedRoute *route);
void sim_data_center_clear_route(SimDataCenter *center);
int sim_data_center_route_revision(const SimDataCenter *center);

const SimSnapshot *sim_data_center_snapshot(const SimDataCenter *center);
const SimPlannedRoute *sim_data_center_route(const SimDataCenter *center);
const char *sim_data_center_route_source_name(SimRouteSource source);
int sim_data_center_is_ready(const SimDataCenter *center);
int sim_data_center_has_route(const SimDataCenter *center);
int sim_data_center_has_pfd_data(const SimDataCenter *center);
int sim_data_center_has_nd_data(const SimDataCenter *center);
int sim_data_center_has_nd_position_data(const SimDataCenter *center);
int sim_data_center_has_eicas_upper_data(const SimDataCenter *center);
int sim_data_center_has_eicas_lower_data(const SimDataCenter *center);
int sim_data_center_has_eicas_data(const SimDataCenter *center);

#endif
