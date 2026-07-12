#include <stdio.h>
#include <string.h>

#include "../src/Data/sim_data_center.h"
#include "../src/FMC/fmc_ui_adapter.h"

static void type_text(FMC_Data *data, const char *text)
{
    for (int i = 0; text != NULL && text[i] != '\0'; ++i)
    {
        fmc_data_append_char(data, text[i]);
    }
}

static int export_is_drawable(FMC_Data *data, SimPlannedRoute *route)
{
    if (!fmc_data_export_planned_route(data, route) || !route->has_coordinates)
    {
        return 0;
    }

    for (int i = 0; i < route->point_count; ++i)
    {
        if (!route->points[i].has_position)
        {
            return 0;
        }
    }

    return 1;
}

int main(void)
{
    SimDataCenter center;
    FMC_Data fmc;
    SimPlannedRoute route;

    if (!sim_data_center_init(&center))
    {
        printf("init_failed\n");
        return 2;
    }

    fmc_data_init(&fmc);
    fmc_data_apply_planned_route(&fmc, sim_data_center_route(&center));

    const int rev0 = sim_data_center_route_revision(&center);
    const int points0 = sim_data_center_route(&center)->point_count;

    if (fmc_data_export_planned_route(&fmc, &route))
    {
        printf("loaded_export origin=%s first_lat=%.6f first_lon=%.6f point_count=%d has_coordinates=%d\n",
               route.origin,
               route.points[0].latitude,
               route.points[0].longitude,
               route.point_count,
               route.has_coordinates);
    }

    type_text(&fmc, "KBFI");
    fmc_data_set_route_field(&fmc, FMC_ROUTE_FIELD_ORIGIN);
    type_text(&fmc, "KSEA");
    fmc_data_set_route_field(&fmc, FMC_ROUTE_FIELD_DESTINATION);

    printf("edit_only revision_before=%d revision_after=%d official_points_before=%d official_points_after=%d mod=%d draft_points=%d\n",
           rev0,
           sim_data_center_route_revision(&center),
           points0,
           sim_data_center_route(&center)->point_count,
           fmc_data_route_has_uncommitted_changes(&fmc),
           fmc.route_count);

    if (export_is_drawable(&fmc, &route))
    {
        sim_data_center_set_route(&center, &route);
        fmc_data_mark_route_committed(&fmc);
    }
    printf("commit revision=%d origin=%s destination=%s planned_points=%d mod=%d\n",
           sim_data_center_route_revision(&center),
           sim_data_center_route(&center)->origin,
           sim_data_center_route(&center)->destination,
           sim_data_center_route(&center)->point_count,
           fmc_data_route_has_uncommitted_changes(&fmc));
    printf("commit_order first=%s last=%s\n",
           sim_data_center_route(&center)->points[0].ident,
           sim_data_center_route(&center)->points[sim_data_center_route(&center)->point_count - 1].ident);

    const int rev_after_commit = sim_data_center_route_revision(&center);
    fmc.route_has_position[0] = 0;
    if (!export_is_drawable(&fmc, &route))
    {
        printf("invalid_skipped revision=%d unchanged=%d\n",
               sim_data_center_route_revision(&center),
               sim_data_center_route_revision(&center) == rev_after_commit);
    }

    fmc_data_show_delete(&fmc);
    fmc_data_set_route_field(&fmc, FMC_ROUTE_FIELD_ORIGIN);
    printf("clear_draft_only revision=%d clear_pending=%d mod=%d official_has_route=%d\n",
           sim_data_center_route_revision(&center),
           fmc_data_route_clear_pending(&fmc),
           fmc_data_route_has_uncommitted_changes(&fmc),
           sim_data_center_has_route(&center));

    sim_data_center_clear_route(&center);
    fmc_data_mark_route_committed(&fmc);
    printf("clear_commit revision=%d has_route=%d mod=%d\n",
           sim_data_center_route_revision(&center),
           sim_data_center_has_route(&center),
           fmc_data_route_has_uncommitted_changes(&fmc));

    fmc_data_destroy(&fmc);
    sim_data_center_destroy(&center);
    return 0;
}
