#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cabin_data.h"
#include "../Data/sim_data_center.h"

static void assert_snapshot_consistency(const Cabin_Data *cabin, const SimSnapshot *snapshot)
{
    assert(cabin != NULL);
    assert(snapshot != NULL);
    assert(cabin->snapshot_valid == snapshot->data_valid);
    assert(fabsf(cabin->altitude - snapshot->altitude) < 0.001f);
    assert(fabsf(cabin->ground_speed - snapshot->ground_speed) < 0.001f);
    assert(fabsf(cabin->vertical_speed - snapshot->vertical_speed) < 0.001f);
    assert(fabsf(cabin->heading - snapshot->heading) < 0.001f);
    assert(fabs(cabin->latitude - snapshot->latitude) < 0.000001);
    assert(fabs(cabin->longitude - snapshot->longitude) < 0.000001);
}

int main(void)
{
    SimDataCenter *center = (SimDataCenter *)malloc(sizeof(*center));
    Cabin_Data cabin;
    SimPlannedRoute changed_route;

    assert(center != NULL);
    assert(sim_data_center_init(center));
    sim_data_center_update(center, 0.033f);
    cabin_data_init(&cabin);
    assert(cabin_data_apply_sim_data_center(&cabin, center, 0.033f) & CABIN_DATA_UPDATE_ROUTE);
    assert_snapshot_consistency(&cabin, sim_data_center_snapshot(center));
    assert(cabin.route_valid);
    assert(strcmp(cabin.origin_airport, sim_data_center_route(center)->origin) == 0);
    assert(strcmp(cabin.destination_airport, sim_data_center_route(center)->destination) == 0);

    for (int i = 0; i < 10; ++i)
    {
        sim_data_center_update(center, 0.033f);
        cabin_data_apply_sim_data_center(&cabin, center, 0.033f);
    }
    assert(strcmp(cabin.flight_phase, "UNKNOWN") != 0);

    changed_route = *sim_data_center_route(center);
    snprintf(changed_route.origin, sizeof(changed_route.origin), "%s", "TEST_ORIGIN");
    snprintf(changed_route.destination, sizeof(changed_route.destination), "%s", "TEST_DEST");
    sim_data_center_set_route(center, &changed_route);
    assert(cabin_data_apply_sim_data_center(&cabin, center, 0.033f) & CABIN_DATA_UPDATE_ROUTE);
    assert(strcmp(cabin.origin_airport, "TEST_ORIGIN") == 0);
    assert(strcmp(cabin.destination_airport, "TEST_DEST") == 0);

    sim_data_center_clear_route(center);
    assert(cabin_data_apply_sim_data_center(&cabin, center, 0.033f) & CABIN_DATA_UPDATE_ROUTE);
    assert(!cabin.route_valid);
    assert(cabin.planned_route_count == 0);
    assert(strcmp(cabin.origin_airport, "----") == 0);

    SimDataCenter *unavailable_center = (SimDataCenter *)calloc(1, sizeof(*unavailable_center));
    assert(unavailable_center != NULL);
    assert(cabin_data_apply_sim_data_center(&cabin, unavailable_center, 0.033f) & CABIN_DATA_UPDATE_VALIDITY);
    assert(!cabin.snapshot_valid);
    assert(cabin.altitude == 0.0f);
    assert(strcmp(cabin.flight_phase, "UNKNOWN") == 0);
    free(unavailable_center);

    for (int i = 0; i < 3; ++i)
    {
        cabin_data_init(&cabin);
        assert(cabin_data_apply_sim_data_center(&cabin, center, 0.033f) != CABIN_DATA_UPDATE_NONE);
    }

    sim_data_center_destroy(center);
    free(center);
    printf("Cabin data adapter tests passed.\n");
    return 0;
}
