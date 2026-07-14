#include "pfd_data.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PFD_DATA_FILE_PATH "assets/pfd.dat"
#define PFD_MAX_FILE_SAMPLES 12000
#define PFD_FILE_SAMPLE_INTERVAL 0.0333333f

typedef struct PFD_FileSample
{
    float airspeed_current;
    float airspeed_target;
    float altitude;
    float altitude_target;
    float agl_altitude;
    float pitch;
    float roll;
    float vertical_speed;
    float heading;
    float throttle;
    float yaw;
} PFD_FileSample;

static PFD_FileSample file_samples[PFD_MAX_FILE_SAMPLES];
static int file_sample_count = 0;
static int file_load_attempted = 0;
static int file_load_ok = 0;

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

static float normalize_heading(float heading)
{
    while (heading >= 360.0f)
    {
        heading -= 360.0f;
    }
    while (heading < 0.0f)
    {
        heading += 360.0f;
    }
    return heading;
}

static int parse_numeric_line(const char *line, float values[], int max_values)
{
    int count = 0;
    const char *cursor = line;

    while (*cursor != '\0' && count < max_values)
    {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',' || *cursor == ';')
        {
            cursor++;
        }

        if (*cursor == '\0' || *cursor == '\n' || *cursor == '\r')
        {
            break;
        }

        char *end_ptr = NULL;
        const float value = strtof(cursor, &end_ptr);
        if (end_ptr == cursor)
        {
            return -1;
        }

        values[count++] = value;
        cursor = end_ptr;

        while (*cursor == ' ' || *cursor == '\t')
        {
            cursor++;
        }

        if (*cursor != ',' && *cursor != ';' && *cursor != '\0' && *cursor != '\n' && *cursor != '\r')
        {
            while (*cursor != '\0' && *cursor != '\n' && *cursor != '\r' && *cursor != ',' && *cursor != ';')
            {
                cursor++;
            }
        }
    }

    return count;
}

static void apply_file_sample(PFD_Data *data, const PFD_FileSample *sample)
{
    if (data == NULL || sample == NULL)
    {
        return;
    }

    data->airspeed_current = sample->airspeed_current;
    data->airspeed_target = sample->airspeed_target;
    data->altitude = sample->altitude;
    data->altitude_target = sample->altitude_target;
    data->agl_altitude = sample->agl_altitude;
    data->pitch = sample->pitch;
    data->roll = sample->roll;
    data->yaw = sample->yaw;
    data->vertical_speed = sample->vertical_speed;
    data->heading = normalize_heading(sample->heading);
    data->heading_target = data->heading;
    data->throttle = clamp_float(sample->throttle, 0.0f, 100.0f);
    data->autopilot_on = 1;
    snprintf(data->flight_mode, sizeof(data->flight_mode), "%s", "PFD DATA");
}

static void load_file_samples_once(void)
{
    if (file_load_attempted)
    {
        return;
    }

    file_load_attempted = 1;

    FILE *file = fopen(PFD_DATA_FILE_PATH, "r");
    if (file == NULL)
    {
        printf("PFD data: failed to open %s, fallback to mock data.\n", PFD_DATA_FILE_PATH);
        fflush(stdout);
        return;
    }

    char line[512];
    int line_number = 0;
    while (fgets(line, sizeof(line), file) != NULL && file_sample_count < PFD_MAX_FILE_SAMPLES)
    {
        line_number++;

        const char *trim = line;
        while (*trim == ' ' || *trim == '\t')
        {
            trim++;
        }
        if (*trim == '\0' || *trim == '\n' || *trim == '\r' || *trim == '#')
        {
            continue;
        }

        float values[16];
        const int count = parse_numeric_line(line, values, 16);
        if (count == 11)
        {
            PFD_FileSample *sample = &file_samples[file_sample_count++];
            sample->airspeed_current = values[0];
            sample->airspeed_target = values[1];
            sample->altitude = values[2];
            sample->altitude_target = values[3];
            sample->agl_altitude = values[4];
            sample->pitch = values[5];
            sample->roll = values[6];
            sample->vertical_speed = values[7];
            sample->heading = values[8];
            sample->throttle = values[9];
            sample->yaw = values[10];
        }
        else
        {
            printf("PFD data: ignored line %d in %s, expected 11 numeric fields but got %d.\n",
                   line_number, PFD_DATA_FILE_PATH, count);
            fflush(stdout);
        }
    }

    fclose(file);

    if (file_sample_count > 0)
    {
        file_load_ok = 1;
        printf("PFD data: loaded %d samples from %s.\n", file_sample_count, PFD_DATA_FILE_PATH);
        printf("PFD data: format detected as comma-separated numeric rows with 11 fields.\n");
        fflush(stdout);
    }
    else
    {
        printf("PFD data: no valid samples in %s, fallback to mock data.\n", PFD_DATA_FILE_PATH);
        fflush(stdout);
    }
}

void pfd_data_init(PFD_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->pitch = 0.0f;
    data->roll = 0.0f;
    data->yaw = 0.0f;
    data->altitude = 12000.0f;
    data->agl_altitude = 11500.0f;
    data->throttle = 65.0f;
    data->airspeed_current = 245.0f;
    data->airspeed_target = 250.0f;
    data->vertical_speed = 0.0f;
    data->heading = 90.0f;
    data->heading_target = 90.0f;
    data->altitude_target = 12000.0f;
    data->autopilot_on = 1;
    snprintf(data->flight_mode, sizeof(data->flight_mode), "%s", "LNAV VNAV");
    data->simulation_time = 0.0f;
    data->using_file_data = 0;
    data->file_sample_index = 0;
    data->file_sample_accumulator = 0.0f;
    data->snapshot_frame_id = 0;
    data->data_valid = 1;

    load_file_samples_once();
    if (file_load_ok)
    {
        data->using_file_data = 1;
        apply_file_sample(data, &file_samples[0]);
    }
}

void pfd_data_update_mock(PFD_Data *data, float delta_time)
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

    if (data->using_file_data && file_load_ok && file_sample_count > 0)
    {
        data->file_sample_accumulator += delta_time;
        while (data->file_sample_accumulator >= PFD_FILE_SAMPLE_INTERVAL)
        {
            data->file_sample_accumulator -= PFD_FILE_SAMPLE_INTERVAL;
            data->file_sample_index++;
            if (data->file_sample_index >= file_sample_count)
            {
                data->file_sample_index = 0;
            }
        }

        apply_file_sample(data, &file_samples[data->file_sample_index]);
        return;
    }

    const float t = data->simulation_time;
    data->airspeed_current = 250.0f + 28.0f * sinf(t * 0.55f) + 6.0f * cosf(t * 1.35f);
    data->airspeed_target = 250.0f;
    data->altitude = 12000.0f + 850.0f * sinf(t * 0.22f) + 120.0f * cosf(t * 0.70f);
    data->agl_altitude = data->altitude - 500.0f;
    data->altitude_target = 12000.0f;
    data->vertical_speed = 900.0f * cosf(t * 0.48f) + 180.0f * sinf(t * 1.25f);
    data->pitch = 4.5f * sinf(t * 0.62f);
    data->roll = 24.0f * sinf(t * 0.38f) + 4.0f * cosf(t * 0.90f);
    data->yaw = 8.0f * sinf(t * 0.52f);
    data->throttle = clamp_float(64.0f + 16.0f * sinf(t * 0.42f) + 5.0f * cosf(t * 1.10f), 0.0f, 100.0f);

    data->heading += (18.0f + 6.0f * sinf(t * 0.30f)) * delta_time;
    data->heading = normalize_heading(data->heading);
    data->heading_target = data->heading;

    data->autopilot_on = ((int)(t / 12.0f) % 2) == 0;
    snprintf(data->flight_mode, sizeof(data->flight_mode), "%s",
             data->autopilot_on ? "LNAV VNAV" : "HDG HOLD");
}

int getPFDData(PFDData *data)
{
    static PFDData current_data;
    static int initialized = 0;

    if (data == NULL)
    {
        return -1;
    }

    if (!initialized)
    {
        pfd_data_init(&current_data);
        initialized = 1;
    }

    pfd_data_update_mock(&current_data, PFD_FILE_SAMPLE_INTERVAL);
    *data = current_data;
    return 0;
}
