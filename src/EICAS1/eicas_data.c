#include "eicas_data.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EICAS1_FIELD_COUNT 10
#define EICAS2_FIELD_COUNT 12
#define EICAS_UPPER_FF_SCALE 500.0f
#define EICAS_LOWER_FF_SCALE 350.0f
#define EICAS_FUEL_CENTER_DISPLAY_SCALE 60.9f
#define EICAS_FUEL_SIDE_DISPLAY_SCALE 48.7f
#define EICAS_ENGINE_RUNNING_N1_THRESHOLD 20.0f
#define EICAS_ENGINE_RUNNING_N2_THRESHOLD 20.0f
#define EICAS_LOW_FUEL_THRESHOLD 20.0f
#define EICAS_LOW_OIL_PRESSURE_THRESHOLD 35.0f
#define EICAS_HIGH_EGT_THRESHOLD 820.0f
#define EICAS_LOW_HYDRAULIC_PRESSURE_THRESHOLD 2500.0f
#define EICAS_LOW_BATTERY_VOLTAGE_THRESHOLD 24.0f
#define EICAS_HIGH_BATTERY_VOLTAGE_THRESHOLD 30.0f
#define EICAS_MAX_NORMAL_FLAPS_LEVEL 30

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

static AircraftSystems_WarningLevel to_aircraft_warning_level(EICAS_WarningLevel level)
{
    switch (level)
    {
    case EICAS_WARNING_WARNING:
        return AIRCRAFT_SYSTEMS_WARNING_WARNING;
    case EICAS_WARNING_CAUTION:
        return AIRCRAFT_SYSTEMS_WARNING_CAUTION;
    case EICAS_WARNING_INFO:
    default:
        return AIRCRAFT_SYSTEMS_WARNING_INFO;
    }
}

static void copy_warnings_to_aircraft_systems(const EICAS_Data *data, AircraftSystems_Data *systems)
{
    if (data == NULL || systems == NULL)
    {
        return;
    }

    systems->warning_count = 0;
    for (int i = 0; i < data->warning_count && i < AIRCRAFT_SYSTEMS_MAX_WARNINGS; ++i)
    {
        snprintf(systems->warnings[i].text, sizeof(systems->warnings[i].text), "%s", data->warnings[i].text);
        systems->warnings[i].level = to_aircraft_warning_level(data->warnings[i].level);
        systems->warnings[i].active = data->warnings[i].active;
        ++systems->warning_count;
    }
}

static void init_engine(EICAS_EngineData *engine, float n1, float n2, float egt)
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
    engine->oil_temperature = 90.2f;
    engine->oil_quantity = 12.0f;
    engine->vibration = 1.0f;
    engine->running = 1;
}

static void clear_warnings(EICAS_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->warning_count = 0;
    for (int i = 0; i < EICAS_DATA_MAX_WARNINGS; ++i)
    {
        data->warnings[i].text[0] = '\0';
        data->warnings[i].level = EICAS_WARNING_INFO;
        data->warnings[i].active = 0;
    }
}

static void add_warning(EICAS_Data *data, const char *text, EICAS_WarningLevel level)
{
    if (data == NULL || text == NULL || data->warning_count >= EICAS_DATA_MAX_WARNINGS)
    {
        return;
    }

    EICAS_WarningItem *item = &data->warnings[data->warning_count++];
    snprintf(item->text, sizeof(item->text), "%s", text);
    item->level = level;
    item->active = 1;
}

static void update_warnings(EICAS_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    clear_warnings(data);

    if (data->engine_left.oil_pressure < EICAS_LOW_OIL_PRESSURE_THRESHOLD)
    {
        add_warning(data, "ENG 1 OIL PRESS", EICAS_WARNING_WARNING);
    }

    if (data->engine_right.oil_pressure < EICAS_LOW_OIL_PRESSURE_THRESHOLD)
    {
        add_warning(data, "ENG 2 OIL PRESS", EICAS_WARNING_WARNING);
    }

    if (data->engine_left.egt > EICAS_HIGH_EGT_THRESHOLD)
    {
        add_warning(data, "ENG 1 OVERHEAT", EICAS_WARNING_WARNING);
    }

    if (data->engine_right.egt > EICAS_HIGH_EGT_THRESHOLD)
    {
        add_warning(data, "ENG 2 OVERHEAT", EICAS_WARNING_WARNING);
    }

    if (data->fuel_quantity < EICAS_LOW_FUEL_THRESHOLD)
    {
        add_warning(data, "LOW FUEL", EICAS_WARNING_CAUTION);
    }

    if (data->hydraulic_pressure < EICAS_LOW_HYDRAULIC_PRESSURE_THRESHOLD)
    {
        add_warning(data, "HYD PRESS", EICAS_WARNING_CAUTION);
    }

    if (data->battery_voltage < EICAS_LOW_BATTERY_VOLTAGE_THRESHOLD ||
        data->battery_voltage > EICAS_HIGH_BATTERY_VOLTAGE_THRESHOLD)
    {
        add_warning(data, "ELEC", EICAS_WARNING_CAUTION);
    }

    if (data->flaps_level < 0 || data->flaps_level > EICAS_MAX_NORMAL_FLAPS_LEVEL ||
        (data->gear_down == 0 && data->flaps_level >= 20))
    {
        add_warning(data, "CONFIG", EICAS_WARNING_CAUTION);
    }

    if (data->warning_count == 0)
    {
        add_warning(data, "NORMAL", EICAS_WARNING_INFO);
    }
}

static void strip_comment(char *line)
{
    char *comment = strchr(line, '#');
    if (comment != NULL)
    {
        *comment = '\0';
    }

    comment = strstr(line, "//");
    if (comment != NULL)
    {
        *comment = '\0';
    }
}

static char *skip_space(char *text)
{
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
    {
        ++text;
    }

    return text;
}

static int parse_csv_floats(char *line, float *values, int expected_count)
{
    int count = 0;
    char *cursor = line;

    while (count < expected_count)
    {
        cursor = skip_space(cursor);
        if (*cursor == '\0')
        {
            break;
        }

        char *end = NULL;
        const float value = (float)strtod(cursor, &end);
        if (end == cursor)
        {
            return 0;
        }

        values[count++] = value;
        cursor = skip_space(end);

        if (count < expected_count)
        {
            if (*cursor != ',')
            {
                return 0;
            }
            ++cursor;
        }
    }

    cursor = skip_space(cursor);
    return count == expected_count && *cursor == '\0';
}

static void set_upper_frame(EICAS1_DataFrame *frame, const float *values)
{
    frame->tat = values[0];
    frame->n1_left = values[1];
    frame->n1_right = values[2];
    frame->egt_left = values[3];
    frame->egt_right = values[4];
    frame->fuel_flow_left_display = values[5];
    frame->fuel_flow_right_display = values[6];
    frame->fuel_center_quantity = values[7];
    frame->fuel_left_quantity = values[8];
    frame->fuel_right_quantity = values[9];
}

static void set_lower_frame(EICAS2_DataFrame *frame, const float *values)
{
    frame->n2_left = values[0];
    frame->n2_right = values[1];
    frame->fuel_flow_left_display = values[2];
    frame->fuel_flow_right_display = values[3];
    frame->oil_pressure_left = values[4];
    frame->oil_pressure_right = values[5];
    frame->oil_temperature_left = values[6];
    frame->oil_temperature_right = values[7];
    frame->oil_quantity_left = values[8];
    frame->oil_quantity_right = values[9];
    frame->vibration_left = values[10];
    frame->vibration_right = values[11];
}

static float upper_frame_to_system_fuel_quantity(const EICAS1_DataFrame *frame);

static void cache_upper_summary(EICAS_Data *data, const EICAS1_DataFrame *frame)
{
    data->tat = frame->tat;
    data->fuel_center_quantity = frame->fuel_center_quantity;
    data->fuel_left_quantity = frame->fuel_left_quantity;
    data->fuel_right_quantity = frame->fuel_right_quantity;
    data->fuel_total_quantity = frame->fuel_center_quantity + frame->fuel_left_quantity + frame->fuel_right_quantity;
    data->fuel_quantity = upper_frame_to_system_fuel_quantity(frame);
}

static float upper_frame_to_system_fuel_quantity(const EICAS1_DataFrame *frame)
{
    const float total_scale = EICAS_FUEL_CENTER_DISPLAY_SCALE + EICAS_FUEL_SIDE_DISPLAY_SCALE * 2.0f;
    if (frame == NULL || total_scale <= 0.0f)
    {
        return 0.0f;
    }

    return (frame->fuel_center_quantity + frame->fuel_left_quantity + frame->fuel_right_quantity) / total_scale;
}

static void apply_upper_frame_to_data(EICAS_Data *data, const EICAS1_DataFrame *frame)
{
    if (data == NULL || frame == NULL)
    {
        return;
    }

    data->tat = frame->tat;
    data->engine_left.n1 = frame->n1_left;
    data->engine_right.n1 = frame->n1_right;
    data->engine_left.egt = frame->egt_left;
    data->engine_right.egt = frame->egt_right;
    data->engine_left.fuel_flow = frame->fuel_flow_left_display * EICAS_UPPER_FF_SCALE;
    data->engine_right.fuel_flow = frame->fuel_flow_right_display * EICAS_UPPER_FF_SCALE;
    data->engine_left.running = frame->n1_left > EICAS_ENGINE_RUNNING_N1_THRESHOLD;
    data->engine_right.running = frame->n1_right > EICAS_ENGINE_RUNNING_N1_THRESHOLD;
    cache_upper_summary(data, frame);
}

static void apply_lower_frame_to_data(EICAS_Data *data, const EICAS2_DataFrame *frame)
{
    if (data == NULL || frame == NULL)
    {
        return;
    }

    data->engine_left.n2 = frame->n2_left;
    data->engine_right.n2 = frame->n2_right;
    if (!data->upper_loaded)
    {
        data->engine_left.fuel_flow = frame->fuel_flow_left_display * EICAS_LOWER_FF_SCALE;
        data->engine_right.fuel_flow = frame->fuel_flow_right_display * EICAS_LOWER_FF_SCALE;
    }
    data->engine_left.oil_pressure = frame->oil_pressure_left;
    data->engine_right.oil_pressure = frame->oil_pressure_right;
    data->engine_left.oil_temperature = frame->oil_temperature_left;
    data->engine_right.oil_temperature = frame->oil_temperature_right;
    data->engine_left.oil_quantity = frame->oil_quantity_left;
    data->engine_right.oil_quantity = frame->oil_quantity_right;
    data->engine_left.vibration = frame->vibration_left;
    data->engine_right.vibration = frame->vibration_right;
    data->engine_left.running = data->engine_left.running || frame->n2_left > EICAS_ENGINE_RUNNING_N2_THRESHOLD;
    data->engine_right.running = data->engine_right.running || frame->n2_right > EICAS_ENGINE_RUNNING_N2_THRESHOLD;
}

static void update_engine_mock(EICAS_EngineData *engine, float t, float phase)
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
    engine->oil_temperature = clamp_float(oil_temp_base + 2.4f * sinf(t * 0.25f + phase * 0.8f), 0.0f, 180.0f);
    engine->oil_quantity = clamp_float(12.0f + 0.2f * sinf(t * 0.18f + phase * 0.7f), 0.0f, 25.0f);
    engine->vibration = clamp_float(vib_base + 0.1f * sinf(t * 0.70f + phase) + 0.03f * cosf(t * 1.35f), 0.0f, 5.0f);
    engine->running = engine->n1 > EICAS_ENGINE_RUNNING_N1_THRESHOLD || engine->n2 > EICAS_ENGINE_RUNNING_N2_THRESHOLD;
}

void eicas_data_init(EICAS_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    memset(data, 0, sizeof(*data));
    data->frame_step_sec = EICAS_DATA_DEFAULT_STEP_SEC;
    init_engine(&data->engine_left, 63.0f, 70.5f, 663.0f);
    init_engine(&data->engine_right, 67.0f, 73.0f, 672.0f);
    data->engine_right.fuel_flow = 4900.0f;
    data->engine_right.oil_pressure = 40.9f;
    data->engine_right.oil_temperature = 91.5f;
    data->engine_right.vibration = 1.1f;

    data->tat = 11.9f;
    data->fuel_quantity = 82.0f;
    data->fuel_center_quantity = data->fuel_quantity * EICAS_FUEL_CENTER_DISPLAY_SCALE;
    data->fuel_left_quantity = data->fuel_quantity * EICAS_FUEL_SIDE_DISPLAY_SCALE;
    data->fuel_right_quantity = data->fuel_quantity * EICAS_FUEL_SIDE_DISPLAY_SCALE;
    data->fuel_total_quantity = data->fuel_center_quantity + data->fuel_left_quantity + data->fuel_right_quantity;
    data->hydraulic_pressure = 3050.0f;
    data->cabin_pressure = 8.2f;
    data->battery_voltage = 27.8f;
    data->gear_down = 0;
    data->flaps_level = 5;
    data->parking_brake_on = 0;

    clear_warnings(data);
    update_warnings(data);
}

void eicas_data_update_mock(EICAS_Data *data, float delta_time)
{
    if (data == NULL)
    {
        return;
    }

    if (delta_time < 0.0f)
    {
        delta_time = 0.0f;
    }
    if (delta_time > 0.1f)
    {
        delta_time = 0.1f;
    }

    data->simulation_time += delta_time;
    const float t = data->simulation_time;

    update_engine_mock(&data->engine_left, t, 0.0f);
    update_engine_mock(&data->engine_right, t, 0.85f);

    data->tat = 11.9f + 0.2f * sinf(t * 0.25f);
    data->fuel_quantity = clamp_float(data->fuel_quantity - 0.010f * delta_time, 0.0f, 100.0f);
    data->fuel_center_quantity = data->fuel_quantity * EICAS_FUEL_CENTER_DISPLAY_SCALE;
    data->fuel_left_quantity = data->fuel_quantity * EICAS_FUEL_SIDE_DISPLAY_SCALE;
    data->fuel_right_quantity = data->fuel_quantity * EICAS_FUEL_SIDE_DISPLAY_SCALE;
    data->fuel_total_quantity = data->fuel_center_quantity + data->fuel_left_quantity + data->fuel_right_quantity;
    data->hydraulic_pressure = clamp_float(3020.0f + 90.0f * sinf(t * 0.40f), 0.0f, 3500.0f);
    data->cabin_pressure = clamp_float(8.1f + 0.3f * sinf(t * 0.18f), 0.0f, 12.0f);
    data->battery_voltage = clamp_float(27.6f + 0.5f * sinf(t * 0.50f), 0.0f, 32.0f);
    data->gear_down = ((int)(t / 24.0f) % 2) == 1;
    data->flaps_level = ((int)(t / 8.0f) % 4) * 5;
    data->parking_brake_on = ((int)(t / 18.0f) % 4) == 0 && t > 2.0f;

    update_warnings(data);
}

int eicas_data_load_upper_file(EICAS_Data *data, const char *path)
{
    if (data == NULL || path == NULL)
    {
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        printf("EICAS upper data: failed to open %s.\n", path);
        fflush(stdout);
        return 0;
    }

    char line[512];
    int loaded_count = 0;
    int ignored_count = 0;
    int line_number = 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        ++line_number;
        strip_comment(line);

        char *content = skip_space(line);
        if (*content == '\0')
        {
            continue;
        }

        float values[EICAS1_FIELD_COUNT];
        if (!parse_csv_floats(content, values, EICAS1_FIELD_COUNT))
        {
            ++ignored_count;
            continue;
        }

        if (loaded_count >= EICAS_DATA_MAX_FRAMES)
        {
            break;
        }

        set_upper_frame(&data->upper_frames[loaded_count++], values);
    }

    fclose(file);

    data->upper_frame_count = loaded_count;
    data->upper_frame_index = 0;
    data->upper_loaded = loaded_count > 0;

    if (data->upper_loaded)
    {
        apply_upper_frame_to_data(data, &data->upper_frames[0]);
        update_warnings(data);
        printf("EICAS upper data: loaded %d frames from %s (%d ignored rows).\n", loaded_count, path, ignored_count);
        fflush(stdout);
    }
    else
    {
        printf("EICAS upper data: no usable frames in %s (%d rows checked, %d ignored rows).\n",
               path,
               line_number,
               ignored_count);
        fflush(stdout);
    }

    return data->upper_loaded;
}

int eicas_data_load_lower_file(EICAS_Data *data, const char *path)
{
    if (data == NULL || path == NULL)
    {
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        printf("EICAS lower data: failed to open %s.\n", path);
        fflush(stdout);
        return 0;
    }

    char line[512];
    int loaded_count = 0;
    int ignored_count = 0;
    int line_number = 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        ++line_number;
        strip_comment(line);

        char *content = skip_space(line);
        if (*content == '\0')
        {
            continue;
        }

        float values[EICAS2_FIELD_COUNT];
        if (!parse_csv_floats(content, values, EICAS2_FIELD_COUNT))
        {
            ++ignored_count;
            continue;
        }

        if (loaded_count >= EICAS_DATA_MAX_FRAMES)
        {
            break;
        }

        set_lower_frame(&data->lower_frames[loaded_count++], values);
    }

    fclose(file);

    data->lower_frame_count = loaded_count;
    data->lower_frame_index = 0;
    data->lower_loaded = loaded_count > 0;

    if (data->lower_loaded)
    {
        apply_lower_frame_to_data(data, &data->lower_frames[0]);
        update_warnings(data);
        printf("EICAS lower data: loaded %d frames from %s (%d ignored rows).\n", loaded_count, path, ignored_count);
        fflush(stdout);
    }
    else
    {
        printf("EICAS lower data: no usable frames in %s (%d rows checked, %d ignored rows).\n",
               path,
               line_number,
               ignored_count);
        fflush(stdout);
    }

    return data->lower_loaded;
}

int eicas_data_load_files(EICAS_Data *data, const char *upper_path, const char *lower_path)
{
    if (data == NULL)
    {
        return 0;
    }

    const int upper_ok = eicas_data_load_upper_file(data, upper_path);
    const int lower_ok = eicas_data_load_lower_file(data, lower_path);
    return upper_ok || lower_ok;
}

void eicas_data_update(EICAS_Data *data, float delta_time)
{
    if (data == NULL)
    {
        return;
    }

    if (delta_time < 0.0f)
    {
        delta_time = 0.0f;
    }

    data->frame_elapsed += delta_time;
    while (data->frame_elapsed >= data->frame_step_sec)
    {
        data->frame_elapsed -= data->frame_step_sec;

        if (data->upper_frame_count > 0)
        {
            data->upper_frame_index = (data->upper_frame_index + 1) % data->upper_frame_count;
            apply_upper_frame_to_data(data, &data->upper_frames[data->upper_frame_index]);
        }

        if (data->lower_frame_count > 0)
        {
            data->lower_frame_index = (data->lower_frame_index + 1) % data->lower_frame_count;
            apply_lower_frame_to_data(data, &data->lower_frames[data->lower_frame_index]);
        }

        update_warnings(data);
    }
}

const EICAS1_DataFrame *eicas_data_current_upper_frame(const EICAS_Data *data)
{
    if (data == NULL || data->upper_frame_count <= 0)
    {
        return NULL;
    }

    return &data->upper_frames[data->upper_frame_index];
}

const EICAS2_DataFrame *eicas_data_current_lower_frame(const EICAS_Data *data)
{
    if (data == NULL || data->lower_frame_count <= 0)
    {
        return NULL;
    }

    return &data->lower_frames[data->lower_frame_index];
}

void eicas_data_apply_upper_to_aircraft_systems(const EICAS_Data *data, AircraftSystems_Data *systems)
{
    if (data == NULL || systems == NULL)
    {
        return;
    }

    systems->engine_left.n1 = data->engine_left.n1;
    systems->engine_right.n1 = data->engine_right.n1;
    systems->engine_left.egt = data->engine_left.egt;
    systems->engine_right.egt = data->engine_right.egt;
    systems->engine_left.fuel_flow = data->engine_left.fuel_flow;
    systems->engine_right.fuel_flow = data->engine_right.fuel_flow;
    systems->engine_left.running = data->engine_left.running;
    systems->engine_right.running = data->engine_right.running;
    systems->total_air_temperature = data->tat;
    systems->fuel_quantity = data->fuel_quantity;
    copy_warnings_to_aircraft_systems(data, systems);
}

void eicas_data_apply_lower_to_aircraft_systems(const EICAS_Data *data, AircraftSystems_Data *systems)
{
    if (data == NULL || systems == NULL)
    {
        return;
    }

    const EICAS2_DataFrame *frame = eicas_data_current_lower_frame(data);
    systems->engine_left.n2 = data->engine_left.n2;
    systems->engine_right.n2 = data->engine_right.n2;
    if (frame != NULL)
    {
        systems->engine_left.fuel_flow = frame->fuel_flow_left_display * EICAS_LOWER_FF_SCALE;
        systems->engine_right.fuel_flow = frame->fuel_flow_right_display * EICAS_LOWER_FF_SCALE;
    }
    else
    {
        systems->engine_left.fuel_flow = data->engine_left.fuel_flow;
        systems->engine_right.fuel_flow = data->engine_right.fuel_flow;
    }
    systems->engine_left.oil_pressure = data->engine_left.oil_pressure;
    systems->engine_right.oil_pressure = data->engine_right.oil_pressure;
    systems->engine_left.oil_temp = data->engine_left.oil_temperature;
    systems->engine_right.oil_temp = data->engine_right.oil_temperature;
    systems->engine_left.oil_quantity = data->engine_left.oil_quantity;
    systems->engine_right.oil_quantity = data->engine_right.oil_quantity;
    systems->engine_left.vibration = data->engine_left.vibration;
    systems->engine_right.vibration = data->engine_right.vibration;
    systems->engine_left.running = data->engine_left.running;
    systems->engine_right.running = data->engine_right.running;
    copy_warnings_to_aircraft_systems(data, systems);
}

void eicas_data_apply_to_aircraft_systems(const EICAS_Data *data, AircraftSystems_Data *systems)
{
    if (data == NULL || systems == NULL)
    {
        return;
    }

    systems->engine_left.n1 = data->engine_left.n1;
    systems->engine_left.n2 = data->engine_left.n2;
    systems->engine_left.egt = data->engine_left.egt;
    systems->engine_left.fuel_flow = data->engine_left.fuel_flow;
    systems->engine_left.oil_pressure = data->engine_left.oil_pressure;
    systems->engine_left.oil_temp = data->engine_left.oil_temperature;
    systems->engine_left.oil_quantity = data->engine_left.oil_quantity;
    systems->engine_left.vibration = data->engine_left.vibration;
    systems->engine_left.running = data->engine_left.running;

    systems->engine_right.n1 = data->engine_right.n1;
    systems->engine_right.n2 = data->engine_right.n2;
    systems->engine_right.egt = data->engine_right.egt;
    systems->engine_right.fuel_flow = data->engine_right.fuel_flow;
    systems->engine_right.oil_pressure = data->engine_right.oil_pressure;
    systems->engine_right.oil_temp = data->engine_right.oil_temperature;
    systems->engine_right.oil_quantity = data->engine_right.oil_quantity;
    systems->engine_right.vibration = data->engine_right.vibration;
    systems->engine_right.running = data->engine_right.running;

    systems->total_air_temperature = data->tat;
    systems->fuel_quantity = data->fuel_quantity;
    systems->hydraulic_pressure = data->hydraulic_pressure;
    systems->cabin_pressure = data->cabin_pressure;
    systems->battery_voltage = data->battery_voltage;
    systems->gear_down = data->gear_down;
    systems->flaps_level = data->flaps_level;
    systems->parking_brake_on = data->parking_brake_on;

    copy_warnings_to_aircraft_systems(data, systems);
}
