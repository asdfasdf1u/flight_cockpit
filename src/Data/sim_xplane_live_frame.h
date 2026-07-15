#ifndef SIM_XPLANE_LIVE_FRAME_H
#define SIM_XPLANE_LIVE_FRAME_H

typedef struct SimXPlaneLiveFrame
{
    int valid;
    int connected;
    int timed_out;
    int frame_id;
    float timestamp;
    float delta_time;
    float last_valid_timestamp;

    double latitude;
    double longitude;
    float altitude;
    float agl_altitude;
    float airspeed;
    float airspeed_target;
    float true_air_speed;
    float ground_speed;
    float heading;
    float heading_target;
    float altitude_target;
    float track;
    float pitch;
    float roll;
    float yaw;
    float vertical_speed;
    float throttle;

    float total_air_temperature;
    float n1_left;
    float n1_right;
    float n2_left;
    float n2_right;
    float egt_left;
    float egt_right;
    float fuel_flow_left;
    float fuel_flow_right;
    float oil_pressure_left;
    float oil_pressure_right;
    float oil_temperature_left;
    float oil_temperature_right;
    float oil_quantity_left;
    float oil_quantity_right;
    float vibration_left;
    float vibration_right;
    int engine_left_running;
    int engine_right_running;

    float fuel_quantity;
    float fuel_left_quantity;
    float fuel_center_quantity;
    float fuel_right_quantity;
    int fuel_tank_quantities_valid;

    int gear_down;
    int flaps_level;
    int parking_brake_on;
    int has_gear;
    int has_flaps;
    int has_parking_brake;

    /* X-Plane 原生报警状态（通过 getDREF 读取 sim/cockpit/warnings/ 下的数据引用）。 */
    int xplane_master_warning;
    int xplane_master_caution;
    int xplane_engine_fire;
    int xplane_stall_warning;
    int xplane_overspeed_warning;
} SimXPlaneLiveFrame;

#endif
