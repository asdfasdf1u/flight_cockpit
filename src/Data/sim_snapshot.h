#ifndef SIM_SNAPSHOT_H
#define SIM_SNAPSHOT_H

#define SIM_SNAPSHOT_MAX_WARNINGS 8
#define SIM_SNAPSHOT_WARNING_TEXT_LEN 64

typedef enum SimWarningLevel
{
    SIM_WARNING_INFO,
    SIM_WARNING_CAUTION,
    SIM_WARNING_WARNING
} SimWarningLevel;

typedef enum SimSnapshotSource
{
    SIM_SNAPSHOT_SOURCE_NONE = 0,  // 无数据
    SIM_SNAPSHOT_SOURCE_DATA_FILES, // 本地样本数据
    SIM_SNAPSHOT_SOURCE_XPLANE      // X-Plane 实时数据
} SimSnapshotSource;

typedef enum SimFlightPhase
{
    SIM_FLIGHT_PHASE_UNKNOWN = 0,
    SIM_FLIGHT_PHASE_GROUND,
    SIM_FLIGHT_PHASE_TAKEOFF,
    SIM_FLIGHT_PHASE_CLIMB,
    SIM_FLIGHT_PHASE_CRUISE,
    SIM_FLIGHT_PHASE_DESCENT,
    SIM_FLIGHT_PHASE_LANDING,
    SIM_FLIGHT_PHASE_EMERGENCY
} SimFlightPhase;

typedef struct SimWarning
{
    char text[SIM_SNAPSHOT_WARNING_TEXT_LEN];
    SimWarningLevel level;
    int active;
} SimWarning;

typedef struct SimSnapshot
{
    // 快照状态信息
    float sim_time;
    float delta_time;
    float playback_speed;
    int current_frame;
    int frame_id;
    float timestamp;
    float last_valid_timestamp;
    int timed_out;
    int xplane_connected;
    int fallback_active;
    float last_valid_xplane_timestamp;
    int data_valid;
    SimSnapshotSource source;
    SimFlightPhase flight_phase;
    int updated_frame;

    // 各显示模块数据有效标志
    int has_pfd;
    int has_nd;
    int has_eicas_upper;
    int has_eicas_lower;

    int pfd_frame_index;
    int nd_frame_index;
    int eicas_upper_frame_index;
    int eicas_lower_frame_index;

    // PFD 主飞行显示数据
    float pitch;
    float roll;
    float yaw;
    float heading;
    float heading_target;
    float altitude;
    float altitude_target;
    float agl_altitude;
    float airspeed;
    float airspeed_target;
    float vertical_speed;
    float throttle;

    // ND 导航显示数据
    double latitude;
    double longitude;
    float track;
    float ground_speed;
    float true_air_speed;

    // EICAS 发动机与系统显示数据
    float total_air_temperature;
    float n1_left;
    float n1_right;
    int engine_left_running;
    int engine_right_running;
    float n2_left;
    float n2_right;
    float egt_left;
    float egt_right;
    float fuel_flow_left;
    float fuel_flow_right;
    float lower_fuel_flow_left;
    float lower_fuel_flow_right;
    float oil_pressure_left;
    float oil_pressure_right;
    float oil_temperature_left;
    float oil_temperature_right;
    float oil_quantity_left;
    float oil_quantity_right;
    float vibration_left;
    float vibration_right;
    float fuel_quantity;
    float fuel_center_quantity;
    float fuel_left_quantity;
    float fuel_right_quantity;
    float hydraulic_pressure;
    float cabin_pressure;
    float battery_voltage;

    int gear_down;
    int flaps_level;
    int parking_brake_on;

    /* X-Plane 原生报警状态（通过 getDREF 从 sim/cockpit/warnings/* 读取） */
    int xplane_master_warning;
    int xplane_master_caution;
    int xplane_engine_fire;
    int xplane_stall_warning;
    int xplane_overspeed_warning;

    SimWarning warnings[SIM_SNAPSHOT_MAX_WARNINGS];
    int warning_count;
} SimSnapshot;

#endif
