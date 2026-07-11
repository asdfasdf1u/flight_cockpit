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

typedef struct SimWarning
{
    char text[SIM_SNAPSHOT_WARNING_TEXT_LEN];
    SimWarningLevel level;
    int active;
} SimWarning;

typedef struct SimSnapshot
{
    float sim_time;
    float delta_time;
    float playback_speed;
    int current_frame;

    int has_pfd;
    int has_nd;
    int has_eicas_upper;
    int has_eicas_lower;

    int pfd_frame_index;
    int nd_frame_index;
    int eicas_upper_frame_index;
    int eicas_lower_frame_index;

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

    double latitude;
    double longitude;
    float track;
    float ground_speed;
    float true_air_speed;

    float total_air_temperature;
    float n1_left;
    float n1_right;
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

    SimWarning warnings[SIM_SNAPSHOT_MAX_WARNINGS];
    int warning_count;
} SimSnapshot;

#endif
