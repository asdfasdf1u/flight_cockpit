#ifndef XPLANE_LIVE_DATA_H
#define XPLANE_LIVE_DATA_H

#include "../EICAS1/eicas_data.h"
#include "../FMC/fmc_ui_adapter.h"
#include "../ND/nd_data.h"
#include "../PFD/pfd_data.h"
#include "../Systems/aircraft_systems_data.h"
#include "../Data/sim_xplane_live_frame.h"
#include <SDL2/SDL.h>
#ifdef _WIN32
#include <windows.h>
#endif
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

    /* 独立 socket，用于通过 getDREF 主动查询报警状态数据ref，
     * 避免与 RREF 订阅数据流互相干扰。 */
    XPCSocket alarm_socket;
    int alarm_socket_open;

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
    float alarm_poll_elapsed;
    int alarm_values[5];
    int alarm_values_valid;

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

/* X-Plane 采集阶段使用的模块临时数据，避免接口传递过长的指针列表。 */
typedef struct XPlaneLiveDataTargets
{
    PFD_Data *pfd_data;
    ND_Data *nd_data;
    EICAS_Data *eicas_data;
    AircraftSystems_Data *systems_data;
} XPlaneLiveDataTargets;

typedef struct XPlaneSharedRuntime
{
    SimDataCenter *sim_data_center;

    /* 数据线程独占 UDP socket 与临时模块数据；SDL 渲染线程不直接访问它们。 */
    XPlaneLiveData live_data;
    PFD_Data *pfd_shadow;
    ND_Data *nd_shadow;
    EICAS_Data *eicas_shadow;
    AircraftSystems_Data *systems_shadow;

    /* 双缓冲只传递完整的飞行帧，避免主线程读到采集中的半帧数据。 */
#ifdef _WIN32
    HANDLE data_thread;
#else
    SDL_Thread *data_thread;
#endif
    SDL_mutex *frame_mutex;
    SDL_atomic_t data_ready;
    SDL_atomic_t thread_exit;
    SimXPlaneLiveFrame frame_buffers[2];
    int published_frame_index;
    unsigned int published_frame_revision;
    unsigned int consumed_frame_revision;

    int initialized;
    int last_frame_id;
    int last_source;
    int last_valid;
} XPlaneSharedRuntime;

void xplane_live_data_init(XPlaneLiveData *live, const char *xp_ip, unsigned short xp_port);
void xplane_live_data_shutdown(XPlaneLiveData *live);

int xplane_live_data_update(
    XPlaneLiveData *live,
    const XPlaneLiveDataTargets *targets,
    FMC_Data *fmc_data,
    float delta_time);

int xplane_live_data_update_with_sim_data_center(
    XPlaneLiveData *live,
    const XPlaneLiveDataTargets *targets,
    SimDataCenter *sim_data_center,
    float delta_time);

/* 仅获取一帧 X-Plane 数据。调用者负责决定在哪个线程写入 SimDataCenter。 */
int xplane_live_data_collect_frame(
    XPlaneLiveData *live,
    const XPlaneLiveDataTargets *targets,
    SimXPlaneLiveFrame *frame,
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
int xplane_shared_runtime_update(XPlaneSharedRuntime *runtime);
SimDataCenter *xplane_shared_runtime_data_center(XPlaneSharedRuntime *runtime);
const SimSnapshot *xplane_shared_runtime_snapshot(const XPlaneSharedRuntime *runtime);
int xplane_shared_runtime_initialized(const XPlaneSharedRuntime *runtime);

#endif
