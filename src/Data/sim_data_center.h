#ifndef SIM_DATA_CENTER_H
#define SIM_DATA_CENTER_H

#include "sim_data_loader.h"
#include "sim_xplane_live_frame.h"
#include "sim_snapshot.h"
#include "alert_manager.h"

typedef struct SimDataCenter
{
    SimDataStore store;
    SimSnapshot snapshot;
    SimPlannedRoute planned_route;
    AlertManager alert_manager;

    float sim_time;
    float delta_time;
    float playback_speed;
    int initialized;
    int route_initialized;
    int route_revision;
    int xplane_recovery_frames;
    int source_log_initialized;
    SimSnapshotSource last_logged_source;
    SimFlightPhase flight_phase;
    SimFlightPhase flight_phase_candidate;
    int flight_phase_candidate_frames;

    double nd_latitude;
    double nd_longitude;
    int nd_position_initialized;
} SimDataCenter;

int sim_data_center_init(SimDataCenter *center);
void sim_data_center_destroy(SimDataCenter *center);
void sim_data_center_update(SimDataCenter *center, float delta_time);
void sim_data_center_set_playback_speed(SimDataCenter *center, float playback_speed);
void sim_data_center_set_position(SimDataCenter *center, double latitude, double longitude);
int sim_data_center_apply_xplane_live_frame(SimDataCenter *center, const SimXPlaneLiveFrame *frame);
void sim_data_center_set_route(SimDataCenter *center, const SimPlannedRoute *route);
void sim_data_center_clear_route(SimDataCenter *center);
int sim_data_center_route_revision(const SimDataCenter *center);

const SimSnapshot *sim_data_center_snapshot(const SimDataCenter *center);
const AlertSnapshot *sim_data_center_alerts(const SimDataCenter *center);
const SimPlannedRoute *sim_data_center_route(const SimDataCenter *center);
const char *sim_data_center_route_source_name(SimRouteSource source);
const char *sim_snapshot_source_name(SimSnapshotSource source);
const char *sim_flight_phase_name(SimFlightPhase phase);
int sim_data_center_is_ready(const SimDataCenter *center);
int sim_data_center_has_route(const SimDataCenter *center);
int sim_data_center_has_pfd_data(const SimDataCenter *center);
int sim_data_center_has_nd_data(const SimDataCenter *center);
int sim_data_center_has_nd_position_data(const SimDataCenter *center);
int sim_data_center_has_eicas_upper_data(const SimDataCenter *center);
int sim_data_center_has_eicas_lower_data(const SimDataCenter *center);
int sim_data_center_has_eicas_data(const SimDataCenter *center);
void sim_data_center_acknowledge_alert(SimDataCenter *center, AlertType type);
void sim_data_center_set_demo_alert(SimDataCenter *center, AlertType type, int active);
void sim_data_center_clear_demo_alerts(SimDataCenter *center);

#endif
