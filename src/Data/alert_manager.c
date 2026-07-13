#include "alert_manager.h"

#include <stdio.h>
#include <string.h>

static void configure_alert(AlertState *alert, AlertType type, AlertLevel level, const char *message, int needs_audio, int latched, int auto_clear)
{
    memset(alert, 0, sizeof(*alert));
    alert->type = type;
    alert->level = level;
    alert->needs_audio = needs_audio;
    alert->latched = latched;
    alert->auto_clear = auto_clear;
    snprintf(alert->message, sizeof(alert->message), "%s", message);
    snprintf(alert->source, sizeof(alert->source), "%s", "NONE");
}

const char *alert_type_name(AlertType type)
{
    static const char *const names[ALERT_MANAGER_MAX_ALERTS] = {
        "ENGINE_FIRE", "CABIN_ALTITUDE", "DOOR_OPEN", "EMERGENCY", "CRASH", "SEATBELT_ON", "DATA_LOST"};
    return type >= 0 && type < ALERT_MANAGER_MAX_ALERTS ? names[type] : "UNKNOWN";
}

const char *alert_level_name(AlertLevel level)
{
    switch (level)
    {
    case ALERT_LEVEL_WARNING: return "WARNING";
    case ALERT_LEVEL_CAUTION: return "CAUTION";
    case ALERT_LEVEL_ADVISORY:
    default: return "ADVISORY";
    }
}

void alert_manager_init(AlertManager *manager)
{
    if (manager == NULL)
    {
        return;
    }
    memset(manager, 0, sizeof(*manager));
    configure_alert(&manager->snapshot.alerts[ALERT_TYPE_ENGINE_FIRE], ALERT_TYPE_ENGINE_FIRE, ALERT_LEVEL_WARNING, "ENGINE FIRE", 1, 1, 1);
    configure_alert(&manager->snapshot.alerts[ALERT_TYPE_CABIN_ALTITUDE], ALERT_TYPE_CABIN_ALTITUDE, ALERT_LEVEL_WARNING, "CABIN ALTITUDE", 1, 1, 1);
    configure_alert(&manager->snapshot.alerts[ALERT_TYPE_DOOR_OPEN], ALERT_TYPE_DOOR_OPEN, ALERT_LEVEL_WARNING, "DOOR OPEN", 1, 1, 1);
    configure_alert(&manager->snapshot.alerts[ALERT_TYPE_EMERGENCY], ALERT_TYPE_EMERGENCY, ALERT_LEVEL_WARNING, "EMERGENCY", 1, 1, 1);
    configure_alert(&manager->snapshot.alerts[ALERT_TYPE_CRASH], ALERT_TYPE_CRASH, ALERT_LEVEL_WARNING, "CRASH", 1, 1, 0);
    configure_alert(&manager->snapshot.alerts[ALERT_TYPE_SEATBELT_ON], ALERT_TYPE_SEATBELT_ON, ALERT_LEVEL_ADVISORY, "SEATBELT ON", 0, 0, 1);
    configure_alert(&manager->snapshot.alerts[ALERT_TYPE_DATA_LOST], ALERT_TYPE_DATA_LOST, ALERT_LEVEL_CAUTION, "FLIGHT DATA LOST", 1, 0, 1);
}

static void set_condition(AlertManager *manager, AlertType type, int condition, const char *source, float now)
{
    AlertState *alert = &manager->snapshot.alerts[type];
    const int was_active = alert->active;
    const int should_remain_active = condition || (alert->latched && !alert->auto_clear && was_active);

    if (should_remain_active)
    {
        alert->active = 1;
        if (!was_active)
        {
            alert->acknowledged = 0;
            alert->start_time = now;
        }
        snprintf(alert->source, sizeof(alert->source), "%s", source != NULL ? source : "SIM");
    }
    else
    {
        alert->active = 0;
        alert->acknowledged = 0;
        snprintf(alert->source, sizeof(alert->source), "%s", "NONE");
    }
}

void alert_manager_update(AlertManager *manager, const SimSnapshot *snapshot)
{
    int emergency_condition = 0;
    int active_count = 0;
    float now = snapshot != NULL ? snapshot->sim_time : 0.0f;

    if (manager == NULL)
    {
        return;
    }
    if (snapshot != NULL)
    {
        for (int i = 0; i < snapshot->warning_count; ++i)
        {
            emergency_condition |= snapshot->warnings[i].active && snapshot->warnings[i].level == SIM_WARNING_WARNING;
        }
    }

    set_condition(manager, ALERT_TYPE_ENGINE_FIRE,
                  manager->demo_engine_fire || (snapshot != NULL && (snapshot->egt_left > 900.0f || snapshot->egt_right > 900.0f)),
                  manager->demo_engine_fire ? "DEMO" : "SIM_ENGINE", now);
    set_condition(manager, ALERT_TYPE_CABIN_ALTITUDE,
                  manager->demo_cabin_altitude || (snapshot != NULL && snapshot->cabin_pressure < 6.0f),
                  manager->demo_cabin_altitude ? "DEMO" : "SIM_CABIN", now);
    set_condition(manager, ALERT_TYPE_DOOR_OPEN, 0, "UNAVAILABLE", now);
    set_condition(manager, ALERT_TYPE_CRASH, manager->demo_crash, "DEMO", now);
    set_condition(manager, ALERT_TYPE_EMERGENCY,
                  emergency_condition || manager->snapshot.alerts[ALERT_TYPE_ENGINE_FIRE].active || manager->snapshot.alerts[ALERT_TYPE_CRASH].active,
                  "SIM_ALERT", now);
    set_condition(manager, ALERT_TYPE_SEATBELT_ON, 0, "UNAVAILABLE", now);
    set_condition(manager, ALERT_TYPE_DATA_LOST, snapshot == NULL || !snapshot->data_valid, "SIM_SNAPSHOT", now);

    for (int i = 0; i < ALERT_MANAGER_MAX_ALERTS; ++i)
    {
        active_count += manager->snapshot.alerts[i].active;
    }
    if (active_count != manager->snapshot.active_count)
    {
        manager->snapshot.revision++;
    }
    manager->snapshot.active_count = active_count;
}

void alert_manager_acknowledge(AlertManager *manager, AlertType type)
{
    if (manager != NULL && type >= 0 && type < ALERT_MANAGER_MAX_ALERTS && manager->snapshot.alerts[type].active)
    {
        manager->snapshot.alerts[type].acknowledged = 1;
        manager->snapshot.revision++;
    }
}

void alert_manager_set_demo(AlertManager *manager, AlertType type, int active)
{
    if (manager == NULL)
    {
        return;
    }
    if (type == ALERT_TYPE_ENGINE_FIRE) manager->demo_engine_fire = active != 0;
    if (type == ALERT_TYPE_CABIN_ALTITUDE) manager->demo_cabin_altitude = active != 0;
    if (type == ALERT_TYPE_CRASH) manager->demo_crash = active != 0;
}

void alert_manager_clear_demo(AlertManager *manager)
{
    if (manager == NULL)
    {
        return;
    }
    manager->demo_engine_fire = 0;
    manager->demo_cabin_altitude = 0;
    manager->demo_crash = 0;
    manager->snapshot.alerts[ALERT_TYPE_CRASH].active = 0;
    manager->snapshot.alerts[ALERT_TYPE_CRASH].acknowledged = 0;
}

const AlertSnapshot *alert_manager_snapshot(const AlertManager *manager)
{
    return manager != NULL ? &manager->snapshot : NULL;
}

const AlertState *alert_snapshot_find(const AlertSnapshot *snapshot, AlertType type)
{
    return snapshot != NULL && type >= 0 && type < ALERT_MANAGER_MAX_ALERTS ? &snapshot->alerts[type] : NULL;
}

void alert_manager_append_sim_warnings(const AlertSnapshot *alerts, SimSnapshot *snapshot)
{
    if (alerts == NULL || snapshot == NULL)
    {
        return;
    }
    for (int i = 0; i < ALERT_MANAGER_MAX_ALERTS && snapshot->warning_count < SIM_SNAPSHOT_MAX_WARNINGS; ++i)
    {
        const AlertState *alert = &alerts->alerts[i];
        SimWarning *warning;
        if (!alert->active)
        {
            continue;
        }
        warning = &snapshot->warnings[snapshot->warning_count++];
        snprintf(warning->text, sizeof(warning->text), "%.*s", (int)sizeof(warning->text) - 1, alert->message);
        warning->level = alert->level == ALERT_LEVEL_WARNING ? SIM_WARNING_WARNING :
                         (alert->level == ALERT_LEVEL_CAUTION ? SIM_WARNING_CAUTION : SIM_WARNING_INFO);
        warning->active = 1;
    }
}
