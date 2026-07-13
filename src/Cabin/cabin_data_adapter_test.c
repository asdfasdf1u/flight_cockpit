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

static SimPlannedRoute domestic_test_route(void)
{
    SimPlannedRoute route;
    memset(&route, 0, sizeof(route));
    route.valid = 1;
    route.source = SIM_ROUTE_SOURCE_FMC_FALLBACK;
    route.has_coordinates = 1;
    route.active_waypoint_index = 1;
    snprintf(route.origin, sizeof(route.origin), "%s", "ZBAA");
    snprintf(route.destination, sizeof(route.destination), "%s", "ZSPD");
    route.point_count = 3;
    snprintf(route.points[0].ident, sizeof(route.points[0].ident), "%s", "ZBAA");
    snprintf(route.points[1].ident, sizeof(route.points[1].ident), "%s", "HFE");
    snprintf(route.points[2].ident, sizeof(route.points[2].ident), "%s", "ZSPD");
    for (int i = 0; i < route.point_count; ++i)
    {
        snprintf(route.points[i].type, sizeof(route.points[i].type), "%s", i == 1 ? "FIX" : "AIRPORT");
        snprintf(route.points[i].coordinate_source, sizeof(route.points[i].coordinate_source), "%s", "ROUTE");
        route.points[i].has_position = 1;
    }
    route.points[0].latitude = 40.080111;
    route.points[0].longitude = 116.584556;
    route.points[1].latitude = 31.988900;
    route.points[1].longitude = 116.978900;
    route.points[2].latitude = 31.143400;
    route.points[2].longitude = 121.805200;
    return route;
}

int main(void)
{
    SimDataCenter *center = (SimDataCenter *)malloc(sizeof(*center));
    Cabin_Data cabin;
    SimPlannedRoute changed_route;

    assert(center != NULL);
    assert(sim_data_center_init(center));
    changed_route = domestic_test_route();
    sim_data_center_set_route(center, &changed_route);
    sim_data_center_update(center, 0.033f);
    cabin_data_init(&cabin);
    assert(cabin_data_apply_sim_data_center(&cabin, center, 0.033f) & CABIN_DATA_UPDATE_ROUTE);
    assert_snapshot_consistency(&cabin, sim_data_center_snapshot(center));
    assert(cabin.route_valid);
    assert(strcmp(cabin.origin_airport, sim_data_center_route(center)->origin) == 0);
    assert(strcmp(cabin.destination_airport, sim_data_center_route(center)->destination) == 0);
    assert(fabs(cabin.origin_lat - changed_route.points[0].latitude) < 0.000001);
    assert(fabs(cabin.origin_lon - changed_route.points[0].longitude) < 0.000001);
    assert(fabs(cabin.destination_lat - changed_route.points[2].latitude) < 0.000001);
    assert(fabs(cabin.destination_lon - changed_route.points[2].longitude) < 0.000001);
    const int initial_revision = sim_data_center_route_revision(center);
    const float initial_time = sim_data_center_snapshot(center)->sim_time;
    const double initial_latitude = cabin.latitude;
    const double initial_longitude = cabin.longitude;

    for (int i = 0; i < 40; ++i)
    {
        sim_data_center_update(center, 0.033f);
        cabin_data_apply_sim_data_center(&cabin, center, 0.033f);
        assert(sim_data_center_route_revision(center) == initial_revision);
    }
    assert(sim_data_center_snapshot(center)->sim_time > initial_time);
    assert(cabin.latitude != initial_latitude || cabin.longitude != initial_longitude);
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
    assert(cabin.map_top_left_lat > cabin.map_bottom_right_lat);
    assert(cabin.map_bottom_right_lon > cabin.map_top_left_lon);
    assert(cabin.latitude <= cabin.map_top_left_lat && cabin.latitude >= cabin.map_bottom_right_lat);
    assert(cabin.longitude >= cabin.map_top_left_lon && cabin.longitude <= cabin.map_bottom_right_lon);

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
