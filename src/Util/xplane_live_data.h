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
    XPCSocket socket; // RREF 主数据 socket
    int socket_open;  // 主 socket 打开状态

    /* 独立 socket，用于通过 getDREF 主动查询报警状态数据ref，
     * 避免与 RREF 订阅数据流互相干扰。 */
    XPCSocket alarm_socket; // 报警状态查询 socket
    int alarm_socket_open;  // 报警 socket 打开状态

    char xp_ip[16];         // X-Plane IP 地址
    unsigned short xp_port; // X-Plane UDP 端口

    int pfd_active;   // PFD 数据有效状态
    int nd_active;    // ND 数据有效状态
    int eicas_active; // EICAS 数据有效状态
    int fmc_active;   // FMC 实时状态
    int connected;    // X-Plane 总连接状态

    int missed_frames;     // 连续丢帧计数
    int frame_id;          // 最新实时帧号
    float elapsed_time;    // 连接累计时间
    float last_valid_time; // 最近有效帧时间

    float poll_elapsed;       // RREF 轮询计时
    float retry_elapsed;      // 断线重试计时
    float alarm_poll_elapsed; // 报警 dataref 轮询计时
    int alarm_values[5];      // 报警状态缓存
    int alarm_values_valid;   // 报警状态是否有效

    int rref_subscribed; // RREF 订阅状态
    int rref_subscription_count; // RREF 订阅数量
    int rref_binding_dref_index[XPLANE_RREF_MAX_SUBSCRIPTIONS];    // 订阅序号对应的 dataref
    int rref_binding_element_index[XPLANE_RREF_MAX_SUBSCRIPTIONS]; // 订阅序号对应的数组元素
    float rref_values[XPLANE_RREF_MAX_SUBSCRIPTIONS];              // RREF 最新数值
    float rref_last_update[XPLANE_RREF_MAX_SUBSCRIPTIONS];         // 每个订阅最近更新时间
    unsigned char rref_seen[XPLANE_RREF_MAX_SUBSCRIPTIONS];        // 是否收到过该订阅数据
    float rref_last_packet_time;    // 最近收到 RREF 包的时间
    float rref_last_subscribe_time; // 最近发送订阅请求的时间

    int compare_initialized; // 快照比对是否初始化
    int compare_connected;   // 上次比对连接状态
    int compare_timed_out;   // 上次比对超时状态
    unsigned long long compare_mismatch_mask; // 字段比对差异掩码
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
    SimDataCenter *sim_data_center;       // 共享统一数据中心
    XPlaneLiveData live_data;             // X-Plane 实时连接状态
    PFD_Data *pfd_shadow;                 // PFD 旧接口影子数据
    ND_Data *nd_shadow;                   // ND 旧接口影子数据
    EICAS_Data *eicas_shadow;             // EICAS 旧接口影子数据
    AircraftSystems_Data *systems_shadow; // 系统旧接口影子数据
    int initialized;                      // 共享运行时初始化状态
    int last_frame_id;                    // 上一次日志帧号
    int last_source;                      // 上一次数据来源
    int last_valid;                       // 上一次有效状态
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

// 初始化 X-Plane 连接
void xplane_live_data_init(XPlaneLiveData *live, const char *xp_ip, unsigned short xp_port);
// 关闭 X-Plane 连接
void xplane_live_data_shutdown(XPlaneLiveData *live);

// 旧路径：直接写入各模块数据
int xplane_live_data_update(
    XPlaneLiveData *live,
    const XPlaneLiveDataTargets *targets,
    FMC_Data *fmc_data,
    float delta_time);

// 新路径：写入统一数据中心
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

// 初始化共享运行时
void xplane_shared_runtime_init(
    XPlaneSharedRuntime *runtime,
    SimDataCenter *sim_data_center,
    const char *xp_ip,
    unsigned short xp_port);
// 关闭共享运行时
void xplane_shared_runtime_shutdown(XPlaneSharedRuntime *runtime);
// 更新共享实时数据
int xplane_shared_runtime_update(XPlaneSharedRuntime *runtime, float delta_time);
// 获取共享数据中心
SimDataCenter *xplane_shared_runtime_data_center(XPlaneSharedRuntime *runtime);
// 获取共享快照
const SimSnapshot *xplane_shared_runtime_snapshot(const XPlaneSharedRuntime *runtime);
// 检查共享运行时状态
int xplane_shared_runtime_initialized(const XPlaneSharedRuntime *runtime);

#endif
