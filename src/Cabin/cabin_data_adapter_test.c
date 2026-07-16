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

static void apply_live_position(SimDataCenter *center,
                                Cabin_Data *cabin,
                                int frame_id,
                                double latitude,
                                double longitude,
                                int connected,
                                int timed_out)
{
    SimXPlaneLiveFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.valid = connected && !timed_out;
    frame.connected = connected;
    frame.timed_out = timed_out;
    frame.frame_id = frame_id;
    frame.timestamp = (float)frame_id * 1.3f;
    frame.delta_time = 1.3f;
    frame.last_valid_timestamp = frame.valid ? frame.timestamp : frame.timestamp - 1.3f;
    frame.latitude = latitude;
    frame.longitude = longitude;
    frame.altitude = 10000.0f;
    frame.ground_speed = 300.0f;
    frame.true_air_speed = 320.0f;
    frame.heading = 90.0f;
    frame.track = 90.0f;
    (void)sim_data_center_apply_xplane_live_frame(center, &frame);
    cabin_data_apply_sim_data_center(cabin, center, frame.delta_time);
}

static void assert_place_display_fallbacks(void)
{
    Cabin_Place place;

    memset(&place, 0, sizeof(place));
    place.status = CABIN_PLACE_VALID;
    snprintf(place.province, sizeof(place.province), "%s", "北京市");
    snprintf(place.district, sizeof(place.district), "%s", "顺义区");
    assert(strcmp(cabin_place_display_name(&place), "北京市") == 0);

    snprintf(place.city, sizeof(place.city), "%s", "成都市");
    assert(strcmp(cabin_place_display_name(&place), "成都市") == 0);

    place.city[0] = '\0';
    place.province[0] = '\0';
    assert(strcmp(cabin_place_display_name(&place), "顺义区") == 0);

    snprintf(place.township, sizeof(place.township), "%s", "南法信镇");
    snprintf(place.street, sizeof(place.street), "%s", "顺平路");
    snprintf(place.formatted_address, sizeof(place.formatted_address), "%s", "北京市顺义区顺平路");
    assert(strcmp(cabin_place_street_or_town(&place), "南法信镇") == 0);
    place.township[0] = '\0';
    assert(strcmp(cabin_place_street_or_town(&place), "顺平路") == 0);
    place.street[0] = '\0';
    assert(strcmp(cabin_place_street_or_town(&place), "北京市顺义区顺平路") == 0);
    place.formatted_address[0] = '\0';
    assert(cabin_place_street_or_town(&place)[0] == '\0');

    place.status = CABIN_PLACE_FAILED;
    assert(cabin_place_display_name(&place)[0] == '\0');
    assert(cabin_place_street_or_town(&place)[0] == '\0');
}

int main(void)
{
    SimDataCenter *center = (SimDataCenter *)malloc(sizeof(*center));
    Cabin_Data cabin;
    SimPlannedRoute changed_route;

    assert(center != NULL);
    assert_place_display_fallbacks();
    assert(sim_data_center_init(center));
    changed_route = domestic_test_route();
    sim_data_center_set_route(center, &changed_route);
    sim_data_center_update(center, 0.033f);
    cabin_data_init(&cabin);
    assert(cabin.flown_track_count == 1);
    assert(cabin.flown_track_seed_is_default);
    assert(!cabin.flown_track_has_real_point);
    assert(fabs(cabin.flown_track[0].latitude - 36.07) < 0.000001);
    assert(fabs(cabin.flown_track[0].longitude - 120.38) < 0.000001);
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
    assert(cabin.flown_track_count == 1);
    assert(cabin.flown_track_seed_is_default);

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
    assert(cabin.flown_track_count == 1);
    assert(cabin.flown_track_seed_is_default);
    assert(strcmp(cabin.origin_airport, "----") == 0);
    assert(cabin.map_top_left_lat > cabin.map_bottom_right_lat);
    assert(cabin.map_bottom_right_lon > cabin.map_top_left_lon);
    assert(cabin.latitude <= cabin.map_top_left_lat && cabin.latitude >= cabin.map_bottom_right_lat);
    assert(cabin.longitude >= cabin.map_top_left_lon && cabin.longitude <= cabin.map_bottom_right_lon);
    assert(cabin.map_zoom >= cabin.map_min_zoom && cabin.map_zoom <= cabin.map_max_zoom);
    assert(strstr(cabin.map_cache_path, "_c") != NULL);
    assert(strstr(cabin.map_cache_path, "_z10_1024x576.png") != NULL);

    /* The first stable X-Plane point replaces the Qingdao seed even without an FMC route. */
    apply_live_position(center, &cabin, 1, 30.0000, 120.0000, 1, 0);
    apply_live_position(center, &cabin, 2, 30.0000, 120.0000, 1, 0);
    apply_live_position(center, &cabin, 3, 30.0000, 120.0000, 1, 0);
    assert(!cabin.route_valid);
    assert(cabin.flown_track_count == 1);
    assert(!cabin.flown_track_seed_is_default);
    assert(cabin.flown_track_has_real_point);
    assert(fabs(cabin.flown_track[0].latitude - 30.0) < 0.000001);
    assert(fabs(cabin.flown_track[0].longitude - 120.0) < 0.000001);

    /* More than 40 samples use strict FIFO and retain the newest 40 points. */
    for (int i = 1; i <= 45; ++i)
    {
        apply_live_position(center, &cabin, 3 + i, 30.0 + i * 0.01, 120.0 + i * 0.01, 1, 0);
        assert(cabin.flown_track_count <= CABIN_FLOWN_TRACK_MAX_POINTS);
    }
    assert(cabin.flown_track_count == CABIN_FLOWN_TRACK_MAX_POINTS);
    assert(fabs(cabin.flown_track[0].latitude - 30.06) < 0.000001);
    assert(fabs(cabin.flown_track[39].latitude - 30.45) < 0.000001);
    assert(cabin.flown_track[0].sequence == 6);
    assert(cabin.flown_track[39].sequence == 45);

    const int count_before_disconnect = cabin.flown_track_count;
    const double last_before_disconnect = cabin.flown_track[count_before_disconnect - 1].latitude;
    apply_live_position(center, &cabin, 49, 0.0, 0.0, 0, 0);
    assert(cabin.flown_track_count == count_before_disconnect);
    assert(fabs(cabin.flown_track[count_before_disconnect - 1].latitude - last_before_disconnect) < 0.000001);

    /* Recovery requires stable frames; only the stable live position is appended. */
    apply_live_position(center, &cabin, 50, 31.0, 121.0, 1, 0);
    apply_live_position(center, &cabin, 51, 31.0, 121.0, 1, 0);
    apply_live_position(center, &cabin, 52, 31.0, 121.0, 1, 0);
    assert(cabin.flown_track_count == CABIN_FLOWN_TRACK_MAX_POINTS);
    assert(fabs(cabin.flown_track[39].latitude - 31.0) < 0.000001);

    cabin_data_commit_map_view(&cabin, 1);
    const int loaded_zoom = cabin.map_loaded_zoom;
    const unsigned int request_revision = cabin.map_request_revision;
    assert(cabin_data_request_map_zoom(&cabin, 1));
    assert(cabin.map_zoom == loaded_zoom + 1);
    assert(cabin.map_refresh_requested);
    assert(cabin.map_request_revision == request_revision + 1);
    assert(strstr(cabin.map_cache_path, "_z11_1024x576.png") != NULL);
    cabin_data_revert_requested_map_zoom(&cabin);
    assert(cabin.map_zoom == loaded_zoom);
    assert(!cabin.map_zoom_change_pending);
    assert(!cabin.map_refresh_requested);

    SimDataCenter *unavailable_center = (SimDataCenter *)calloc(1, sizeof(*unavailable_center));
    assert(unavailable_center != NULL);
    assert(cabin_data_apply_sim_data_center(&cabin, unavailable_center, 0.033f) & CABIN_DATA_UPDATE_VALIDITY);
    assert(!cabin.snapshot_valid);
    assert(cabin.altitude == 0.0f);
    assert(strcmp(cabin.flight_phase, "UNKNOWN") == 0);
    assert(cabin.flown_track_count == CABIN_FLOWN_TRACK_MAX_POINTS);
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
