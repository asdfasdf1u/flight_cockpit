#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "alert_manager.h"

static SimSnapshot valid_snapshot(void)
{
    SimSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.data_valid = 1;
    snapshot.sim_time = 12.0f;
    snapshot.cabin_pressure = 8.2f;
    return snapshot;
}

int main(void)
{
    AlertManager manager;
    SimSnapshot snapshot = valid_snapshot();

    alert_manager_init(&manager);
    alert_manager_update(&manager, &snapshot);
    assert(alert_manager_snapshot(&manager)->active_count == 0);

    alert_manager_set_demo(&manager, ALERT_TYPE_ENGINE_FIRE, 1);
    alert_manager_update(&manager, &snapshot);
    assert(alert_snapshot_find(alert_manager_snapshot(&manager), ALERT_TYPE_ENGINE_FIRE)->active);
    alert_manager_acknowledge(&manager, ALERT_TYPE_ENGINE_FIRE);
    assert(alert_snapshot_find(alert_manager_snapshot(&manager), ALERT_TYPE_ENGINE_FIRE)->acknowledged);
    alert_manager_update(&manager, &snapshot);
    assert(alert_snapshot_find(alert_manager_snapshot(&manager), ALERT_TYPE_ENGINE_FIRE)->active);

    alert_manager_set_demo(&manager, ALERT_TYPE_CABIN_ALTITUDE, 1);
    alert_manager_update(&manager, &snapshot);
    assert(alert_manager_snapshot(&manager)->active_count >= 3); /* fire, cabin altitude, emergency */

    alert_manager_set_demo(&manager, ALERT_TYPE_CRASH, 1);
    alert_manager_update(&manager, &snapshot);
    assert(alert_snapshot_find(alert_manager_snapshot(&manager), ALERT_TYPE_CRASH)->active);
    alert_manager_clear_demo(&manager);
    alert_manager_update(&manager, &snapshot);
    assert(!alert_snapshot_find(alert_manager_snapshot(&manager), ALERT_TYPE_CRASH)->active);
    assert(!alert_snapshot_find(alert_manager_snapshot(&manager), ALERT_TYPE_ENGINE_FIRE)->active);

    snapshot.data_valid = 0;
    alert_manager_update(&manager, &snapshot);
    assert(alert_snapshot_find(alert_manager_snapshot(&manager), ALERT_TYPE_DATA_LOST)->active);
    assert(!alert_snapshot_find(alert_manager_snapshot(&manager), ALERT_TYPE_CRASH)->active);

    printf("Alert manager tests passed.\n");
    return 0;
}
