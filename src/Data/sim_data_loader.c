#include "sim_data_loader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIM_PFD_FIELD_COUNT 11
#define SIM_EICAS_UPPER_FIELD_COUNT 10
#define SIM_EICAS_LOWER_FIELD_COUNT 12

static char *trim_whitespace(char *text)
{
    if (text == NULL)
    {
        return NULL;
    }

    while (*text != '\0' && isspace((unsigned char)*text))
    {
        ++text;
    }

    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)*(end - 1)))
    {
        --end;
    }
    *end = '\0';
    return text;
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

static int contains_alpha(const char *text)
{
    if (text == NULL)
    {
        return 0;
    }

    for (int i = 0; text[i] != '\0'; ++i)
    {
        if (isalpha((unsigned char)text[i]))
        {
            return 1;
        }
    }
    return 0;
}

static void normalize_token(char *text)
{
    if (text == NULL)
    {
        return;
    }

    int write_index = 0;
    for (int read_index = 0; text[read_index] != '\0'; ++read_index)
    {
        const unsigned char ch = (unsigned char)text[read_index];
        if (isalnum(ch))
        {
            text[write_index++] = (char)tolower(ch);
        }
    }
    text[write_index] = '\0';
}

static int split_tokens(char *line, char **tokens, int max_tokens)
{
    int count = 0;
    char *token = strtok(line, ", \t\r\n");
    while (token != NULL && count < max_tokens)
    {
        tokens[count++] = trim_whitespace(token);
        token = strtok(NULL, ", \t\r\n");
    }
    return count;
}

static int parse_float_row(char *line, float *values, int expected_count)
{
    int count = 0;
    char *cursor = line;

    while (count < expected_count)
    {
        cursor = trim_whitespace(cursor);
        if (cursor == NULL || *cursor == '\0')
        {
            break;
        }

        char *end = NULL;
        const float value = strtof(cursor, &end);
        if (end == cursor)
        {
            return 0;
        }

        values[count++] = value;
        cursor = trim_whitespace(end);

        if (count < expected_count)
        {
            if (*cursor != ',' && *cursor != ';')
            {
                return 0;
            }
            ++cursor;
        }
    }

    cursor = trim_whitespace(cursor);
    return count == expected_count && cursor != NULL && *cursor == '\0';
}

static unsigned int nd_field_from_header(const char *header)
{
    char name[64];
    snprintf(name, sizeof(name), "%s", header != NULL ? header : "");
    normalize_token(name);

    if (strcmp(name, "time") == 0 || strcmp(name, "timesec") == 0 || strcmp(name, "seconds") == 0 || strcmp(name, "sec") == 0)
    {
        return SIM_ND_FIELD_TIME;
    }
    if (strcmp(name, "latitude") == 0 || strcmp(name, "lat") == 0)
    {
        return SIM_ND_FIELD_LATITUDE;
    }
    if (strcmp(name, "longitude") == 0 || strcmp(name, "lon") == 0 || strcmp(name, "lng") == 0)
    {
        return SIM_ND_FIELD_LONGITUDE;
    }
    if (strcmp(name, "heading") == 0 || strcmp(name, "hdg") == 0)
    {
        return SIM_ND_FIELD_HEADING;
    }
    if (strcmp(name, "track") == 0 || strcmp(name, "trk") == 0)
    {
        return SIM_ND_FIELD_TRACK;
    }
    if (strcmp(name, "groundspeed") == 0 || strcmp(name, "gs") == 0)
    {
        return SIM_ND_FIELD_GROUND_SPEED;
    }
    if (strcmp(name, "trueairspeed") == 0 || strcmp(name, "tas") == 0)
    {
        return SIM_ND_FIELD_TRUE_AIR_SPEED;
    }

    return 0;
}

static void set_nd_frame_value(SimNdFrame *frame, unsigned int field, double value)
{
    if (frame == NULL || field == 0)
    {
        return;
    }

    frame->fields |= field;
    switch (field)
    {
    case SIM_ND_FIELD_TIME:
        frame->time_sec = (float)value;
        break;
    case SIM_ND_FIELD_LATITUDE:
        frame->latitude = value;
        break;
    case SIM_ND_FIELD_LONGITUDE:
        frame->longitude = value;
        break;
    case SIM_ND_FIELD_HEADING:
        frame->heading = (float)value;
        break;
    case SIM_ND_FIELD_TRACK:
        frame->track = (float)value;
        break;
    case SIM_ND_FIELD_GROUND_SPEED:
        frame->ground_speed = (float)value;
        break;
    case SIM_ND_FIELD_TRUE_AIR_SPEED:
        frame->true_air_speed = (float)value;
        break;
    default:
        break;
    }
}

void sim_data_store_init(SimDataStore *store)
{
    if (store == NULL)
    {
        return;
    }
    memset(store, 0, sizeof(*store));
}

int sim_data_loader_load_pfd(SimDataStore *store, const char *path)
{
    if (store == NULL || path == NULL)
    {
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        return 0;
    }

    char line[512];
    store->pfd_count = 0;
    while (fgets(line, sizeof(line), file) != NULL && store->pfd_count < SIM_MAX_PFD_SAMPLES)
    {
        strip_comment(line);
        char *content = trim_whitespace(line);
        if (content == NULL || *content == '\0')
        {
            continue;
        }

        float values[SIM_PFD_FIELD_COUNT];
        if (!parse_float_row(content, values, SIM_PFD_FIELD_COUNT))
        {
            continue;
        }

        SimPfdSample *sample = &store->pfd_samples[store->pfd_count++];
        sample->airspeed = values[0];
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

    fclose(file);
    return store->pfd_count > 0;
}

int sim_data_loader_load_nd(SimDataStore *store, const char *path)
{
    if (store == NULL || path == NULL)
    {
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        return 0;
    }

    char line[512];
    unsigned int header_fields[32];
    int header_count = 0;
    int has_header = 0;

    memset(header_fields, 0, sizeof(header_fields));
    store->nd_count = 0;
    store->nd_has_time = 0;

    while (fgets(line, sizeof(line), file) != NULL && store->nd_count < SIM_MAX_ND_FRAMES)
    {
        strip_comment(line);
        char *content = trim_whitespace(line);
        if (content == NULL || *content == '\0')
        {
            continue;
        }

        char token_line[512];
        snprintf(token_line, sizeof(token_line), "%s", content);
        char *tokens[32];
        const int token_count = split_tokens(token_line, tokens, 32);
        if (token_count <= 0)
        {
            continue;
        }

        if (!has_header && store->nd_count == 0 && contains_alpha(content))
        {
            has_header = 1;
            header_count = token_count;
            for (int i = 0; i < header_count; ++i)
            {
                header_fields[i] = nd_field_from_header(tokens[i]);
                if (header_fields[i] == SIM_ND_FIELD_TIME)
                {
                    store->nd_has_time = 1;
                }
            }
            continue;
        }

        SimNdFrame frame;
        memset(&frame, 0, sizeof(frame));

        if (has_header)
        {
            for (int i = 0; i < token_count && i < header_count; ++i)
            {
                char *end = NULL;
                const double value = strtod(tokens[i], &end);
                if (end != tokens[i])
                {
                    set_nd_frame_value(&frame, header_fields[i], value);
                }
            }
        }
        else
        {
            double values[32];
            int value_count = 0;
            for (int i = 0; i < token_count && value_count < 32; ++i)
            {
                char *end = NULL;
                const double value = strtod(tokens[i], &end);
                if (end != tokens[i])
                {
                    values[value_count++] = value;
                }
            }

            if (value_count == 4)
            {
                set_nd_frame_value(&frame, SIM_ND_FIELD_GROUND_SPEED, values[0]);
                set_nd_frame_value(&frame, SIM_ND_FIELD_TRUE_AIR_SPEED, values[1]);
                set_nd_frame_value(&frame, SIM_ND_FIELD_HEADING, values[2]);
                set_nd_frame_value(&frame, SIM_ND_FIELD_TRACK, values[3]);
            }
            else if (value_count >= 6)
            {
                set_nd_frame_value(&frame, SIM_ND_FIELD_LATITUDE, values[0]);
                set_nd_frame_value(&frame, SIM_ND_FIELD_LONGITUDE, values[1]);
                set_nd_frame_value(&frame, SIM_ND_FIELD_HEADING, values[2]);
                set_nd_frame_value(&frame, SIM_ND_FIELD_TRACK, values[3]);
                set_nd_frame_value(&frame, SIM_ND_FIELD_GROUND_SPEED, values[4]);
                set_nd_frame_value(&frame, SIM_ND_FIELD_TRUE_AIR_SPEED, values[5]);
            }
        }

        if (frame.fields != 0)
        {
            if ((frame.fields & SIM_ND_FIELD_TIME) != 0)
            {
                store->nd_has_time = 1;
            }
            store->nd_frames[store->nd_count++] = frame;
        }
    }

    fclose(file);
    return store->nd_count > 0;
}

int sim_data_loader_load_eicas_upper(SimDataStore *store, const char *path)
{
    if (store == NULL || path == NULL)
    {
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        return 0;
    }

    char line[512];
    store->eicas_upper_count = 0;
    while (fgets(line, sizeof(line), file) != NULL && store->eicas_upper_count < SIM_MAX_EICAS_FRAMES)
    {
        strip_comment(line);
        char *content = trim_whitespace(line);
        if (content == NULL || *content == '\0')
        {
            continue;
        }

        float values[SIM_EICAS_UPPER_FIELD_COUNT];
        if (!parse_float_row(content, values, SIM_EICAS_UPPER_FIELD_COUNT))
        {
            continue;
        }

        SimEicasUpperFrame *frame = &store->eicas_upper_frames[store->eicas_upper_count++];
        frame->total_air_temperature = values[0];
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

    fclose(file);
    return store->eicas_upper_count > 0;
}

int sim_data_loader_load_eicas_lower(SimDataStore *store, const char *path)
{
    if (store == NULL || path == NULL)
    {
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        return 0;
    }

    char line[512];
    store->eicas_lower_count = 0;
    while (fgets(line, sizeof(line), file) != NULL && store->eicas_lower_count < SIM_MAX_EICAS_FRAMES)
    {
        strip_comment(line);
        char *content = trim_whitespace(line);
        if (content == NULL || *content == '\0')
        {
            continue;
        }

        float values[SIM_EICAS_LOWER_FIELD_COUNT];
        if (!parse_float_row(content, values, SIM_EICAS_LOWER_FIELD_COUNT))
        {
            continue;
        }

        SimEicasLowerFrame *frame = &store->eicas_lower_frames[store->eicas_lower_count++];
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

    fclose(file);
    return store->eicas_lower_count > 0;
}

static const char *sim_fms_point_type_name(int type)
{
    switch (type)
    {
    case 1:
        return "AIRPORT";
    case 2:
        return "NDB";
    case 3:
        return "NAVAID";
    case 11:
        return "FIX";
    default:
        return "POINT";
    }
}

static int sim_valid_route_position(double latitude, double longitude)
{
    return latitude >= -90.0 && latitude <= 90.0 &&
           longitude >= -180.0 && longitude <= 180.0 &&
           !(latitude == 0.0 && longitude == 0.0);
}

int sim_data_loader_load_fms_route(SimPlannedRoute *route, const char *path)
{
    if (route == NULL || path == NULL)
    {
        return 0;
    }

    memset(route, 0, sizeof(*route));
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        return 0;
    }

    char line[512];
    int coordinate_count = 0;
    while (fgets(line, sizeof(line), file) != NULL && route->point_count < SIM_ROUTE_MAX_POINTS)
    {
        int type = 0;
        char ident[SIM_ROUTE_TEXT_LEN];
        double altitude = 0.0;
        double latitude = 0.0;
        double longitude = 0.0;

        strip_comment(line);
        char *content = trim_whitespace(line);
        if (content == NULL || *content == '\0')
        {
            continue;
        }

        ident[0] = '\0';
        if (sscanf(content, "%d %63s %lf %lf %lf", &type, ident, &altitude, &latitude, &longitude) != 5)
        {
            continue;
        }

        SimRoutePoint *point = &route->points[route->point_count++];
        snprintf(point->ident, sizeof(point->ident), "%s", ident);
        snprintf(point->type, sizeof(point->type), "%s", sim_fms_point_type_name(type));
        snprintf(point->coordinate_source, sizeof(point->coordinate_source), "%s", "ROUTE");
        point->altitude = altitude;
        point->latitude = latitude;
        point->longitude = longitude;
        point->has_position = sim_valid_route_position(latitude, longitude);
        if (point->has_position)
        {
            coordinate_count++;
        }
    }

    fclose(file);

    if (route->point_count <= 0)
    {
        memset(route, 0, sizeof(*route));
        return 0;
    }

    snprintf(route->origin, sizeof(route->origin), "%s", route->points[0].ident);
    snprintf(route->destination, sizeof(route->destination), "%s", route->points[route->point_count - 1].ident);
    snprintf(route->source_path, sizeof(route->source_path), "%s", path);
    route->valid = 1;
    route->source = SIM_ROUTE_SOURCE_FMC_FMS_FILE;
    route->loaded_from_file = 1;
    route->has_coordinates = coordinate_count == route->point_count;
    route->active_waypoint_index = route->point_count > 1 ? 1 : 0;
    return 1;
}

int sim_data_loader_load_all(SimDataStore *store)
{
    if (store == NULL)
    {
        return 0;
    }

    sim_data_store_init(store);
    const int pfd_ok = sim_data_loader_load_pfd(store, "assets/pfd.dat");
    const int nd_ok = sim_data_loader_load_nd(store, "assets/nd.dat");
    const int eicas1_ok = sim_data_loader_load_eicas_upper(store, "assets/eicas1.dat");
    const int eicas2_ok = sim_data_loader_load_eicas_lower(store, "assets/eicas2.dat");

    printf("SimDataCenter loader: pfd=%d nd=%d eicas1=%d eicas2=%d.\n",
           pfd_ok ? store->pfd_count : 0,
           nd_ok ? store->nd_count : 0,
           eicas1_ok ? store->eicas_upper_count : 0,
           eicas2_ok ? store->eicas_lower_count : 0);
    fflush(stdout);

    return pfd_ok || nd_ok || eicas1_ok || eicas2_ok;
}
