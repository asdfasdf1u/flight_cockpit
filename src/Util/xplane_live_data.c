#include "xplane_live_data.h"

#include "../Data/sim_data_center.h"

#include <math.h>
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
#define XPLANE_LIVE_MAX_FUEL_FLOW_LB_PER_HOUR 50000.0f
#define XPLANE_LIVE_OIL_QUANTITY_FULL_SCALE_QT 20.0f
#define XPLANE_LIVE_VIBRATION_MAX_HZ 100.0f
#define XPLANE_LIVE_VIBRATION_DISPLAY_MAX 5.0f
#define XPLANE_LIVE_MIN_TRACK_GROUND_SPEED_KT 5.0f
#define XPLANE_RREF_FREQUENCY_HZ 30
#define XPLANE_RREF_PATH_BYTES 400
#define XPLANE_RREF_REQUEST_BYTES (5 + 4 + 4 + XPLANE_RREF_PATH_BYTES)
#define XPLANE_RREF_RESPONSE_HEADER_BYTES 5
#define XPLANE_RREF_RESPONSE_RECORD_BYTES 8
#define XPLANE_RREF_STALE_SEC 1.5f
#define XPLANE_RREF_RESUBSCRIBE_SEC 2.0f
#define XPLANE_RREF_MAX_DRAIN_PACKETS 4

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

static float normalize_signed_degrees(float value)
{
    value = normalize_degrees(value);
    if (value > 180.0f)
    {
        value -= 360.0f;
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

/*
 * X-Plane exposes vibration through cockpit2.  Its generic vibration dataref
 * is zero for the Laminar B738 even while its EICAS shows a non-zero value,
 * so a non-positive sample must not mask the next available data source.
 */
static float engine_value_with_legacy_fallback(
    const float *primary,
    int primary_size,
    const float *legacy,
    int legacy_size,
    int engine_index,
    float fallback)
{
    if (primary != NULL && primary_size > engine_index && primary[engine_index] > 0.0f)
    {
        return primary[engine_index];
    }

    if (legacy != NULL && legacy_size > engine_index && legacy[engine_index] > 0.0f)
    {
        return legacy[engine_index];
    }

    return fallback;
}

static float vibration_hz_to_display(float hz_value)
{
    if (hz_value <= 0.0f)
    {
        return 0.0f;
    }

    const float normalized = hz_value / XPLANE_LIVE_VIBRATION_MAX_HZ;
    return clamp_float(normalized * XPLANE_LIVE_VIBRATION_DISPLAY_MAX, 0.0f, XPLANE_LIVE_VIBRATION_DISPLAY_MAX);
}

static float b738_vibration_display_value(const float *values, int size, float fallback)
{
    /* The B738 dataref is already in the same 0.0--5.0 VIB scale as its EICAS. */
    if (values != NULL && size > 0 && values[0] > 0.0f)
    {
        return clamp_float(values[0], 0.0f, XPLANE_LIVE_VIBRATION_DISPLAY_MAX);
    }

    return fallback;
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

static float oil_quantity_display(float value, float fallback)
{
    if (value < 0.0f)
    {
        return fallback;
    }

    return value <= 1.5f ? value * XPLANE_LIVE_OIL_QUANTITY_FULL_SCALE_QT : value;
}

static int valid_float(float value)
{
    return isfinite(value);
}

static int valid_double(double value)
{
    return isfinite(value);
}

static int valid_geo(double latitude, double longitude)
{
    return valid_double(latitude) && valid_double(longitude) &&
           latitude >= -90.0 && latitude <= 90.0 &&
           longitude >= -180.0 && longitude <= 180.0;
}

static int valid_airspeed(float value)
{
    return valid_float(value) && value >= 0.0f;
}

static int valid_altitude(float value)
{
    return valid_float(value);
}

static int valid_engine(float value)
{
    return valid_float(value) && value >= 0.0f;
}

static void set_status(XPlaneLiveData *live, int pfd_active, int nd_active, int eicas_active, int fmc_active)
{
    if (live == NULL)
    {
        return;
    }

    const int was_connected = live->connected;
    live->pfd_active = pfd_active;
    live->nd_active = nd_active;
    live->eicas_active = eicas_active;
    live->fmc_active = fmc_active;
    live->connected = pfd_active || nd_active || eicas_active || fmc_active;

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
    printf("X-Plane live data: waiting for native RREF data at %s:%u.\n", live->xp_ip, live->xp_port);
    fflush(stdout);
}

static int rref_write_dref_path(char *dest, int dest_size, const char *dref, int element_index, int element_count)
{
    int written;

    if (dest == NULL || dest_size <= 0 || dref == NULL)
    {
        return 0;
    }

    if (element_count > 1)
    {
        written = snprintf(dest, (size_t)dest_size, "%s[%d]", dref, element_index);
    }
    else
    {
        written = snprintf(dest, (size_t)dest_size, "%s", dref);
    }

    return written > 0 && written < dest_size;
}

static int rref_send_subscription(XPlaneLiveData *live, int subscription_index, const char *dref, int element_index, int element_count, int frequency_hz)
{
    char buffer[XPLANE_RREF_REQUEST_BYTES];
    char path[XPLANE_RREF_PATH_BYTES];
    int xplane_index;

    if (live == NULL || !live->socket_open)
    {
        return 0;
    }

    memset(buffer, 0, sizeof(buffer));
    memset(path, 0, sizeof(path));
    if (!rref_write_dref_path(path, sizeof(path), dref, element_index, element_count))
    {
        return 0;
    }

    memcpy(buffer, "RREF\0", 5);
    xplane_index = subscription_index + 1;
    memcpy(buffer + 5, &frequency_hz, sizeof(frequency_hz));
    memcpy(buffer + 9, &xplane_index, sizeof(xplane_index));
    memcpy(buffer + 13, path, strlen(path));

    return sendUDP(live->socket, buffer, sizeof(buffer)) >= 0;
}

static int rref_ensure_subscriptions(XPlaneLiveData *live, const char *drefs[], int dref_count, const int capacities[])
{
    int subscription_index = 0;
    int failures = 0;
    int should_resubscribe;
    int rref_stale;

    if (live == NULL || drefs == NULL || capacities == NULL || !live->socket_open)
    {
        return 0;
    }

    rref_stale = live->rref_last_packet_time <= 0.0f ||
                 live->elapsed_time - live->rref_last_packet_time > XPLANE_RREF_STALE_SEC;
    should_resubscribe = !live->rref_subscribed ||
                         (rref_stale &&
                          live->elapsed_time - live->rref_last_subscribe_time >= XPLANE_RREF_RESUBSCRIBE_SEC);
    if (!should_resubscribe)
    {
        return 1;
    }

    for (int dref_index = 0; dref_index < dref_count; ++dref_index)
    {
        const int element_count = capacities[dref_index] > 0 ? capacities[dref_index] : 1;
        for (int element_index = 0; element_index < element_count; ++element_index)
        {
            if (subscription_index >= XPLANE_RREF_MAX_SUBSCRIPTIONS)
            {
                return 0;
            }

            live->rref_binding_dref_index[subscription_index] = dref_index;
            live->rref_binding_element_index[subscription_index] = element_index;
            if (!rref_send_subscription(live, subscription_index, drefs[dref_index], element_index, element_count, XPLANE_RREF_FREQUENCY_HZ))
            {
                failures++;
            }
            subscription_index++;
        }
    }

    live->rref_subscription_count = subscription_index;
    live->rref_subscribed = failures == 0;
    live->rref_last_subscribe_time = live->elapsed_time;
    if (live->rref_subscribed)
    {
        printf("X-Plane live data: subscribed to %d native RREF datarefs at %s:%u.\n",
               live->rref_subscription_count,
               live->xp_ip,
               live->xp_port);
        fflush(stdout);
    }

    return live->rref_subscribed;
}

static void rref_drain_packets(XPlaneLiveData *live)
{
    char buffer[4096];

    if (live == NULL || !live->socket_open)
    {
        return;
    }

    for (int packet_index = 0; packet_index < XPLANE_RREF_MAX_DRAIN_PACKETS; ++packet_index)
    {
        const int bytes_read = readUDP(live->socket, buffer, sizeof(buffer));
        if (bytes_read <= 0)
        {
            return;
        }
        if (bytes_read < XPLANE_RREF_RESPONSE_HEADER_BYTES + XPLANE_RREF_RESPONSE_RECORD_BYTES ||
            memcmp(buffer, "RREF", 4) != 0)
        {
            continue;
        }

        for (int offset = XPLANE_RREF_RESPONSE_HEADER_BYTES;
             offset + XPLANE_RREF_RESPONSE_RECORD_BYTES <= bytes_read;
             offset += XPLANE_RREF_RESPONSE_RECORD_BYTES)
        {
            int xplane_index;
            float value;
            int subscription_index;

            memcpy(&xplane_index, buffer + offset, sizeof(xplane_index));
            memcpy(&value, buffer + offset + 4, sizeof(value));
            subscription_index = xplane_index - 1;
            if (subscription_index < 0 || subscription_index >= live->rref_subscription_count)
            {
                continue;
            }

            live->rref_values[subscription_index] = value;
            live->rref_last_update[subscription_index] = live->elapsed_time;
            live->rref_seen[subscription_index] = 1;
            live->rref_last_packet_time = live->elapsed_time;
        }
    }
}

static int rref_subscription_fresh(const XPlaneLiveData *live, int subscription_index)
{
    return live != NULL &&
           subscription_index >= 0 &&
           subscription_index < live->rref_subscription_count &&
           live->rref_seen[subscription_index] &&
           live->elapsed_time - live->rref_last_update[subscription_index] <= XPLANE_RREF_STALE_SEC;
}

static int rref_find_subscription(const XPlaneLiveData *live, int dref_index, int element_index)
{
    if (live == NULL)
    {
        return -1;
    }

    for (int subscription_index = 0; subscription_index < live->rref_subscription_count; ++subscription_index)
    {
        if (live->rref_binding_dref_index[subscription_index] == dref_index &&
            live->rref_binding_element_index[subscription_index] == element_index)
        {
            return subscription_index;
        }
    }

    return -1;
}

static void rref_copy_values(const XPlaneLiveData *live, float *values[], int dref_count, int sizes[])
{
    if (live == NULL || values == NULL || sizes == NULL)
    {
        return;
    }

    for (int dref_index = 0; dref_index < dref_count; ++dref_index)
    {
        const int capacity = sizes[dref_index];
        int actual_count = 0;

        for (int element_index = 0; element_index < capacity; ++element_index)
        {
            const int subscription_index = rref_find_subscription(live, dref_index, element_index);
            if (!rref_subscription_fresh(live, subscription_index))
            {
                break;
            }

            values[dref_index][element_index] = live->rref_values[subscription_index];
            actual_count++;
        }

        sizes[dref_index] = actual_count;
    }
}

static void apply_frame_to_legacy_modules(
    const SimXPlaneLiveFrame *frame,
    PFD_Data *pfd_data,
    ND_Data *nd_data,
    EICAS_Data *eicas_data,
    AircraftSystems_Data *systems_data,
    FMC_Data *fmc_data)
{
    if (frame == NULL || pfd_data == NULL || nd_data == NULL ||
        eicas_data == NULL || systems_data == NULL || !frame->valid)
    {
        return;
    }

    pfd_data->pitch = frame->pitch;
    pfd_data->roll = frame->roll;
    pfd_data->yaw = frame->yaw;
    pfd_data->altitude = frame->altitude;
    pfd_data->agl_altitude = frame->agl_altitude;
    pfd_data->throttle = frame->throttle;
    pfd_data->airspeed_current = frame->airspeed;
    pfd_data->airspeed_target = frame->airspeed_target;
    pfd_data->vertical_speed = frame->vertical_speed;
    pfd_data->heading = frame->heading;
    pfd_data->heading_target = frame->heading_target;
    pfd_data->altitude_target = frame->altitude_target;
    pfd_data->autopilot_on = 1;
    pfd_data->simulation_time = frame->timestamp;
    pfd_data->snapshot_frame_id = frame->frame_id;
    pfd_data->data_valid = frame->valid;
    snprintf(pfd_data->flight_mode, sizeof(pfd_data->flight_mode), "%s", "X-PLANE LIVE");

    nd_data->latitude = frame->latitude;
    nd_data->longitude = frame->longitude;
    nd_data->heading = frame->heading;
    nd_data->track = frame->track;
    nd_data->true_air_speed = frame->true_air_speed;
    nd_data->ground_speed = frame->ground_speed;
    nd_data->simulation_time = frame->timestamp;
    nd_data->snapshot_frame_id = frame->frame_id;
    nd_data->data_valid = frame->valid;
    nd_data_recalculate_nav_points(nd_data);

    eicas_data->tat = frame->total_air_temperature;
    eicas_data->engine_left.n1 = frame->n1_left;
    eicas_data->engine_right.n1 = frame->n1_right;
    eicas_data->engine_left.n2 = frame->n2_left;
    eicas_data->engine_right.n2 = frame->n2_right;
    eicas_data->engine_left.egt = frame->egt_left;
    eicas_data->engine_right.egt = frame->egt_right;
    eicas_data->engine_left.fuel_flow = frame->fuel_flow_left;
    eicas_data->engine_right.fuel_flow = frame->fuel_flow_right;
    eicas_data->engine_left.oil_pressure = frame->oil_pressure_left;
    eicas_data->engine_right.oil_pressure = frame->oil_pressure_right;
    eicas_data->engine_left.oil_temperature = frame->oil_temperature_left;
    eicas_data->engine_right.oil_temperature = frame->oil_temperature_right;
    eicas_data->engine_left.oil_quantity = frame->oil_quantity_left;
    eicas_data->engine_right.oil_quantity = frame->oil_quantity_right;
    eicas_data->engine_left.vibration = frame->vibration_left;
    eicas_data->engine_right.vibration = frame->vibration_right;
    eicas_data->engine_left.running = frame->engine_left_running;
    eicas_data->engine_right.running = frame->engine_right_running;
    eicas_data->simulation_time = frame->timestamp;
    eicas_data->fuel_left_quantity = frame->fuel_left_quantity;
    eicas_data->fuel_center_quantity = frame->fuel_center_quantity;
    eicas_data->fuel_right_quantity = frame->fuel_right_quantity;
    eicas_data->fuel_total_quantity = frame->fuel_left_quantity + frame->fuel_center_quantity + frame->fuel_right_quantity;
    eicas_data->fuel_quantity = frame->fuel_quantity;
    if (frame->has_gear)
    {
        eicas_data->gear_down = frame->gear_down;
    }
    if (frame->has_flaps)
    {
        eicas_data->flaps_level = frame->flaps_level;
    }
    if (frame->has_parking_brake)
    {
        eicas_data->parking_brake_on = frame->parking_brake_on;
    }
    eicas_data_refresh_warnings(eicas_data);
    eicas_data_apply_to_aircraft_systems(eicas_data, systems_data);
    systems_data->engine_left.eicas1_fuel_flow_display = frame->fuel_flow_left / 1000.0f;
    systems_data->engine_right.eicas1_fuel_flow_display = frame->fuel_flow_right / 1000.0f;
    systems_data->engine_left.eicas1_fuel_flow_display_valid = 1;
    systems_data->engine_right.eicas1_fuel_flow_display_valid = 1;
    systems_data->engine_left.eicas2_fuel_flow_display = frame->fuel_flow_left / 1000.0f;
    systems_data->engine_right.eicas2_fuel_flow_display = frame->fuel_flow_right / 1000.0f;
    systems_data->engine_left.eicas2_fuel_flow_display_valid = 1;
    systems_data->engine_right.eicas2_fuel_flow_display_valid = 1;
    systems_data->simulation_time = frame->timestamp;
    systems_data->snapshot_frame_id = frame->frame_id;
    systems_data->data_valid = frame->valid;

    if (fmc_data != NULL)
    {
        fmc_data->live_data_active = 1;
    }
}

static int poll_all_data(
    XPlaneLiveData *live,
    PFD_Data *pfd_data,
    ND_Data *nd_data,
    EICAS_Data *eicas_data,
    AircraftSystems_Data *systems_data,
    FMC_Data *fmc_data,
    SimXPlaneLiveFrame *frame,
    float delta_time,
    int write_legacy_outputs)
{
    if (live == NULL || pfd_data == NULL || nd_data == NULL ||
        eicas_data == NULL || systems_data == NULL || frame == NULL || !live->socket_open)
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
        DREF_HPATH,
        DREF_TRUE_AIRSPEED,
        DREF_GROUNDSPEED,
        DREF_TAT,
        DREF_N1,
        DREF_N2,
        DREF_EGT,
        DREF_FUEL_FLOW,
        DREF_OIL_PRESSURE,
        DREF_OIL_TEMPERATURE,
        DREF_OIL_QUANTITY,
        DREF_VIBRATION_HZ,
        DREF_VIBRATION_LEGACY,
        DREF_B738_VIBRATION_LEFT,
        DREF_B738_VIBRATION_RIGHT,
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
        "sim/flightmodel/position/hpath",
        "sim/flightmodel/position/true_airspeed",
        "sim/flightmodel/position/groundspeed",
        "sim/weather/temperature_ambient_c",
        "sim/flightmodel/engine/ENGN_N1_",
        "sim/flightmodel/engine/ENGN_N2_",
        "sim/flightmodel/engine/ENGN_EGT_c",
        "sim/cockpit2/engine/indicators/fuel_flow_kg_sec",
        "sim/flightmodel/engine/ENGN_oil_press_psi",
        "sim/flightmodel/engine/ENGN_oil_temp_c",
        "sim/flightmodel/engine/ENGN_oil_quan",
        "sim/cockpit2/engine/indicators/vibration_hz",
        "sim/flightmodel/engine/ENGN_vib_",
        "laminar/B738/engine/indicators/engine1_vib",
        "laminar/B738/engine/indicators/engine2_vib",
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
    float hpath[1] = {0.0f};
    float true_airspeed[1] = {0.0f};
    float groundspeed[1] = {0.0f};

    float tat[1] = {0.0f};
    float n1[8] = {0.0f};
    float n2[8] = {0.0f};
    float egt[8] = {0.0f};
    float fuel_flow[8] = {0.0f};
    float oil_pressure[8] = {0.0f};
    float oil_temperature[8] = {0.0f};
    float oil_quantity[8] = {0.0f};
    float vibration_hz[8] = {0.0f};
    float vibration_legacy[8] = {0.0f};
    float b738_vibration_left[1] = {0.0f};
    float b738_vibration_right[1] = {0.0f};
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
        hpath,
        true_airspeed,
        groundspeed,
        tat,
        n1,
        n2,
        egt,
        fuel_flow,
        oil_pressure,
        oil_temperature,
        oil_quantity,
        vibration_hz,
        vibration_legacy,
        b738_vibration_left,
        b738_vibration_right,
        fuel_mass};
    int sizes[DREF_COUNT] = {
        1, 1, 1, 1, 1, 8,
        1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1,
        1, 8, 8, 8, 8, 8, 8, 8, 8, 8, 1, 1, 9};

    if (!rref_ensure_subscriptions(live, drefs, DREF_COUNT, sizes))
    {
        return 0;
    }
    rref_drain_packets(live);
    rref_copy_values(live, values, DREF_COUNT, sizes);

    if (sizes[DREF_LATITUDE] <= 0 ||
        sizes[DREF_LONGITUDE] <= 0 ||
        sizes[DREF_H_IND] <= 0 ||
        sizes[DREF_IAS] <= 0)
    {
        return 0;
    }

    const float left_vibration_hz = engine_value_with_legacy_fallback(
        vibration_hz,
        sizes[DREF_VIBRATION_HZ],
        vibration_legacy,
        sizes[DREF_VIBRATION_LEGACY],
        0,
        0.0f);
    const float right_vibration_hz = engine_value_with_legacy_fallback(
        vibration_hz,
        sizes[DREF_VIBRATION_HZ],
        vibration_legacy,
        sizes[DREF_VIBRATION_LEGACY],
        1,
        0.0f);
    memset(frame, 0, sizeof(*frame));
    frame->frame_id = live->frame_id + 1;
    frame->timestamp = live->elapsed_time;
    frame->delta_time = delta_time;
    frame->connected = 1;
    frame->last_valid_timestamp = live->last_valid_time;

    frame->pitch = first_or_default(theta, sizes[DREF_THETA], pfd_data->pitch);
    frame->roll = first_or_default(phi, sizes[DREF_PHI], pfd_data->roll);
    frame->yaw = first_or_default(psi, sizes[DREF_PSI], pfd_data->yaw);
    frame->altitude = first_or_default(h_ind, sizes[DREF_H_IND], pfd_data->altitude);
    frame->agl_altitude = first_or_default(y_agl, sizes[DREF_Y_AGL], pfd_data->agl_altitude / XPLANE_LIVE_METER_TO_FEET) * XPLANE_LIVE_METER_TO_FEET;
    frame->throttle = clamp_float(array_average_nonnegative(throttle, sizes[DREF_THROTTLE], 8, pfd_data->throttle / 100.0f) * 100.0f, 0.0f, 100.0f);
    frame->airspeed = first_or_default(indicated_airspeed, sizes[DREF_IAS], pfd_data->airspeed_current);
    frame->airspeed_target = first_or_default(ap_airspeed, sizes[DREF_AP_AIRSPEED], pfd_data->airspeed_target);
    frame->vertical_speed = first_or_default(vertical_speed, sizes[DREF_VSPEED], pfd_data->vertical_speed);
    frame->heading = normalize_degrees(first_or_default(mag_psi, sizes[DREF_MAG_PSI], pfd_data->heading));
    frame->heading_target = normalize_degrees(first_or_default(ap_heading, sizes[DREF_AP_HEADING], pfd_data->heading_target));
    frame->altitude_target = first_or_default(ap_altitude, sizes[DREF_AP_ALTITUDE], pfd_data->altitude_target);
    frame->latitude = (double)first_or_default(latitude, sizes[DREF_LATITUDE], (float)nd_data->latitude);
    frame->longitude = (double)first_or_default(longitude, sizes[DREF_LONGITUDE], (float)nd_data->longitude);
    frame->true_air_speed = first_or_default(true_airspeed, sizes[DREF_TRUE_AIRSPEED], nd_data->true_air_speed / XPLANE_LIVE_MPS_TO_KNOTS) * XPLANE_LIVE_MPS_TO_KNOTS;
    frame->ground_speed = first_or_default(groundspeed, sizes[DREF_GROUNDSPEED], nd_data->ground_speed / XPLANE_LIVE_MPS_TO_KNOTS) * XPLANE_LIVE_MPS_TO_KNOTS;
    if (frame->ground_speed >= XPLANE_LIVE_MIN_TRACK_GROUND_SPEED_KT &&
        sizes[DREF_HPATH] > 0 &&
        valid_float(hpath[0]))
    {
        const float magnetic_offset = normalize_signed_degrees(frame->heading - frame->yaw);
        frame->track = normalize_degrees(hpath[0] + magnetic_offset);
    }
    else
    {
        frame->track = frame->heading;
    }
    frame->total_air_temperature = first_or_default(tat, sizes[DREF_TAT], eicas_data->tat);
    frame->n1_left = clamp_float(first_or_default(n1, sizes[DREF_N1], eicas_data->engine_left.n1), 0.0f, 110.0f);
    frame->n1_right = clamp_float(second_or_default(n1, sizes[DREF_N1], eicas_data->engine_right.n1), 0.0f, 110.0f);
    frame->n2_left = clamp_float(first_or_default(n2, sizes[DREF_N2], eicas_data->engine_left.n2), 0.0f, 110.0f);
    frame->n2_right = clamp_float(second_or_default(n2, sizes[DREF_N2], eicas_data->engine_right.n2), 0.0f, 110.0f);
    frame->egt_left = first_or_default(egt, sizes[DREF_EGT], eicas_data->engine_left.egt);
    frame->egt_right = second_or_default(egt, sizes[DREF_EGT], eicas_data->engine_right.egt);
    frame->fuel_flow_left = clamp_float(first_or_default(fuel_flow, sizes[DREF_FUEL_FLOW], eicas_data->engine_left.fuel_flow / XPLANE_LIVE_KG_PER_SEC_TO_LB_PER_HOUR) * XPLANE_LIVE_KG_PER_SEC_TO_LB_PER_HOUR, 0.0f, XPLANE_LIVE_MAX_FUEL_FLOW_LB_PER_HOUR);
    frame->fuel_flow_right = clamp_float(second_or_default(fuel_flow, sizes[DREF_FUEL_FLOW], eicas_data->engine_right.fuel_flow / XPLANE_LIVE_KG_PER_SEC_TO_LB_PER_HOUR) * XPLANE_LIVE_KG_PER_SEC_TO_LB_PER_HOUR, 0.0f, XPLANE_LIVE_MAX_FUEL_FLOW_LB_PER_HOUR);
    frame->oil_pressure_left = first_or_default(oil_pressure, sizes[DREF_OIL_PRESSURE], eicas_data->engine_left.oil_pressure);
    frame->oil_pressure_right = second_or_default(oil_pressure, sizes[DREF_OIL_PRESSURE], eicas_data->engine_right.oil_pressure);
    frame->oil_temperature_left = first_or_default(oil_temperature, sizes[DREF_OIL_TEMPERATURE], eicas_data->engine_left.oil_temperature);
    frame->oil_temperature_right = second_or_default(oil_temperature, sizes[DREF_OIL_TEMPERATURE], eicas_data->engine_right.oil_temperature);
    frame->oil_quantity_left = oil_quantity_display(first_or_default(oil_quantity, sizes[DREF_OIL_QUANTITY], -1.0f), eicas_data->engine_left.oil_quantity);
    frame->oil_quantity_right = oil_quantity_display(second_or_default(oil_quantity, sizes[DREF_OIL_QUANTITY], -1.0f), eicas_data->engine_right.oil_quantity);
    frame->vibration_left = b738_vibration_display_value(
        b738_vibration_left,
        sizes[DREF_B738_VIBRATION_LEFT],
        left_vibration_hz > 0.0f ? vibration_hz_to_display(left_vibration_hz) : eicas_data->engine_left.vibration);
    frame->vibration_right = b738_vibration_display_value(
        b738_vibration_right,
        sizes[DREF_B738_VIBRATION_RIGHT],
        right_vibration_hz > 0.0f ? vibration_hz_to_display(right_vibration_hz) : eicas_data->engine_right.vibration);
    frame->engine_left_running = frame->n1_left > 20.0f;
    frame->engine_right_running = frame->n1_right > 20.0f;

    if (sizes[DREF_FUEL_MASS] >= 3)
    {
        frame->fuel_left_quantity = fuel_mass[0] * XPLANE_LIVE_KG_TO_KLB;
        frame->fuel_center_quantity = fuel_mass[1] * XPLANE_LIVE_KG_TO_KLB;
        frame->fuel_right_quantity = fuel_mass[2] * XPLANE_LIVE_KG_TO_KLB;
        frame->fuel_tank_quantities_valid = 1;
    }
    else
    {
        frame->fuel_left_quantity = eicas_data->fuel_left_quantity;
        frame->fuel_center_quantity = eicas_data->fuel_center_quantity;
        frame->fuel_right_quantity = eicas_data->fuel_right_quantity;
        frame->fuel_tank_quantities_valid = 0;
    }

    frame->fuel_quantity = frame->fuel_left_quantity + frame->fuel_center_quantity + frame->fuel_right_quantity;
    frame->valid = valid_geo(frame->latitude, frame->longitude) &&
                   valid_altitude(frame->altitude) &&
                   valid_airspeed(frame->airspeed) &&
                   valid_engine(frame->n1_left) &&
                   valid_engine(frame->n1_right);

    if (!frame->valid)
    {
        return 0;
    }

    live->frame_id = frame->frame_id;
    live->last_valid_time = frame->timestamp;
    frame->last_valid_timestamp = live->last_valid_time;

    if (write_legacy_outputs)
    {
        apply_frame_to_legacy_modules(frame, pfd_data, nd_data, eicas_data, systems_data, fmc_data);
    }

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
    set_status(live, 0, 0, 0, 0);
}

static float angular_difference(float lhs, float rhs)
{
    return fabsf(normalize_signed_degrees(lhs - rhs));
}

static unsigned long long compare_float_field(
    const char *field_name,
    float legacy_value,
    float snapshot_value,
    float tolerance,
    const char *unit,
    int bit_index,
    int frame_id,
    int log_field)
{
    const float diff = strstr(field_name, "heading") != NULL || strstr(field_name, "track") != NULL
                           ? angular_difference(legacy_value, snapshot_value)
                           : fabsf(legacy_value - snapshot_value);
    const int exceeded = diff > tolerance;
    if (log_field && exceeded)
    {
        printf("X-Plane compare: field=%s old=%.6f snapshot=%.6f diff=%.6f unit=%s frame_id=%d exceeded=1\n",
               field_name,
               legacy_value,
               snapshot_value,
               diff,
               unit,
               frame_id);
    }
    return exceeded ? (1ULL << bit_index) : 0ULL;
}

static unsigned long long compare_double_field(
    const char *field_name,
    double legacy_value,
    double snapshot_value,
    double tolerance,
    const char *unit,
    int bit_index,
    int frame_id,
    int log_field)
{
    const double diff = fabs(legacy_value - snapshot_value);
    const int exceeded = diff > tolerance;
    if (log_field && exceeded)
    {
        printf("X-Plane compare: field=%s old=%.9f snapshot=%.9f diff=%.9f unit=%s frame_id=%d exceeded=1\n",
               field_name,
               legacy_value,
               snapshot_value,
               diff,
               unit,
               frame_id);
    }
    return exceeded ? (1ULL << bit_index) : 0ULL;
}

static unsigned long long compare_int_field(
    const char *field_name,
    int legacy_value,
    int snapshot_value,
    const char *unit,
    int bit_index,
    int frame_id,
    int log_field)
{
    const int diff = legacy_value - snapshot_value;
    const int exceeded = diff != 0;
    if (log_field && exceeded)
    {
        printf("X-Plane compare: field=%s old=%d snapshot=%d diff=%d unit=%s frame_id=%d exceeded=1\n",
               field_name,
               legacy_value,
               snapshot_value,
               diff,
               unit,
               frame_id);
    }
    return exceeded ? (1ULL << bit_index) : 0ULL;
}

static unsigned long long compare_xplane_frame_to_snapshot_fields(
    const SimXPlaneLiveFrame *frame,
    const SimSnapshot *snapshot,
    int log_fields)
{
    unsigned long long mask = 0ULL;
    int bit = 0;
    const int frame_id = frame != NULL ? frame->frame_id : 0;
    const int valid = frame != NULL && frame->valid && frame->connected && !frame->timed_out;

    if (frame == NULL || snapshot == NULL)
    {
        return 1ULL;
    }

    mask |= compare_float_field("PFD.IAS", frame->airspeed, snapshot->airspeed, 0.05f, "kt", bit++, frame_id, log_fields);
    mask |= compare_float_field("PFD.TAS", frame->true_air_speed, snapshot->true_air_speed, 0.05f, "kt", bit++, frame_id, log_fields);
    mask |= compare_float_field("PFD.altitude", frame->altitude, snapshot->altitude, 0.5f, "ft", bit++, frame_id, log_fields);
    mask |= compare_float_field("PFD.heading", frame->heading, snapshot->heading, 0.05f, "deg magnetic", bit++, frame_id, log_fields);
    mask |= compare_float_field("PFD.pitch", frame->pitch, snapshot->pitch, 0.01f, "deg", bit++, frame_id, log_fields);
    mask |= compare_float_field("PFD.roll", frame->roll, snapshot->roll, 0.01f, "deg", bit++, frame_id, log_fields);
    mask |= compare_float_field("PFD.vertical_speed", frame->vertical_speed, snapshot->vertical_speed, 1.0f, "fpm", bit++, frame_id, log_fields);
    mask |= compare_int_field("PFD.valid", valid, snapshot->data_valid, "bool", bit++, frame_id, log_fields);

    mask |= compare_double_field("ND.latitude", frame->latitude, snapshot->latitude, 0.000001, "deg", bit++, frame_id, log_fields);
    mask |= compare_double_field("ND.longitude", frame->longitude, snapshot->longitude, 0.000001, "deg", bit++, frame_id, log_fields);
    mask |= compare_float_field("ND.GS", frame->ground_speed, snapshot->ground_speed, 0.05f, "kt", bit++, frame_id, log_fields);
    mask |= compare_float_field("ND.TAS", frame->true_air_speed, snapshot->true_air_speed, 0.05f, "kt", bit++, frame_id, log_fields);
    mask |= compare_float_field("ND.heading", frame->heading, snapshot->heading, 0.05f, "deg magnetic", bit++, frame_id, log_fields);
    mask |= compare_float_field("ND.track", frame->track, snapshot->track, 0.05f, "deg magnetic", bit++, frame_id, log_fields);
    mask |= compare_int_field("ND.valid", valid, snapshot->data_valid, "bool", bit++, frame_id, log_fields);

    mask |= compare_float_field("EICAS.N1_L", frame->n1_left, snapshot->n1_left, 0.05f, "percent", bit++, frame_id, log_fields);
    mask |= compare_float_field("EICAS.N1_R", frame->n1_right, snapshot->n1_right, 0.05f, "percent", bit++, frame_id, log_fields);
    mask |= compare_float_field("EICAS.N2_L", frame->n2_left, snapshot->n2_left, 0.05f, "percent", bit++, frame_id, log_fields);
    mask |= compare_float_field("EICAS.N2_R", frame->n2_right, snapshot->n2_right, 0.05f, "percent", bit++, frame_id, log_fields);
    mask |= compare_float_field("EICAS.EGT_L", frame->egt_left, snapshot->egt_left, 0.5f, "C", bit++, frame_id, log_fields);
    mask |= compare_float_field("EICAS.EGT_R", frame->egt_right, snapshot->egt_right, 0.5f, "C", bit++, frame_id, log_fields);
    mask |= compare_float_field("EICAS.fuel_flow_L", frame->fuel_flow_left, snapshot->fuel_flow_left, 1.0f, "lb/h", bit++, frame_id, log_fields);
    mask |= compare_float_field("EICAS.fuel_flow_R", frame->fuel_flow_right, snapshot->fuel_flow_right, 1.0f, "lb/h", bit++, frame_id, log_fields);
    mask |= compare_float_field("EICAS.fuel_quantity", frame->fuel_quantity, snapshot->fuel_quantity, 0.01f, "display", bit++, frame_id, log_fields);
    mask |= compare_int_field("EICAS.engine_L_running", frame->engine_left_running, snapshot->engine_left_running, "bool", bit++, frame_id, log_fields);
    mask |= compare_int_field("EICAS.engine_R_running", frame->engine_right_running, snapshot->engine_right_running, "bool", bit++, frame_id, log_fields);

    mask |= compare_int_field("Systems.gear_down", frame->gear_down, snapshot->gear_down, "bool", bit++, frame_id, log_fields);
    mask |= compare_int_field("Systems.flaps_level", frame->flaps_level, snapshot->flaps_level, "deg", bit++, frame_id, log_fields);
    mask |= compare_int_field("Systems.parking_brake", frame->parking_brake_on, snapshot->parking_brake_on, "bool", bit++, frame_id, log_fields);
    mask |= compare_float_field("Systems.hydraulic_pressure", snapshot->hydraulic_pressure, snapshot->hydraulic_pressure, 0.1f, "psi", bit++, frame_id, log_fields);
    mask |= compare_float_field("Systems.cabin_pressure", snapshot->cabin_pressure, snapshot->cabin_pressure, 0.01f, "psi", bit++, frame_id, log_fields);
    mask |= compare_float_field("Systems.battery_voltage", snapshot->battery_voltage, snapshot->battery_voltage, 0.01f, "V", bit++, frame_id, log_fields);
    mask |= compare_float_field("refresh_time", frame->timestamp, snapshot->sim_time, 0.001f, "sec", bit++, frame_id, log_fields);

    return mask;
}

static void compare_xplane_frame_to_snapshot(
    XPlaneLiveData *live,
    const SimXPlaneLiveFrame *frame,
    const SimSnapshot *snapshot)
{
    int status_changed;
    int mismatch_changed;
    unsigned long long mismatch_mask;

    if (live == NULL || frame == NULL || snapshot == NULL)
    {
        return;
    }

    mismatch_mask = compare_xplane_frame_to_snapshot_fields(frame, snapshot, 0);
    status_changed = !live->compare_initialized ||
                     live->compare_connected != frame->connected ||
                     live->compare_timed_out != frame->timed_out;
    mismatch_changed = !live->compare_initialized ||
                       live->compare_mismatch_mask != mismatch_mask;

    if (!live->compare_initialized || status_changed || mismatch_changed)
    {
        printf("X-Plane compare: frame_id=%d source=%s connected=%d timeout=%d valid=%d mismatches=0x%llx\n",
               snapshot->frame_id,
               sim_snapshot_source_name(snapshot->source),
               frame->connected,
               frame->timed_out,
               snapshot->data_valid,
               mismatch_mask);
        if (mismatch_mask != 0ULL)
        {
            compare_xplane_frame_to_snapshot_fields(frame, snapshot, 1);
        }
        else if (live->compare_initialized && live->compare_mismatch_mask != 0ULL)
        {
            printf("X-Plane compare: frame_id=%d all tracked fields recovered within tolerance.\n",
                   snapshot->frame_id);
        }
        fflush(stdout);
    }

    live->compare_initialized = 1;
    live->compare_connected = frame->connected;
    live->compare_timed_out = frame->timed_out;
    live->compare_mismatch_mask = mismatch_mask;
}

static void report_xplane_snapshot_status(XPlaneLiveData *live, const SimSnapshot *snapshot)
{
    int timed_out;

    if (live == NULL || snapshot == NULL)
    {
        return;
    }

    timed_out = snapshot->timed_out;
    if (!live->compare_initialized ||
        live->compare_connected != live->connected ||
        live->compare_timed_out != timed_out)
    {
        printf("X-Plane compare: status frame_id=%d source=%s connected=%d timeout=%d valid=%d last_valid=%.2f\n",
               snapshot->frame_id,
               sim_snapshot_source_name(snapshot->source),
               live->connected,
               timed_out,
               snapshot->data_valid,
               snapshot->last_valid_timestamp);
        fflush(stdout);
    }

    live->compare_initialized = 1;
    live->compare_connected = live->connected;
    live->compare_timed_out = timed_out;
}

static void publish_connection_status_frame(XPlaneLiveData *live, SimDataCenter *sim_data_center, float delta_time)
{
    SimXPlaneLiveFrame frame;

    if (live == NULL || sim_data_center == NULL)
    {
        return;
    }

    memset(&frame, 0, sizeof(frame));
    frame.connected = live->connected;
    frame.timed_out = !live->connected;
    frame.frame_id = live->frame_id;
    frame.timestamp = live->elapsed_time;
    frame.delta_time = delta_time;
    frame.last_valid_timestamp = live->last_valid_time;
    sim_data_center_apply_xplane_live_frame(sim_data_center, &frame);
    report_xplane_snapshot_status(live, sim_data_center_snapshot(sim_data_center));
}

static int xplane_live_data_update_internal(
    XPlaneLiveData *live,
    PFD_Data *pfd_data,
    ND_Data *nd_data,
    EICAS_Data *eicas_data,
    AircraftSystems_Data *systems_data,
    FMC_Data *fmc_data,
    SimDataCenter *sim_data_center,
    float delta_time,
    int write_legacy_outputs,
    int compare_snapshot)
{
    SimXPlaneLiveFrame frame;

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
    live->elapsed_time += delta_time;

    open_socket_once(live);

    if (live->socket_open)
    {
        static int sent_hello = 0;
        if (!sent_hello)
        {
            sent_hello = 1;
            printf("X-Plane live data: socket opened, using native RREF protocol to %s:%u.\n", live->xp_ip, live->xp_port);
            fflush(stdout);
        }
    }

    if (live->connected)
    {
        live->poll_elapsed += delta_time;
        if (live->poll_elapsed < XPLANE_LIVE_POLL_INTERVAL_SEC)
        {
            publish_connection_status_frame(live, sim_data_center, delta_time);
            return live->connected;
        }
        live->poll_elapsed = 0.0f;
    }
    else
    {
        live->retry_elapsed += delta_time;
        if (live->retry_elapsed < XPLANE_LIVE_RETRY_INTERVAL_SEC)
        {
            publish_connection_status_frame(live, sim_data_center, delta_time);
            return 0;
        }
        live->retry_elapsed = 0.0f;
    }

    memset(&frame, 0, sizeof(frame));
    const int ok = poll_all_data(
        live,
        pfd_data,
        nd_data,
        eicas_data,
        systems_data,
        fmc_data,
        &frame,
        delta_time,
        write_legacy_outputs);
    if (ok)
    {
        live->missed_frames = 0;
        set_status(live, 1, 1, 1, fmc_data != NULL);
        frame.connected = live->connected;
        frame.timed_out = 0;
        if (fmc_data != NULL)
        {
            fmc_data->live_data_active = 1;
        }
        if (sim_data_center != NULL)
        {
            const int applied_xplane = sim_data_center_apply_xplane_live_frame(sim_data_center, &frame);
            if (compare_snapshot && applied_xplane)
            {
                compare_xplane_frame_to_snapshot(live, &frame, sim_data_center_snapshot(sim_data_center));
            }
        }
    }
    else
    {
        live->missed_frames++;
        if (live->missed_frames >= XPLANE_LIVE_MAX_MISSED_FRAMES)
        {
            set_status(live, 0, 0, 0, 0);
            if (fmc_data != NULL)
            {
                fmc_data->live_data_active = 0;
            }
        }
        publish_connection_status_frame(live, sim_data_center, delta_time);
    }

    return live->connected;
}

int xplane_live_data_update(
    XPlaneLiveData *live,
    PFD_Data *pfd_data,
    ND_Data *nd_data,
    EICAS_Data *eicas_data,
    AircraftSystems_Data *systems_data,
    FMC_Data *fmc_data,
    float delta_time)
{
    return xplane_live_data_update_internal(live, pfd_data, nd_data, eicas_data, systems_data, fmc_data, NULL, delta_time, 1, 0);
}

int xplane_live_data_update_with_sim_data_center(
    XPlaneLiveData *live,
    PFD_Data *pfd_data,
    ND_Data *nd_data,
    EICAS_Data *eicas_data,
    AircraftSystems_Data *systems_data,
    SimDataCenter *sim_data_center,
    float delta_time)
{
    return xplane_live_data_update_internal(live, pfd_data, nd_data, eicas_data, systems_data, NULL, sim_data_center, delta_time, 0, 0);
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

int xplane_live_data_fmc_active(const XPlaneLiveData *live)
{
    return live != NULL && live->fmc_active;
}

int xplane_live_data_connected(const XPlaneLiveData *live)
{
    return live != NULL && live->connected;
}

int xplane_live_data_has_valid_frame(const XPlaneLiveData *live)
{
    return live != NULL && live->frame_id > 0;
}

void xplane_shared_runtime_init(
    XPlaneSharedRuntime *runtime,
    SimDataCenter *sim_data_center,
    const char *xp_ip,
    unsigned short xp_port)
{
    if (runtime == NULL)
    {
        return;
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->sim_data_center = sim_data_center;
    runtime->last_frame_id = -1;
    runtime->last_source = -1;
    runtime->last_valid = -1;
    runtime->pfd_shadow = (PFD_Data *)malloc(sizeof(*runtime->pfd_shadow));
    runtime->nd_shadow = (ND_Data *)malloc(sizeof(*runtime->nd_shadow));
    runtime->eicas_shadow = (EICAS_Data *)malloc(sizeof(*runtime->eicas_shadow));
    runtime->systems_shadow = (AircraftSystems_Data *)malloc(sizeof(*runtime->systems_shadow));
    if (runtime->pfd_shadow == NULL ||
        runtime->nd_shadow == NULL ||
        runtime->eicas_shadow == NULL ||
        runtime->systems_shadow == NULL)
    {
        printf("Shared Runtime: failed to allocate shadow data.\n");
        fflush(stdout);
        free(runtime->pfd_shadow);
        free(runtime->nd_shadow);
        free(runtime->eicas_shadow);
        free(runtime->systems_shadow);
        memset(runtime, 0, sizeof(*runtime));
        return;
    }

    pfd_data_init(runtime->pfd_shadow);
    nd_data_init(runtime->nd_shadow);
    eicas_data_init(runtime->eicas_shadow);
    aircraft_systems_data_init(runtime->systems_shadow);
    xplane_live_data_init(&runtime->live_data, xp_ip, xp_port);
    runtime->initialized = sim_data_center != NULL;
    printf("Shared Runtime: initialized SimDataCenter=%p X-Plane=%s:%u.\n",
           (void *)sim_data_center,
           runtime->live_data.xp_ip,
           runtime->live_data.xp_port);
    fflush(stdout);
}

void xplane_shared_runtime_shutdown(XPlaneSharedRuntime *runtime)
{
    if (runtime == NULL)
    {
        return;
    }

    if (runtime->initialized)
    {
        xplane_live_data_shutdown(&runtime->live_data);
    }

    free(runtime->pfd_shadow);
    free(runtime->nd_shadow);
    free(runtime->eicas_shadow);
    free(runtime->systems_shadow);
    memset(runtime, 0, sizeof(*runtime));
}

int xplane_shared_runtime_update(XPlaneSharedRuntime *runtime, float delta_time)
{
    const SimSnapshot *snapshot;

    if (runtime == NULL || !runtime->initialized || runtime->sim_data_center == NULL ||
        runtime->pfd_shadow == NULL ||
        runtime->nd_shadow == NULL ||
        runtime->eicas_shadow == NULL ||
        runtime->systems_shadow == NULL)
    {
        return 0;
    }

    xplane_live_data_update_with_sim_data_center(
        &runtime->live_data,
        runtime->pfd_shadow,
        runtime->nd_shadow,
        runtime->eicas_shadow,
        runtime->systems_shadow,
        runtime->sim_data_center,
        delta_time);

    snapshot = sim_data_center_snapshot(runtime->sim_data_center);
    if (snapshot != NULL &&
        (runtime->last_frame_id > snapshot->frame_id ||
         runtime->last_source != (int)snapshot->source ||
         runtime->last_valid != snapshot->data_valid))
    {
        printf("Shared Runtime: frame_id=%d source=%s valid=%d fallback=%d xplane_connected=%d timeout=%d.\n",
               snapshot->frame_id,
               sim_snapshot_source_name(snapshot->source),
               snapshot->data_valid,
               snapshot->fallback_active,
               snapshot->xplane_connected,
               snapshot->timed_out);
        fflush(stdout);
    }
    if (snapshot != NULL)
    {
        runtime->last_frame_id = snapshot->frame_id;
        runtime->last_source = (int)snapshot->source;
        runtime->last_valid = snapshot->data_valid;
    }

    return snapshot != NULL && snapshot->data_valid;
}

SimDataCenter *xplane_shared_runtime_data_center(XPlaneSharedRuntime *runtime)
{
    return runtime != NULL ? runtime->sim_data_center : NULL;
}

const SimSnapshot *xplane_shared_runtime_snapshot(const XPlaneSharedRuntime *runtime)
{
    return runtime != NULL && runtime->sim_data_center != NULL
               ? sim_data_center_snapshot(runtime->sim_data_center)
               : NULL;
}

int xplane_shared_runtime_initialized(const XPlaneSharedRuntime *runtime)
{
    return runtime != NULL && runtime->initialized && runtime->sim_data_center != NULL;
}
