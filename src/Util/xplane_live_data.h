#ifndef XPLANE_LIVE_DATA_H
#define XPLANE_LIVE_DATA_H

#include "../EICAS1/eicas_data.h"
#include "../FMC/fmc_ui_adapter.h"
#include "../ND/nd_data.h"
#include "../PFD/pfd_data.h"
#include "../Systems/aircraft_systems_data.h"
#include "xplaneConnect.h"

#define XPLANE_LIVE_DEFAULT_IP "127.0.0.1"
#define XPLANE_LIVE_DEFAULT_PORT 49000
#define XPLANE_RREF_MAX_SUBSCRIPTIONS 128

typedef struct SimDataCenter SimDataCenter;
typedef struct SimSnapshot SimSnapshot;

typedef struct XPlaneLiveData
{
    XPCSocket socket;
    int socket_open;

    char xp_ip[16];
    unsigned short xp_port;

    int pfd_active;
    int nd_active;
    int eicas_active;
    int fmc_active;
    int connected;

    int missed_frames;
    int frame_id;
    float elapsed_time;
    float last_valid_time;

    float poll_elapsed;
    float retry_elapsed;

    int rref_subscribed;
    int rref_subscription_count;
    int rref_binding_dref_index[XPLANE_RREF_MAX_SUBSCRIPTIONS];
    int rref_binding_element_index[XPLANE_RREF_MAX_SUBSCRIPTIONS];
    float rref_values[XPLANE_RREF_MAX_SUBSCRIPTIONS];
    float rref_last_update[XPLANE_RREF_MAX_SUBSCRIPTIONS];
    unsigned char rref_seen[XPLANE_RREF_MAX_SUBSCRIPTIONS];
    float rref_last_packet_time;
    float rref_last_subscribe_time;

    int compare_initialized;
    int compare_connected;
    int compare_timed_out;
    unsigned long long compare_mismatch_mask;
} XPlaneLiveData;

typedef struct XPlaneSharedRuntime
{
    SimDataCenter *sim_data_center;
    XPlaneLiveData live_data;
    PFD_Data *pfd_shadow;
    ND_Data *nd_shadow;
    EICAS_Data *eicas_shadow;
    AircraftSystems_Data *systems_shadow;
    int initialized;
    int last_frame_id;
    int last_source;
    int last_valid;
} XPlaneSharedRuntime;

void xplane_live_data_init(XPlaneLiveData *live, const char *xp_ip, unsigned short xp_port);
void xplane_live_data_shutdown(XPlaneLiveData *live);

int xplane_live_data_update(
    XPlaneLiveData *live,
    PFD_Data *pfd_data,
    ND_Data *nd_data,
    EICAS_Data *eicas_data,
    AircraftSystems_Data *systems_data,
    FMC_Data *fmc_data,
    float delta_time);

int xplane_live_data_update_with_sim_data_center(
    XPlaneLiveData *live,
    PFD_Data *pfd_data,
    ND_Data *nd_data,
    EICAS_Data *eicas_data,
    AircraftSystems_Data *systems_data,
    SimDataCenter *sim_data_center,
    float delta_time);

int xplane_live_data_pfd_active(const XPlaneLiveData *live);
int xplane_live_data_nd_active(const XPlaneLiveData *live);
int xplane_live_data_eicas_active(const XPlaneLiveData *live);
int xplane_live_data_fmc_active(const XPlaneLiveData *live);
int xplane_live_data_connected(const XPlaneLiveData *live);
int xplane_live_data_has_valid_frame(const XPlaneLiveData *live);

void xplane_shared_runtime_init(
    XPlaneSharedRuntime *runtime,
    SimDataCenter *sim_data_center,
    const char *xp_ip,
    unsigned short xp_port);
void xplane_shared_runtime_shutdown(XPlaneSharedRuntime *runtime);
int xplane_shared_runtime_update(XPlaneSharedRuntime *runtime, float delta_time);
SimDataCenter *xplane_shared_runtime_data_center(XPlaneSharedRuntime *runtime);
const SimSnapshot *xplane_shared_runtime_snapshot(const XPlaneSharedRuntime *runtime);
int xplane_shared_runtime_initialized(const XPlaneSharedRuntime *runtime);

#endif
