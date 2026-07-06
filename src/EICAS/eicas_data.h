#ifndef EICAS_DATA_H
#define EICAS_DATA_H

#define EICAS_MAX_WARNINGS 8
#define EICAS_WARNING_TEXT_LEN 64

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
    float oil_temp;
    int running;
} EICAS_EngineData;

typedef struct EICAS_WarningItem
{
    char text[EICAS_WARNING_TEXT_LEN];
    EICAS_WarningLevel level;
    int active;
} EICAS_WarningItem;

typedef struct EICAS_Data
{
    EICAS_EngineData engine_left;
    EICAS_EngineData engine_right;

    float fuel_quantity;
    float hydraulic_pressure;
    float cabin_pressure;
    float battery_voltage;

    int gear_down;
    int flaps_level;
    int parking_brake_on;

    EICAS_WarningItem warnings[EICAS_MAX_WARNINGS];
    int warning_count;
    float simulation_time;
} EICAS_Data;

void eicas_data_init(EICAS_Data *data);
void eicas_data_update_mock(EICAS_Data *data, float delta_time);

#endif
