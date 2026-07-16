#ifndef SIM_XPLANE_LIVE_FRAME_H
#define SIM_XPLANE_LIVE_FRAME_H

typedef struct SimXPlaneLiveFrame
{
    int valid;                 // 实时帧是否有效
    int connected;             // X-Plane 是否连接
    int timed_out;             // 数据是否超时
    int frame_id;              // 实时帧编号
    float timestamp;           // 当前帧时间戳
    float delta_time;          // 当前帧时间步长
    float last_valid_timestamp; // 最近有效帧时间

    double latitude;       // 纬度
    double longitude;      // 经度
    float altitude;        // 气压高度
    float agl_altitude;    // 离地高度
    float airspeed;        // 指示空速
    float airspeed_target; // 目标空速
    float true_air_speed;  // 真空速
    float ground_speed;    // 地速
    float heading;         // 磁航向
    float heading_target;  // 目标航向
    float altitude_target; // 目标高度
    float track;           // 航迹角
    float pitch;           // 俯仰角
    float roll;            // 滚转角
    float yaw;             // 偏航角
    float vertical_speed;  // 垂直速度
    float throttle;        // 油门

    float total_air_temperature; // 总温
    float n1_left;               // 左发 N1
    float n1_right;              // 右发 N1
    float n2_left;               // 左发 N2
    float n2_right;              // 右发 N2
    float egt_left;              // 左发排气温度
    float egt_right;             // 右发排气温度
    float fuel_flow_left;        // 左发燃油流量
    float fuel_flow_right;       // 右发燃油流量
    float oil_pressure_left;     // 左发滑油压力
    float oil_pressure_right;    // 右发滑油压力
    float oil_temperature_left;  // 左发滑油温度
    float oil_temperature_right; // 右发滑油温度
    float oil_quantity_left;     // 左发滑油量
    float oil_quantity_right;    // 右发滑油量
    float vibration_left;        // 左发振动
    float vibration_right;       // 右发振动
    int engine_left_running;     // 左发运行状态
    int engine_right_running;    // 右发运行状态

    float fuel_quantity;             // 总燃油量
    float fuel_left_quantity;        // 左油箱油量
    float fuel_center_quantity;      // 中央油箱油量
    float fuel_right_quantity;       // 右油箱油量
    int fuel_tank_quantities_valid;  // 分油箱油量是否有效

    int gear_down;            // 起落架放下状态
    int flaps_level;          // 襟翼角度/档位
    int parking_brake_on;     // 停留刹车状态
    int has_gear;             // 是否读取到起落架数据
    int has_flaps;            // 是否读取到襟翼数据
    int has_parking_brake;    // 是否读取到停留刹车数据

    /* X-Plane 原生报警状态（通过 getDREF 读取 sim/cockpit/warnings/*） */
    int xplane_master_warning;   // 主警告
    int xplane_master_caution;   // 主注意
    int xplane_engine_fire;      // 发动机火警
    int xplane_stall_warning;    // 失速警告
    int xplane_overspeed_warning; // 超速警告
} SimXPlaneLiveFrame;

#endif
