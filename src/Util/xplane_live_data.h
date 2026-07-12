#ifndef XPLANE_LIVE_DATA_H
#define XPLANE_LIVE_DATA_H

#include "../EICAS1/eicas_data.h"
#include "../ND/nd_data.h"
#include "../PFD/pfd_data.h"
#include "../Systems/aircraft_systems_data.h"
#include "xplaneConnect.h"

#define XPLANE_LIVE_DEFAULT_IP "127.0.0.1"
#define XPLANE_LIVE_DEFAULT_PORT 49009

typedef struct XPlaneLiveData
{
    XPCSocket socket;
    int socket_open;

    char xp_ip[16];
    unsigned short xp_port;

    int pfd_active;
    int nd_active;
    int eicas_active;
    int connected;

    int missed_frames;

    float poll_elapsed;
    float retry_elapsed;
} XPlaneLiveData;

void xplane_live_data_init(XPlaneLiveData *live, const char *xp_ip, unsigned short xp_port);
void xplane_live_data_shutdown(XPlaneLiveData *live);

int xplane_live_data_update(
    XPlaneLiveData *live,
    PFD_Data *pfd_data,
    ND_Data *nd_data,
    EICAS_Data *eicas_data,
    AircraftSystems_Data *systems_data,
    float delta_time);

int xplane_live_data_pfd_active(const XPlaneLiveData *live);
int xplane_live_data_nd_active(const XPlaneLiveData *live);
int xplane_live_data_eicas_active(const XPlaneLiveData *live);
int xplane_live_data_connected(const XPlaneLiveData *live);

#endif
