#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include "sim_snapshot.h"

#define ALERT_MANAGER_MAX_ALERTS 7
#define ALERT_MANAGER_TEXT_LEN 96

typedef enum AlertType
{
    ALERT_TYPE_ENGINE_FIRE = 0,
    ALERT_TYPE_CABIN_ALTITUDE,
    ALERT_TYPE_DOOR_OPEN,
    ALERT_TYPE_EMERGENCY,
    ALERT_TYPE_CRASH,
    ALERT_TYPE_SEATBELT_ON,
    ALERT_TYPE_DATA_LOST
} AlertType;

typedef enum AlertLevel
{
    ALERT_LEVEL_ADVISORY = 0,
    ALERT_LEVEL_CAUTION,
    ALERT_LEVEL_WARNING
} AlertLevel;

typedef struct AlertState
{
    AlertType type;
    AlertLevel level;
    int active;
    int acknowledged;
    char source[ALERT_MANAGER_TEXT_LEN];
    float start_time;
    char message[ALERT_MANAGER_TEXT_LEN];
    int needs_audio;
    int latched;
    int auto_clear;
} AlertState;

typedef struct AlertSnapshot
{
    int revision;
    int active_count;
    AlertState alerts[ALERT_MANAGER_MAX_ALERTS];
} AlertSnapshot;

typedef struct AlertManager
{
    AlertSnapshot snapshot;
    int demo_engine_fire;
    int demo_cabin_altitude;
    int demo_crash;
} AlertManager;

void alert_manager_init(AlertManager *manager);
void alert_manager_update(AlertManager *manager, const SimSnapshot *snapshot);
void alert_manager_acknowledge(AlertManager *manager, AlertType type);
void alert_manager_set_demo(AlertManager *manager, AlertType type, int active);
void alert_manager_clear_demo(AlertManager *manager);
const AlertSnapshot *alert_manager_snapshot(const AlertManager *manager);
const AlertState *alert_snapshot_find(const AlertSnapshot *snapshot, AlertType type);
const char *alert_type_name(AlertType type);
const char *alert_level_name(AlertLevel level);
void alert_manager_append_sim_warnings(const AlertSnapshot *alerts, SimSnapshot *snapshot);

#endif
