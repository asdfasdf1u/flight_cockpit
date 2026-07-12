#include <stdio.h>
#include <string.h>

#include "../src/Data/sim_data_center.h"
#include "../src/FMC/fmc_ui_adapter.h"
#include "../src/ND/nd_data.h"

static void type_text(FMC_Data *data, const char *text)
{
    for (int i = 0; text != NULL && text[i] != '\0'; ++i)
    {
        fmc_data_append_char(data, text[i]);
    }
}

static int route_export_has_drawable(FMC_Data *data, SimPlannedRoute *route)
{
    if (!fmc_data_export_planned_route(data, route))
    {
        return 0;
    }
    if (!route->valid || route->point_count < 2)
    {
        return 0;
    }

    for (int i = 0; i < route->point_count; ++i)
    {
        if (route->points[i].has_position)
        {
            return 1;
        }
    }

    return 0;
}

static void set_route_point(SimRoutePoint *point, const char *ident, double latitude, double longitude, int has_position)
{
    memset(point, 0, sizeof(*point));
    snprintf(point->ident, sizeof(point->ident), "%s", ident);
    snprintf(point->type, sizeof(point->type), "%s", "FIX");
    point->latitude = latitude;
    point->longitude = longitude;
    point->has_position = has_position;
}

int main(void)
{
    SimDataCenter center;
    ND_Data nd;
    FMC_Data fmc;
    SimPlannedRoute route;

    if (!sim_data_center_init(&center))
    {
        printf("center_init_failed\n");
        return 2;
    }

    nd_data_init(&nd);
    fmc_data_init(&fmc);
    fmc_data_apply_planned_route(&fmc, sim_data_center_route(&center));

    nd_data_sync_planned_route(&nd, sim_data_center_route(&center), sim_data_center_route_revision(&center), 1);
    printf("initial nd_rev=%d nd_points=%d nd_segments=%d first=%s last=%s\n",
           nd.route_cached_revision,
           nd.route_point_count,
           nd.route_segment_count,
           nd.route_point_count > 0 ? nd.route_points[0].ident : "----",
           nd.route_point_count > 0 ? nd.route_points[nd.route_point_count - 1].ident : "----");

    type_text(&fmc, "KBFI");
    fmc_data_set_route_field(&fmc, FMC_ROUTE_FIELD_ORIGIN);
    type_text(&fmc, "KSEA");
    fmc_data_set_route_field(&fmc, FMC_ROUTE_FIELD_DESTINATION);
    nd_data_sync_planned_route(&nd, sim_data_center_route(&center), sim_data_center_route_revision(&center), 1);
    printf("uncommitted nd_rev=%d center_rev=%d nd_first=%s nd_last=%s mod=%d\n",
           nd.route_cached_revision,
           sim_data_center_route_revision(&center),
           nd.route_point_count > 0 ? nd.route_points[0].ident : "----",
           nd.route_point_count > 0 ? nd.route_points[nd.route_point_count - 1].ident : "----",
           fmc_data_route_has_uncommitted_changes(&fmc));

    if (route_export_has_drawable(&fmc, &route))
    {
        sim_data_center_set_route(&center, &route);
        fmc_data_mark_route_committed(&fmc);
    }
    nd_data_sync_planned_route(&nd, sim_data_center_route(&center), sim_data_center_route_revision(&center), 0);
    printf("committed nd_rev=%d center_rev=%d nd_points=%d nd_segments=%d first=%s last=%s\n",
           nd.route_cached_revision,
           sim_data_center_route_revision(&center),
           nd.route_point_count,
           nd.route_segment_count,
           nd.route_point_count > 0 ? nd.route_points[0].ident : "----",
           nd.route_point_count > 0 ? nd.route_points[nd.route_point_count - 1].ident : "----");

    nd_data_sync_planned_route(&nd, sim_data_center_route(&center), sim_data_center_route_revision(&center), 1);
    printf("repeat_unchanged nd_rev=%d center_rev=%d nd_points=%d nd_segments=%d\n",
           nd.route_cached_revision,
           sim_data_center_route_revision(&center),
           nd.route_point_count,
           nd.route_segment_count);

    SimPlannedRoute single;
    memset(&single, 0, sizeof(single));
    single.valid = 1;
    single.source = SIM_ROUTE_SOURCE_FMC_FALLBACK;
    snprintf(single.origin, sizeof(single.origin), "%s", "ONE");
    snprintf(single.destination, sizeof(single.destination), "%s", "ONE");
    single.point_count = 1;
    set_route_point(&single.points[0], "ONE", 39.95, 116.45, 1);
    sim_data_center_set_route(&center, &single);
    nd_data_sync_planned_route(&nd, sim_data_center_route(&center), sim_data_center_route_revision(&center), 1);
    printf("single_valid nd_rev=%d center_rev=%d nd_points=%d nd_segments=%d first_has_pos=%d\n",
           nd.route_cached_revision,
           sim_data_center_route_revision(&center),
           nd.route_point_count,
           nd.route_segment_count,
           nd.route_point_count > 0 ? nd.route_points[0].has_position : 0);

    SimPlannedRoute gap;
    memset(&gap, 0, sizeof(gap));
    gap.valid = 1;
    gap.source = SIM_ROUTE_SOURCE_FMC_FALLBACK;
    snprintf(gap.origin, sizeof(gap.origin), "%s", "A");
    snprintf(gap.destination, sizeof(gap.destination), "%s", "C");
    gap.point_count = 3;
    gap.active_waypoint_index = 99;
    set_route_point(&gap.points[0], "A", 39.95, 116.45, 1);
    set_route_point(&gap.points[1], "BAD", 0.0, 0.0, 0);
    set_route_point(&gap.points[2], "C", 40.00, 116.50, 1);
    sim_data_center_set_route(&center, &gap);
    nd_data_sync_planned_route(&nd, sim_data_center_route(&center), sim_data_center_route_revision(&center), 1);
    printf("gap_route nd_rev=%d center_rev=%d nd_points=%d nd_segments=%d middle_has_pos=%d\n",
           nd.route_cached_revision,
           sim_data_center_route_revision(&center),
           nd.route_point_count,
           nd.route_segment_count,
           nd.route_point_count > 1 ? nd.route_points[1].has_position : -1);

    const int rev_before_failed = sim_data_center_route_revision(&center);
    SimPlannedRoute failed;
    memset(&failed, 0, sizeof(failed));
    failed.valid = 0;
    sim_data_center_set_route(&center, &failed);
    nd_data_sync_planned_route(&nd, sim_data_center_route(&center), sim_data_center_route_revision(&center), 1);
    printf("failed_commit rev_before=%d rev_after=%d unchanged=%d nd_points=%d\n",
           rev_before_failed,
           sim_data_center_route_revision(&center),
           sim_data_center_route_revision(&center) == rev_before_failed,
           nd.route_point_count);

    sim_data_center_clear_route(&center);
    nd_data_sync_planned_route(&nd, sim_data_center_route(&center), sim_data_center_route_revision(&center), 1);
    printf("cleared nd_rev=%d center_rev=%d nd_points=%d nd_segments=%d has_route=%d\n",
           nd.route_cached_revision,
           sim_data_center_route_revision(&center),
           nd.route_point_count,
           nd.route_segment_count,
           sim_data_center_has_route(&center));

    fmc_data_destroy(&fmc);
    sim_data_center_destroy(&center);
    return 0;
}
