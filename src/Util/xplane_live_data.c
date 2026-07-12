#include "xplane_live_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XPLANE_LIVE_POLL_INTERVAL_SEC (1.0f / 30.0f)
#define XPLANE_LIVE_RETRY_INTERVAL_SEC 1.0f
#define XPLANE_LIVE_MAX_MISSED_FRAMES 5
#define XPLANE_LIVE_METER_TO_FEET 3.280839895f
#define XPLANE_LIVE_MPS_TO_KNOTS 1.943844492f
#define XPLANE_LIVE_KG_PER_SEC_TO_LB_PER_HOUR 7936.641438f
#define XPLANE_LIVE_KG_TO_KLB 0.00220462262f

static float clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

static float normalize_degrees(float value)
{
    while (value >= 360.0f)
    {
        value -= 360.0f;
    }
    while (value < 0.0f)
    {
        value += 360.0f;
    }
    return value;
}

static float first_or_default(const float *values, int size, float fallback)
{
    return values != NULL && size > 0 ? values[0] : fallback;
}

static float second_or_default(const float *values, int size, float fallback)
{
    return values != NULL && size > 1 ? values[1] : fallback;
}

static float array_average_nonnegative(const float *values, int size, int max_count, float fallback)
{
    if (values == NULL || size <= 0)
    {
        return fallback;
    }

    const int count_limit = size < max_count ? size : max_count;
    float sum = 0.0f;
    int count = 0;
    for (int i = 0; i < count_limit; ++i)
    {
        if (values[i] >= 0.0f)
        {
            sum += values[i];
            ++count;
        }
    }

    return count > 0 ? sum / (float)count : fallback;
}

static void set_status(XPlaneLiveData *live, int pfd_active, int nd_active, int eicas_active)
{
    if (live == NULL)
    {
        return;
    }

    const int was_connected = live->connected;
    live->pfd_active = pfd_active;
    live->nd_active = nd_active;
    live->eicas_active = eicas_active;
    live->connected = pfd_active || nd_active || eicas_active;

    if (!was_connected && live->connected)
    {
        printf("X-Plane live data: connected to %s:%u.\n", live->xp_ip, live->xp_port);
        fflush(stdout);
    }
    else if (was_connected && !live->connected)
    {
        printf("X-Plane live data: connection lost, using local fallback data.\n");
        fflush(stdout);
    }
}

static void open_socket_once(XPlaneLiveData *live)
{
    if (live == NULL || live->socket_open)
    {
        return;
    }

    live->socket = aopenUDP(live->xp_ip, live->xp_port, 0);
    live->socket_open = 1;
    printf("X-Plane live data: waiting for X-Plane Connect at %s:%u.\n", live->xp_ip, live->xp_port);
    fflush(stdout);
}

static int poll_all_data(
    XPlaneLiveData *live,
    PFD_Data *pfd_data,
    ND_Data *nd_data,
    EICAS_Data *eicas_data,
    AircraftSystems_Data *systems_data)
{
    if (live == NULL || pfd_data == NULL || nd_data == NULL ||
        eicas_data == NULL || systems_data == NULL || !live->socket_open)
    {
        return 0;
    }

    enum
    {
        DREF_THETA,
        DREF_PHI,
        DREF_PSI,
        DREF_H_IND,
        DREF_Y_AGL,
        DREF_THROTTLE,
        DREF_IAS,
        DREF_AP_AIRSPEED,
        DREF_VSPEED,
        DREF_MAG_PSI,
        DREF_AP_HEADING,
        DREF_AP_ALTITUDE,
        DREF_LATITUDE,
        DREF_LONGITUDE,
        DREF_ND_MAG_PSI,
        DREF_TRUE_AIRSPEED,
        DREF_GROUNDSPEED,
        DREF_TAT,
        DREF_N1,
        DREF_EGT,
        DREF_FUEL_FLOW,
        DREF_FUEL_MASS,
        DREF_COUNT
    };

    const char *drefs[DREF_COUNT] = {
        "sim/flightmodel/position/theta",
        "sim/flightmodel/position/phi",
        "sim/flightmodel/position/psi",
        "sim/flightmodel/misc/h_ind",
        "sim/flightmodel/position/y_agl",
        "sim/flightmodel/engine/ENGN_thro",
        "sim/flightmodel/position/indicated_airspeed",
        "sim/cockpit/autopilot/airspeed",
        "sim/flightmodel/position/vh_ind_fpm",
        "sim/flightmodel/position/mag_psi",
        "sim/cockpit/autopilot/heading_mag",
        "sim/cockpit/autopilot/altitude",
        "sim/flightmodel/position/latitude",
        "sim/flightmodel/position/longitude",
        "sim/flightmodel/position/mag_psi",
        "sim/flightmodel/position/true_airspeed",
        "sim/flightmodel/position/groundspeed",
        "sim/weather/temperature_ambient_c",
        "sim/flightmodel/engine/ENGN_N1_",
        "sim/flightmodel/engine/ENGN_EGT_c",
        "sim/cockpit2/engine/indicators/fuel_flow_kg_sec",
        "sim/flightmodel/weight/m_fuel"};

    float theta[1] = {0.0f};
    float phi[1] = {0.0f};
    float psi[1] = {0.0f};
    float h_ind[1] = {0.0f};
    float y_agl[1] = {0.0f};
    float throttle[8] = {0.0f};
    float indicated_airspeed[1] = {0.0f};
    float ap_airspeed[1] = {0.0f};
    float vertical_speed[1] = {0.0f};
    float mag_psi[1] = {0.0f};
    float ap_heading[1] = {0.0f};
    float ap_altitude[1] = {0.0f};

    float latitude[1] = {0.0f};
    float longitude[1] = {0.0f};
    float nd_mag_psi[1] = {0.0f};
    float true_airspeed[1] = {0.0f};
    float groundspeed[1] = {0.0f};

    float tat[1] = {0.0f};
    float n1[8] = {0.0f};
    float egt[8] = {0.0f};
    float fuel_flow[8] = {0.0f};
    float fuel_mass[9] = {0.0f};

    float *values[DREF_COUNT] = {
        theta,
        phi,
        psi,
        h_ind,
        y_agl,
        throttle,
        indicated_airspeed,
        ap_airspeed,
        vertical_speed,
        mag_psi,
        ap_heading,
        ap_altitude,
        latitude,
        longitude,
        nd_mag_psi,
        true_airspeed,
        groundspeed,
        tat,
        n1,
        egt,
        fuel_flow,
        fuel_mass};
    int sizes[DREF_COUNT] = {
        1, 1, 1, 1, 1, 8,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1,
        1, 8, 8, 8, 9};

    if (getDREFs(live->socket, drefs, values, DREF_COUNT, sizes) < 0)
    {
        return 0;
    }

    /* PFD */
    pfd_data->pitch = first_or_default(theta, sizes[DREF_THETA], pfd_data->pitch);
    pfd_data->roll = first_or_default(phi, sizes[DREF_PHI], pfd_data->roll);
    pfd_data->yaw = first_or_default(psi, sizes[DREF_PSI], pfd_data->yaw);
    pfd_data->altitude = first_or_default(h_ind, sizes[DREF_H_IND], pfd_data->altitude);
    pfd_data->agl_altitude = first_or_default(y_agl, sizes[DREF_Y_AGL], pfd_data->agl_altitude / XPLANE_LIVE_METER_TO_FEET) * XPLANE_LIVE_METER_TO_FEET;
    pfd_data->throttle = clamp_float(array_average_nonnegative(throttle, sizes[DREF_THROTTLE], 8, pfd_data->throttle / 100.0f) * 100.0f, 0.0f, 100.0f);
    pfd_data->airspeed_current = first_or_default(indicated_airspeed, sizes[DREF_IAS], pfd_data->airspeed_current);
    pfd_data->airspeed_target = first_or_default(ap_airspeed, sizes[DREF_AP_AIRSPEED], pfd_data->airspeed_target);
    pfd_data->vertical_speed = first_or_default(vertical_speed, sizes[DREF_VSPEED], pfd_data->vertical_speed);
    pfd_data->heading = normalize_degrees(first_or_default(mag_psi, sizes[DREF_MAG_PSI], pfd_data->heading));
    pfd_data->heading_target = normalize_degrees(first_or_default(ap_heading, sizes[DREF_AP_HEADING], pfd_data->heading_target));
    pfd_data->altitude_target = first_or_default(ap_altitude, sizes[DREF_AP_ALTITUDE], pfd_data->altitude_target);
    pfd_data->autopilot_on = 1;
    snprintf(pfd_data->flight_mode, sizeof(pfd_data->flight_mode), "%s", "X-PLANE LIVE");

    /* ND */
    nd_data->latitude = (double)first_or_default(latitude, sizes[DREF_LATITUDE], (float)nd_data->latitude);
    nd_data->longitude = (double)first_or_default(longitude, sizes[DREF_LONGITUDE], (float)nd_data->longitude);
    nd_data->heading = normalize_degrees(first_or_default(nd_mag_psi, sizes[DREF_ND_MAG_PSI], nd_data->heading));
    nd_data->track = nd_data->heading;
    nd_data->true_air_speed = first_or_default(true_airspeed, sizes[DREF_TRUE_AIRSPEED], nd_data->true_air_speed / XPLANE_LIVE_MPS_TO_KNOTS) * XPLANE_LIVE_MPS_TO_KNOTS;
    nd_data->ground_speed = first_or_default(groundspeed, sizes[DREF_GROUNDSPEED], nd_data->ground_speed / XPLANE_LIVE_MPS_TO_KNOTS) * XPLANE_LIVE_MPS_TO_KNOTS;
    nd_data_recalculate_nav_points(nd_data);

    /* EICAS */
    eicas_data->tat = first_or_default(tat, sizes[DREF_TAT], eicas_data->tat);
    eicas_data->engine_left.n1 = clamp_float(first_or_default(n1, sizes[DREF_N1], eicas_data->engine_left.n1), 0.0f, 110.0f);
    eicas_data->engine_right.n1 = clamp_float(second_or_default(n1, sizes[DREF_N1], eicas_data->engine_right.n1), 0.0f, 110.0f);
    eicas_data->engine_left.egt = first_or_default(egt, sizes[DREF_EGT], eicas_data->engine_left.egt);
    eicas_data->engine_right.egt = second_or_default(egt, sizes[DREF_EGT], eicas_data->engine_right.egt);
    eicas_data->engine_left.fuel_flow = clamp_float(first_or_default(fuel_flow, sizes[DREF_FUEL_FLOW], eicas_data->engine_left.fuel_flow / XPLANE_LIVE_KG_PER_SEC_TO_LB_PER_HOUR) * XPLANE_LIVE_KG_PER_SEC_TO_LB_PER_HOUR, 0.0f, 10000.0f);
    eicas_data->engine_right.fuel_flow = clamp_float(second_or_default(fuel_flow, sizes[DREF_FUEL_FLOW], eicas_data->engine_right.fuel_flow / XPLANE_LIVE_KG_PER_SEC_TO_LB_PER_HOUR) * XPLANE_LIVE_KG_PER_SEC_TO_LB_PER_HOUR, 0.0f, 10000.0f);
    eicas_data->engine_left.running = eicas_data->engine_left.n1 > 20.0f;
    eicas_data->engine_right.running = eicas_data->engine_right.n1 > 20.0f;

    if (sizes[DREF_FUEL_MASS] >= 3)
    {
        eicas_data->fuel_left_quantity = fuel_mass[0] * XPLANE_LIVE_KG_TO_KLB;
        eicas_data->fuel_center_quantity = fuel_mass[1] * XPLANE_LIVE_KG_TO_KLB;
        eicas_data->fuel_right_quantity = fuel_mass[2] * XPLANE_LIVE_KG_TO_KLB;
        eicas_data->fuel_total_quantity = eicas_data->fuel_left_quantity + eicas_data->fuel_center_quantity + eicas_data->fuel_right_quantity;
        eicas_data->fuel_quantity = eicas_data->fuel_total_quantity / (60.9f + 48.7f * 2.0f);
    }

    eicas_data_refresh_warnings(eicas_data);
    eicas_data_apply_to_aircraft_systems(eicas_data, systems_data);

    return 1;
}

void xplane_live_data_init(XPlaneLiveData *live, const char *xp_ip, unsigned short xp_port)
{
    if (live == NULL)
    {
        return;
    }

    memset(live, 0, sizeof(*live));
    snprintf(live->xp_ip, sizeof(live->xp_ip), "%s", xp_ip != NULL ? xp_ip : XPLANE_LIVE_DEFAULT_IP);
    live->xp_port = xp_port != 0 ? xp_port : XPLANE_LIVE_DEFAULT_PORT;
    live->retry_elapsed = XPLANE_LIVE_RETRY_INTERVAL_SEC;
    live->missed_frames = 0;
}

void xplane_live_data_shutdown(XPlaneLiveData *live)
{
    if (live == NULL || !live->socket_open)
    {
        return;
    }

    closeUDP(live->socket);
    live->socket_open = 0;
    set_status(live, 0, 0, 0);
}

int xplane_live_data_update(
    XPlaneLiveData *live,
    PFD_Data *pfd_data,
    ND_Data *nd_data,
    EICAS_Data *eicas_data,
    AircraftSystems_Data *systems_data,
    float delta_time)
{
    if (live == NULL)
    {
        return 0;
    }

    if (delta_time < 0.0f)
    {
        delta_time = 0.0f;
    }
    if (delta_time > 0.1f)
    {
        delta_time = 0.1f;
    }

    open_socket_once(live);

    if (live->connected)
    {
        live->poll_elapsed += delta_time;
        if (live->poll_elapsed < XPLANE_LIVE_POLL_INTERVAL_SEC)
        {
            return live->connected;
        }
        live->poll_elapsed = 0.0f;
    }
    else
    {
        live->retry_elapsed += delta_time;
        if (live->retry_elapsed < XPLANE_LIVE_RETRY_INTERVAL_SEC)
        {
            return 0;
        }
        live->retry_elapsed = 0.0f;
    }

    const int ok = poll_all_data(live, pfd_data, nd_data, eicas_data, systems_data);
    if (ok)
    {
        live->missed_frames = 0;
        set_status(live, 1, 1, 1);
    }
    else
    {
        live->missed_frames++;
        if (live->missed_frames >= XPLANE_LIVE_MAX_MISSED_FRAMES)
        {
            set_status(live, 0, 0, 0);
        }
    }

    return live->connected;
}

int xplane_live_data_pfd_active(const XPlaneLiveData *live)
{
    return live != NULL && live->pfd_active;
}

int xplane_live_data_nd_active(const XPlaneLiveData *live)
{
    return live != NULL && live->nd_active;
}

int xplane_live_data_eicas_active(const XPlaneLiveData *live)
{
    return live != NULL && live->eicas_active;
}

int xplane_live_data_connected(const XPlaneLiveData *live)
{
    return live != NULL && live->connected;
}
