#ifndef AIRCRAFT_SYSTEMS_DATA_H
#define AIRCRAFT_SYSTEMS_DATA_H

#define AIRCRAFT_SYSTEMS_MAX_WARNINGS 8
#define AIRCRAFT_SYSTEMS_WARNING_TEXT_LEN 64

typedef enum AircraftSystems_WarningLevel
{
    AIRCRAFT_SYSTEMS_WARNING_INFO,
    AIRCRAFT_SYSTEMS_WARNING_CAUTION,
    AIRCRAFT_SYSTEMS_WARNING_WARNING
} AircraftSystems_WarningLevel;

typedef struct AircraftSystems_EngineData
{
    float n1;
    float n2;
    float egt;
    float fuel_flow;
    float eicas1_fuel_flow_display;
    int eicas1_fuel_flow_display_valid;
    float eicas2_fuel_flow_display;
    int eicas2_fuel_flow_display_valid;
    float oil_pressure;
    float oil_temp;
    float oil_quantity;
    float vibration;
    int running;
} AircraftSystems_EngineData;

typedef struct AircraftSystems_WarningItem
{
    char text[AIRCRAFT_SYSTEMS_WARNING_TEXT_LEN];
    AircraftSystems_WarningLevel level;
    int active;
} AircraftSystems_WarningItem;

typedef struct AircraftSystems_Data
{
    AircraftSystems_EngineData engine_left;
    AircraftSystems_EngineData engine_right;

    float total_air_temperature;
    float fuel_quantity;
    float fuel_left_quantity;
    float fuel_center_quantity;
    float fuel_right_quantity;
    float fuel_total_quantity;
    int fuel_tank_quantities_valid;
    float hydraulic_pressure;
    float cabin_pressure;
    float battery_voltage;

    int gear_down;
    int flaps_level;
    int parking_brake_on;

    AircraftSystems_WarningItem warnings[AIRCRAFT_SYSTEMS_MAX_WARNINGS];
    int warning_count;
    float simulation_time;
    int snapshot_frame_id;
    int data_valid;
} AircraftSystems_Data;

void aircraft_systems_data_init(AircraftSystems_Data *data);
void aircraft_systems_data_update_mock(AircraftSystems_Data *data, float delta_time);

#endif

