#include "aircraft_systems_data.h"

#include <math.h>
#include <stdio.h>

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

static void init_engine(AircraftSystems_EngineData *engine, float n1, float n2, float egt)
{
    if (engine == NULL)
    {
        return;
    }

    engine->n1 = n1;
    engine->n2 = n2;
    engine->egt = egt;
    engine->fuel_flow = 2450.0f;
    engine->oil_pressure = 40.2f;
    engine->oil_temp = 90.2f;
    engine->oil_quantity = 12.0f;
    engine->vibration = 1.0f;
    engine->running = 1;
}

static void update_engine_mock(AircraftSystems_EngineData *engine, float t, float phase)
{
    if (engine == NULL)
    {
        return;
    }

    const int right_engine = phase > 0.1f;
    const float n1_base = right_engine ? 67.0f : 63.0f;
    const float n2_base = right_engine ? 73.0f : 70.5f;
    const float egt_base = right_engine ? 672.0f : 663.0f;
    const float fuel_base = right_engine ? 4900.0f : 2350.0f;
    const float oil_press_base = right_engine ? 40.9f : 40.2f;
    const float oil_temp_base = right_engine ? 91.5f : 90.2f;
    const float vib_base = right_engine ? 1.1f : 1.0f;

    engine->n1 = clamp_float(n1_base + 2.0f * sinf(t * 0.42f + phase) + 0.6f * cosf(t * 1.10f), 0.0f, 110.0f);
    engine->n2 = clamp_float(n2_base + 1.6f * sinf(t * 0.38f + phase * 0.6f), 0.0f, 110.0f);
    engine->egt = clamp_float(egt_base + 14.0f * sinf(t * 0.32f + phase) + 3.0f * cosf(t * 0.95f), 200.0f, 980.0f);
    engine->fuel_flow = clamp_float(fuel_base + 120.0f * sinf(t * 0.48f + phase) + 35.0f * cosf(t * 1.20f), 0.0f, 6500.0f);
    engine->oil_pressure = clamp_float(oil_press_base + 1.4f * sinf(t * 0.55f + phase), 0.0f, 100.0f);
    engine->oil_temp = clamp_float(oil_temp_base + 2.4f * sinf(t * 0.25f + phase * 0.8f), 0.0f, 180.0f);
    engine->oil_quantity = clamp_float(12.0f + 0.2f * sinf(t * 0.18f + phase * 0.7f), 0.0f, 25.0f);
    engine->vibration = clamp_float(vib_base + 0.1f * sinf(t * 0.70f + phase) + 0.03f * cosf(t * 1.35f), 0.0f, 5.0f);
    engine->running = engine->n1 > 20.0f;
}

static void clear_warnings(AircraftSystems_Data *data)
{
    data->warning_count = 0;
    for (int i = 0; i < AIRCRAFT_SYSTEMS_MAX_WARNINGS; ++i)
    {
        data->warnings[i].text[0] = '\0';
        data->warnings[i].level = AIRCRAFT_SYSTEMS_WARNING_INFO;
        data->warnings[i].active = 0;
    }
}

static void add_warning(AircraftSystems_Data *data, const char *text, AircraftSystems_WarningLevel level)
{
    if (data == NULL || text == NULL || data->warning_count >= AIRCRAFT_SYSTEMS_MAX_WARNINGS)
    {
        return;
    }

    AircraftSystems_WarningItem *item = &data->warnings[data->warning_count++];
    snprintf(item->text, sizeof(item->text), "%s", text);
    item->level = level;
    item->active = 1;
}

static void update_warnings(AircraftSystems_Data *data)
{
    clear_warnings(data);

    if (data->fuel_quantity < 20.0f)
    {
        add_warning(data, "LOW FUEL", AIRCRAFT_SYSTEMS_WARNING_CAUTION);
    }

    if (data->engine_left.oil_pressure < 35.0f)
    {
        add_warning(data, "ENG 1 OIL PRESS", AIRCRAFT_SYSTEMS_WARNING_WARNING);
    }

    if (data->engine_right.oil_pressure < 35.0f)
    {
        add_warning(data, "ENG 2 OIL PRESS", AIRCRAFT_SYSTEMS_WARNING_WARNING);
    }

    if (data->engine_left.egt > 820.0f)
    {
        add_warning(data, "ENG 1 OVERHEAT", AIRCRAFT_SYSTEMS_WARNING_WARNING);
    }

    if (data->engine_right.egt > 820.0f)
    {
        add_warning(data, "ENG 2 OVERHEAT", AIRCRAFT_SYSTEMS_WARNING_WARNING);
    }

    if (data->hydraulic_pressure < 2500.0f)
    {
        add_warning(data, "HYD PRESS", AIRCRAFT_SYSTEMS_WARNING_CAUTION);
    }

    if (data->parking_brake_on)
    {
        add_warning(data, "PARK BRAKE SET", AIRCRAFT_SYSTEMS_WARNING_INFO);
    }

    if (data->warning_count == 0)
    {
        add_warning(data, "NO ACTIVE WARNINGS", AIRCRAFT_SYSTEMS_WARNING_INFO);
    }
}

void aircraft_systems_data_init(AircraftSystems_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    init_engine(&data->engine_left, 63.0f, 70.5f, 663.0f);
    init_engine(&data->engine_right, 67.0f, 73.0f, 672.0f);
    data->engine_right.fuel_flow = 4900.0f;
    data->engine_right.oil_pressure = 40.9f;
    data->engine_right.oil_temp = 91.5f;
    data->engine_right.vibration = 1.1f;

    data->total_air_temperature = 11.9f;
    data->fuel_quantity = 82.0f;
    data->hydraulic_pressure = 3050.0f;
    data->cabin_pressure = 8.2f;
    data->battery_voltage = 27.8f;
    data->gear_down = 0;
    data->flaps_level = 5;
    data->parking_brake_on = 0;
    data->simulation_time = 0.0f;

    clear_warnings(data);
    update_warnings(data);
}

void aircraft_systems_data_update_mock(AircraftSystems_Data *data, float delta_time)
{
    if (data == NULL)
    {
        return;
    }

    if (delta_time < 0.0f)
    {
        delta_time = 0.0f;
    }

    data->simulation_time += delta_time;
    const float t = data->simulation_time;

    update_engine_mock(&data->engine_left, t, 0.0f);
    update_engine_mock(&data->engine_right, t, 0.85f);

    data->total_air_temperature = 11.9f + 0.2f * sinf(t * 0.25f);
    data->fuel_quantity = clamp_float(data->fuel_quantity - 0.010f * delta_time, 0.0f, 100.0f);
    data->hydraulic_pressure = clamp_float(3020.0f + 90.0f * sinf(t * 0.40f), 0.0f, 3500.0f);
    data->cabin_pressure = clamp_float(8.1f + 0.3f * sinf(t * 0.18f), 0.0f, 12.0f);
    data->battery_voltage = clamp_float(27.6f + 0.5f * sinf(t * 0.50f), 0.0f, 32.0f);

    data->gear_down = ((int)(t / 24.0f) % 2) == 1;
    data->flaps_level = ((int)(t / 8.0f) % 4) * 5;
    data->parking_brake_on = ((int)(t / 18.0f) % 4) == 0 && t > 2.0f;

    update_warnings(data);
}

