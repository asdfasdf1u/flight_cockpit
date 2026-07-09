#ifndef EICAS_DATA_H
#define EICAS_DATA_H

#include "../Systems/aircraft_systems_data.h"

#define EICAS_DATA_MAX_FRAMES 4096
#define EICAS_DATA_DEFAULT_STEP_SEC (1.0f / 30.0f)
#define EICAS_DATA_MAX_WARNINGS 8
#define EICAS_DATA_WARNING_TEXT_LEN 64

typedef enum EICAS_WarningLevel
{
    EICAS_WARNING_INFO,
    EICAS_WARNING_CAUTION,
    EICAS_WARNING_WARNING
} EICAS_WarningLevel;

typedef struct EICAS_EngineData
{
    float n1;
    float n2;
    float egt;
    float fuel_flow;
    float oil_pressure;
    float oil_temperature;
    float oil_quantity;
    float vibration;
    int running;
} EICAS_EngineData;

typedef struct EICAS_WarningItem
{
    char text[EICAS_DATA_WARNING_TEXT_LEN];
    EICAS_WarningLevel level;
    int active;
} EICAS_WarningItem;

typedef struct EICAS1_DataFrame
{
    float tat;
    float n1_left;
    float n1_right;
    float egt_left;
    float egt_right;
    float fuel_flow_left_display;
    float fuel_flow_right_display;
    float fuel_center_quantity;
    float fuel_left_quantity;
    float fuel_right_quantity;
} EICAS1_DataFrame;

typedef struct EICAS2_DataFrame
{
    float n2_left;
    float n2_right;
    float fuel_flow_left_display;
    float fuel_flow_right_display;
    float oil_pressure_left;
    float oil_pressure_right;
    float oil_temperature_left;
    float oil_temperature_right;
    float oil_quantity_left;
    float oil_quantity_right;
    float vibration_left;
    float vibration_right;
} EICAS2_DataFrame;

typedef struct EICAS_Data
{
    EICAS_EngineData engine_left;
    EICAS_EngineData engine_right;

    float tat;
    float fuel_quantity;
    float fuel_center_quantity;
    float fuel_left_quantity;
    float fuel_right_quantity;
    float fuel_total_quantity;
    float hydraulic_pressure;
    float cabin_pressure;
    float battery_voltage;

    int gear_down;
    int flaps_level;
    int parking_brake_on;

    EICAS_WarningItem warnings[EICAS_DATA_MAX_WARNINGS];
    int warning_count;
    float simulation_time;

    EICAS1_DataFrame upper_frames[EICAS_DATA_MAX_FRAMES];
    int upper_frame_count;
    int upper_frame_index;
    int upper_loaded;

    EICAS2_DataFrame lower_frames[EICAS_DATA_MAX_FRAMES];
    int lower_frame_count;
    int lower_frame_index;
    int lower_loaded;

    float frame_elapsed;
    float frame_step_sec;
} EICAS_Data;

void eicas_data_init(EICAS_Data *data);
void eicas_data_update_mock(EICAS_Data *data, float delta_time);
int eicas_data_load_upper_file(EICAS_Data *data, const char *path);
int eicas_data_load_lower_file(EICAS_Data *data, const char *path);
int eicas_data_load_files(EICAS_Data *data, const char *upper_path, const char *lower_path);
void eicas_data_update(EICAS_Data *data, float delta_time);

const EICAS1_DataFrame *eicas_data_current_upper_frame(const EICAS_Data *data);
const EICAS2_DataFrame *eicas_data_current_lower_frame(const EICAS_Data *data);

void eicas_data_apply_upper_to_aircraft_systems(const EICAS_Data *data, AircraftSystems_Data *systems);
void eicas_data_apply_lower_to_aircraft_systems(const EICAS_Data *data, AircraftSystems_Data *systems);
void eicas_data_apply_to_aircraft_systems(const EICAS_Data *data, AircraftSystems_Data *systems);

#endif
