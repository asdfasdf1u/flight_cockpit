#include <stdio.h>
#include <math.h>
#include "src/ND/nd_data.h"
#include "src/Data/sim_data_center.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float norm_signed(float a) {
    while (a > 180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

static void apply_snapshot(ND_Data *data, const SimSnapshot *snapshot) {
    if (!data || !snapshot || !snapshot->has_nd) return;
    data->latitude = snapshot->latitude;
    data->longitude = snapshot->longitude;
    data->heading = snapshot->heading;
    data->track = snapshot->track;
    data->ground_speed = snapshot->ground_speed;
    data->true_air_speed = snapshot->true_air_speed;
    data->simulation_time = snapshot->sim_time;
    data->data_frame_index = snapshot->nd_frame_index;
    data->data_frame_elapsed = snapshot->sim_time;
    nd_data_recalculate_nav_points(data);
}

int main(void) {
    ND_Data data;
    SimDataCenter center;
    nd_data_init(&data);
    if (sim_data_center_init(&center) && sim_data_center_has_nd_data(&center)) {
        apply_snapshot(&data, sim_data_center_snapshot(&center));
    }

    printf("nd lat=%.6f lon=%.6f track=%.1f range=%.1f nav_points=%d route_valid=%d route_count=%d cached_rev=%d\n",
           data.latitude, data.longitude, data.track, data.range_nm, data.nav_point_count,
           data.route_valid, data.route_point_count, data.route_cached_revision);
    printf("visible WPT candidates within range and +/-45deg:\n");
    int count = 0;
    for (int i = 0; i < data.nav_point_count; ++i) {
        ND_NavPoint *p = &data.nav_points[i];
        if (p->type != ND_POINT_WAYPOINT) continue;
        if (!p->visible) continue;
        if (p->distance_nm < 0.0f || p->distance_nm > data.range_nm) continue;
        float rel = norm_signed(p->bearing_deg - data.track);
        if (rel < -45.0f || rel > 45.0f) continue;
        printf("%2d %-8s dist=%6.1f bearing=%6.1f rel=%6.1f lat=%.6f lon=%.6f\n",
               ++count, p->ident, p->distance_nm, p->bearing_deg, rel, p->latitude, p->longitude);
    }
    printf("count=%d\n", count);
    sim_data_center_destroy(&center);
    return 0;
}
