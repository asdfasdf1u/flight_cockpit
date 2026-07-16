#ifndef SIM_DATA_CENTER_H
#define SIM_DATA_CENTER_H

#include "sim_data_loader.h"
#include "sim_xplane_live_frame.h"
#include "sim_snapshot.h"
#include "alert_manager.h"

typedef struct SimDataCenter
{
    SimDataStore store;           // 本地样本数据仓库
    SimSnapshot snapshot;         // 当前统一快照
    SimPlannedRoute planned_route; // FMC 提交的计划航路
    AlertManager alert_manager;   // 告警管理器

    float sim_time;                    // 统一仿真时间
    float delta_time;                  // 当前帧时间步长
    float playback_speed;              // 本地样本播放速度
    int initialized;                   // 数据中心初始化状态
    int route_initialized;             // 航路是否已初始化
    int route_revision;                // 航路版本号
    int xplane_recovery_frames;        // X-Plane 恢复稳定帧计数
    int source_log_initialized;        // 数据来源日志初始化状态
    SimSnapshotSource last_logged_source; // 上一次记录的数据来源
    SimFlightPhase flight_phase;       // 当前飞行阶段
    SimFlightPhase flight_phase_candidate; // 候选飞行阶段
    int flight_phase_candidate_frames; // 候选阶段持续帧数

    double nd_latitude;          // ND 当前纬度
    double nd_longitude;         // ND 当前经度
    int nd_position_initialized; // ND 位置初始化状态
    int demo_route_origin_initialized; // 当前 DATA_FILES 航路是否已执行过起点初始化
} SimDataCenter;

// 初始化统一数据中心
int sim_data_center_init(SimDataCenter *center);
// 销毁统一数据中心
void sim_data_center_destroy(SimDataCenter *center);
// 按本地样本更新时间
void sim_data_center_update(SimDataCenter *center, float delta_time);
void sim_data_center_set_playback_speed(SimDataCenter *center, float playback_speed);
void sim_data_center_set_position(SimDataCenter *center, double latitude, double longitude);
// 接收 X-Plane 实时帧并重建统一快照
int sim_data_center_apply_xplane_live_frame(SimDataCenter *center, const SimXPlaneLiveFrame *frame);
void sim_data_center_set_route(SimDataCenter *center, const SimPlannedRoute *route);
void sim_data_center_clear_route(SimDataCenter *center);
int sim_data_center_route_revision(const SimDataCenter *center);

// 给 PFD、ND、EICAS 等模块提供最新统一快照
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
