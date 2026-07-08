#include "nd_data.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ND_DEG_TO_RAD 0.01745329251994329577f
#define ND_RAD_TO_DEG 57.295779513082320876f
#define ND_DATA_DEFAULT_STEP_SEC (1.0f / 30.0f)
#define ND_DATA_PATH "assets/nd.dat"
#define ND_EARTH_FIX_PATH "assets/earth_fix.dat"
#define ND_EARTH_NAV_PATH "assets/earth_nav.dat"
#define ND_APT_PATH "assets/apt.dat"
#define ND_EARTH_FIX_READ_LIMIT 5000
#define ND_EARTH_FIX_LOAD_RADIUS_NM 180.0f
#define ND_EARTH_NAV_LOAD_RADIUS_NM 180.0f
#define ND_APT_LOAD_RADIUS_NM 180.0f

#define ND_FRAME_TIME (1u << 0)
#define ND_FRAME_LATITUDE (1u << 1)
#define ND_FRAME_LONGITUDE (1u << 2)
#define ND_FRAME_HEADING (1u << 3)
#define ND_FRAME_TRACK (1u << 4)
#define ND_FRAME_GROUND_SPEED (1u << 5)
#define ND_FRAME_TRUE_AIR_SPEED (1u << 6)
#define ND_FRAME_RANGE (1u << 7)
#define ND_FRAME_ACTIVE_DISTANCE (1u << 8)
#define ND_FRAME_ACTIVE_ETA (1u << 9)

static float normalize_degrees(float degrees)
{
    while (degrees >= 360.0f)
    {
        degrees -= 360.0f;
    }

    while (degrees < 0.0f)
    {
        degrees += 360.0f;
    }

    return degrees;
}

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

static unsigned int frame_field_from_header(const char *header)
{
    char name[64];
    snprintf(name, sizeof(name), "%s", header != NULL ? header : "");
    normalize_token(name);

    if (strcmp(name, "time") == 0 || strcmp(name, "timesec") == 0 || strcmp(name, "seconds") == 0 || strcmp(name, "sec") == 0)
    {
        return ND_FRAME_TIME;
    }
    if (strcmp(name, "latitude") == 0 || strcmp(name, "lat") == 0)
    {
        return ND_FRAME_LATITUDE;
    }
    if (strcmp(name, "longitude") == 0 || strcmp(name, "lon") == 0 || strcmp(name, "lng") == 0)
    {
        return ND_FRAME_LONGITUDE;
    }
    if (strcmp(name, "heading") == 0 || strcmp(name, "hdg") == 0)
    {
        return ND_FRAME_HEADING;
    }
    if (strcmp(name, "track") == 0 || strcmp(name, "trk") == 0)
    {
        return ND_FRAME_TRACK;
    }
    if (strcmp(name, "groundspeed") == 0 || strcmp(name, "gs") == 0)
    {
        return ND_FRAME_GROUND_SPEED;
    }
    if (strcmp(name, "trueairspeed") == 0 || strcmp(name, "tas") == 0)
    {
        return ND_FRAME_TRUE_AIR_SPEED;
    }
    if (strcmp(name, "range") == 0 || strcmp(name, "rangenm") == 0)
    {
        return ND_FRAME_RANGE;
    }
    if (strcmp(name, "activewaypointdistance") == 0 || strcmp(name, "activewaypointdistancenm") == 0 ||
        strcmp(name, "activedistance") == 0 || strcmp(name, "activedistancenm") == 0)
    {
        return ND_FRAME_ACTIVE_DISTANCE;
    }
    if (strcmp(name, "activewaypointeta") == 0 || strcmp(name, "activewaypointetamin") == 0 ||
        strcmp(name, "activeeta") == 0 || strcmp(name, "activeetamin") == 0)
    {
        return ND_FRAME_ACTIVE_ETA;
    }

    return 0;
}

static void set_frame_value(ND_DataFrame *frame, unsigned int field, double value)
{
    if (frame == NULL || field == 0)
    {
        return;
    }

    frame->fields |= field;
    switch (field)
    {
    case ND_FRAME_TIME:
        frame->time_sec = (float)value;
        break;
    case ND_FRAME_LATITUDE:
        frame->latitude = value;
        break;
    case ND_FRAME_LONGITUDE:
        frame->longitude = value;
        break;
    case ND_FRAME_HEADING:
        frame->heading = normalize_degrees((float)value);
        break;
    case ND_FRAME_TRACK:
        frame->track = normalize_degrees((float)value);
        break;
    case ND_FRAME_GROUND_SPEED:
        frame->ground_speed = (float)value;
        break;
    case ND_FRAME_TRUE_AIR_SPEED:
        frame->true_air_speed = (float)value;
        break;
    case ND_FRAME_RANGE:
        frame->range_nm = (float)value;
        break;
    case ND_FRAME_ACTIVE_DISTANCE:
        frame->active_waypoint_distance_nm = (float)value;
        break;
    case ND_FRAME_ACTIVE_ETA:
        frame->active_waypoint_eta_min = (float)value;
        break;
    default:
        break;
    }
}

static void infer_unlabeled_frame(ND_DataFrame *frame, double *values, int count)
{
    if (frame == NULL || values == NULL)
    {
        return;
    }

    if (count == 4)
    {
        set_frame_value(frame, ND_FRAME_GROUND_SPEED, values[0]);
        set_frame_value(frame, ND_FRAME_TRUE_AIR_SPEED, values[1]);
        set_frame_value(frame, ND_FRAME_HEADING, values[2]);
        set_frame_value(frame, ND_FRAME_TRACK, values[3]);
    }
    else if (count >= 6)
    {
        set_frame_value(frame, ND_FRAME_LATITUDE, values[0]);
        set_frame_value(frame, ND_FRAME_LONGITUDE, values[1]);
        set_frame_value(frame, ND_FRAME_HEADING, values[2]);
        set_frame_value(frame, ND_FRAME_TRACK, values[3]);
        set_frame_value(frame, ND_FRAME_GROUND_SPEED, values[4]);
        set_frame_value(frame, ND_FRAME_TRUE_AIR_SPEED, values[5]);
        if (count >= 7)
        {
            set_frame_value(frame, ND_FRAME_RANGE, values[6]);
        }
    }
}

static void integrate_aircraft_position(ND_Data *data, float delta_time)
{
    if (data == NULL || delta_time <= 0.0f)
    {
        return;
    }

    const float distance_nm = data->ground_speed * delta_time / 3600.0f;
    const float track_rad = data->track * ND_DEG_TO_RAD;
    const float latitude_rad = (float)data->latitude * ND_DEG_TO_RAD;
    const float longitude_scale = cosf(latitude_rad);

    data->latitude += (double)(cosf(track_rad) * distance_nm / 60.0f);
    if (fabsf(longitude_scale) > 0.001f)
    {
        data->longitude += (double)(sinf(track_rad) * distance_nm / (60.0f * longitude_scale));
    }
}

static void set_nav_point(
    ND_NavPoint *point,
    const char *ident,
    ND_PointType type,
    double latitude,
    double longitude,
    int active)
{
    if (point == NULL)
    {
        return;
    }

    snprintf(point->ident, sizeof(point->ident), "%s", ident);
    point->type = type;
    point->latitude = latitude;
    point->longitude = longitude;
    point->distance_nm = 0.0f;
    point->bearing_deg = 0.0f;
    point->visible = 1;
    point->active = active;
}

static int add_nav_point(
    ND_Data *data,
    const char *ident,
    ND_PointType type,
    double latitude,
    double longitude,
    int active)
{
    if (data == NULL || data->nav_point_count >= ND_MAX_NAV_POINTS)
    {
        return 0;
    }

    set_nav_point(&data->nav_points[data->nav_point_count], ident, type, latitude, longitude, active);
    if (active)
    {
        data->active_point_index = data->nav_point_count;
    }
    ++data->nav_point_count;
    return 1;
}

static int load_earth_fix_file(ND_Data *data, const char *path)
{
    if (data == NULL || path == NULL)
    {
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        printf("ND FIX: failed to open %s, using mock navigation points only.\n", path);
        data->earth_fix_loaded = 0;
        data->earth_fix_count = 0;
        return 0;
    }

    char line[512];
    int cached_count = 0;
    int parsed_count = 0;
    int line_number = 0;
    const float reference_lat_rad = (float)data->latitude * ND_DEG_TO_RAD;
    const float reference_lon_scale = cosf(reference_lat_rad);

    while (fgets(line, sizeof(line), file) != NULL)
    {
        ++line_number;

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

        char *content = trim_whitespace(line);
        if (content == NULL || content[0] == '\0')
        {
            continue;
        }

        char token_line[512];
        snprintf(token_line, sizeof(token_line), "%s", content);
        char *tokens[8];
        const int token_count = split_tokens(token_line, tokens, 8);
        if (token_count < 3)
        {
            continue;
        }

        char *lat_end = NULL;
        char *lon_end = NULL;
        const double latitude = strtod(tokens[0], &lat_end);
        const double longitude = strtod(tokens[1], &lon_end);
        if (lat_end == tokens[0] || lon_end == tokens[1])
        {
            continue;
        }
        if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0)
        {
            continue;
        }
        ++parsed_count;

        const float north_nm = (float)((latitude - data->latitude) * 60.0);
        const float east_nm = (float)((longitude - data->longitude) * 60.0 * (double)reference_lon_scale);
        const float distance_nm = sqrtf(north_nm * north_nm + east_nm * east_nm);
        if (distance_nm > ND_EARTH_FIX_LOAD_RADIUS_NM)
        {
            continue;
        }

        char ident[ND_NAME_LEN];
        snprintf(ident, sizeof(ident), "%s", tokens[2]);
        if (ident[0] == '\0')
        {
            continue;
        }

        if (!add_nav_point(data, ident, ND_POINT_WAYPOINT, latitude, longitude, 0))
        {
            break;
        }

        ++cached_count;
        if (cached_count >= ND_EARTH_FIX_READ_LIMIT)
        {
            break;
        }
    }

    fclose(file);

    data->earth_fix_count = cached_count;
    data->earth_fix_loaded = cached_count > 0;

    if (cached_count > 0)
    {
        printf("ND FIX: loaded %d nearby FIX rows from %s (%d valid rows parsed, %.0fNM preload radius).\n",
               cached_count,
               path,
               parsed_count,
               ND_EARTH_FIX_LOAD_RADIUS_NM);
    }
    else
    {
        printf("ND FIX: no nearby FIX rows in %s (%d valid rows parsed), using mock navigation points only.\n",
               path,
               parsed_count);
    }

    (void)line_number;
    return data->earth_fix_loaded;
}

static int classify_earth_nav_type(int raw_type, ND_PointType *type)
{
    if (type == NULL)
    {
        return 0;
    }

    switch (raw_type)
    {
    case 2:
        *type = ND_POINT_NDB;
        return 1;
    case 3:
        *type = ND_POINT_VOR;
        return 1;
    case 4:
    case 5:
        *type = ND_POINT_ILS;
        return 1;
    default:
        break;
    }

    return 0;
}

static void count_earth_nav_type(ND_Data *data, ND_PointType type)
{
    if (data == NULL)
    {
        return;
    }

    switch (type)
    {
    case ND_POINT_VOR:
        ++data->earth_nav_vor_count;
        break;
    case ND_POINT_NDB:
        ++data->earth_nav_ndb_count;
        break;
    case ND_POINT_ILS:
        ++data->earth_nav_ils_count;
        break;
    default:
        break;
    }
}

static int load_earth_nav_file(ND_Data *data, const char *path)
{
    if (data == NULL || path == NULL)
    {
        return 0;
    }

    data->earth_nav_loaded = 0;
    data->earth_nav_count = 0;
    data->earth_nav_vor_count = 0;
    data->earth_nav_ndb_count = 0;
    data->earth_nav_ils_count = 0;

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        printf("ND NAV: failed to open %s, skipping real navigation stations.\n", path);
        return 0;
    }

    char line[768];
    int parsed_count = 0;
    int cached_count = 0;
    const float reference_lat_rad = (float)data->latitude * ND_DEG_TO_RAD;
    const float reference_lon_scale = cosf(reference_lat_rad);

    while (fgets(line, sizeof(line), file) != NULL)
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

        char *content = trim_whitespace(line);
        if (content == NULL || content[0] == '\0')
        {
            continue;
        }

        char token_line[768];
        snprintf(token_line, sizeof(token_line), "%s", content);
        char *tokens[24];
        const int token_count = split_tokens(token_line, tokens, 24);
        if (token_count < 8)
        {
            continue;
        }

        char *type_end = NULL;
        const long raw_type_long = strtol(tokens[0], &type_end, 10);
        if (type_end == tokens[0] || *type_end != '\0')
        {
            continue;
        }
        if (raw_type_long == 99)
        {
            break;
        }

        ND_PointType point_type;
        if (!classify_earth_nav_type((int)raw_type_long, &point_type))
        {
            continue;
        }

        char *lat_end = NULL;
        char *lon_end = NULL;
        const double latitude = strtod(tokens[1], &lat_end);
        const double longitude = strtod(tokens[2], &lon_end);
        if (lat_end == tokens[1] || lon_end == tokens[2])
        {
            continue;
        }
        if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0)
        {
            continue;
        }

        ++parsed_count;

        const float north_nm = (float)((latitude - data->latitude) * 60.0);
        const float east_nm = (float)((longitude - data->longitude) * 60.0 * (double)reference_lon_scale);
        const float distance_nm = sqrtf(north_nm * north_nm + east_nm * east_nm);
        if (distance_nm > ND_EARTH_NAV_LOAD_RADIUS_NM)
        {
            continue;
        }

        char ident[ND_NAME_LEN];
        snprintf(ident, sizeof(ident), "%s", tokens[7]);
        if (ident[0] == '\0')
        {
            continue;
        }

        if (!add_nav_point(data, ident, point_type, latitude, longitude, 0))
        {
            break;
        }

        ++cached_count;
        ++data->earth_nav_count;
        count_earth_nav_type(data, point_type);
    }

    fclose(file);

    data->earth_nav_loaded = cached_count > 0;

    if (cached_count > 0)
    {
        printf("ND NAV: loaded %d nearby NAV rows from %s (%d VOR, %d NDB, %d ILS/LOC; %d usable rows parsed, %.0fNM preload radius).\n",
               cached_count,
               path,
               data->earth_nav_vor_count,
               data->earth_nav_ndb_count,
               data->earth_nav_ils_count,
               parsed_count,
               ND_EARTH_NAV_LOAD_RADIUS_NM);
    }
    else
    {
        printf("ND NAV: no nearby VOR/NDB/ILS rows in %s (%d usable rows parsed), skipping real navigation stations.\n",
               path,
               parsed_count);
    }

    return data->earth_nav_loaded;
}

static int load_apt_file(ND_Data *data, const char *path)
{
    if (data == NULL || path == NULL)
    {
        return 0;
    }

    data->apt_loaded = 0;
    data->apt_airport_count = 0;
    data->apt_tower_count = 0;

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        printf("ND APT: failed to open %s, skipping real airport/tower data.\n", path);
        return 0;
    }

    char line[512];
    int parsed_count = 0;
    int cached_airport_count = 0;
    const float reference_lat_rad = (float)data->latitude * ND_DEG_TO_RAD;
    const float reference_lon_scale = cosf(reference_lat_rad);

    while (fgets(line, sizeof(line), file) != NULL)
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

        char *content = trim_whitespace(line);
        if (content == NULL || content[0] == '\0')
        {
            continue;
        }

        char token_line[512];
        snprintf(token_line, sizeof(token_line), "%s", content);
        char *tokens[8];
        const int token_count = split_tokens(token_line, tokens, 8);
        if (token_count < 3)
        {
            continue;
        }

        char *lat_end = NULL;
        char *lon_end = NULL;
        const double latitude = strtod(tokens[1], &lat_end);
        const double longitude = strtod(tokens[2], &lon_end);
        if (lat_end == tokens[1] || lon_end == tokens[2])
        {
            continue;
        }
        if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0)
        {
            continue;
        }

        char ident[ND_NAME_LEN];
        snprintf(ident, sizeof(ident), "%s", tokens[0]);
        if (ident[0] == '\0')
        {
            continue;
        }

        ++parsed_count;

        const float north_nm = (float)((latitude - data->latitude) * 60.0);
        const float east_nm = (float)((longitude - data->longitude) * 60.0 * (double)reference_lon_scale);
        const float distance_nm = sqrtf(north_nm * north_nm + east_nm * east_nm);
        if (distance_nm > ND_APT_LOAD_RADIUS_NM)
        {
            continue;
        }

        if (!add_nav_point(data, ident, ND_POINT_AIRPORT, latitude, longitude, 0))
        {
            break;
        }

        ++cached_airport_count;
        ++data->apt_airport_count;
    }

    fclose(file);

    data->apt_loaded = cached_airport_count > 0;

    if (cached_airport_count > 0)
    {
        printf("ND APT: loaded %d nearby airport rows from %s (%d airport rows parsed, %d tower rows parsed, %.0fNM preload radius).\n",
               cached_airport_count,
               path,
               parsed_count,
               data->apt_tower_count,
               ND_APT_LOAD_RADIUS_NM);
    }
    else
    {
        printf("ND APT: no nearby airport rows in %s (%d airport rows parsed, no tower coordinates in this file), skipping real airport/tower data.\n",
               path,
               parsed_count);
    }

    return data->apt_loaded;
}

static int load_nd_data_file(ND_Data *data, const char *path)
{
    if (data == NULL || path == NULL)
    {
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        printf("ND data: failed to open %s, using internal mock data.\n", path);
        return 0;
    }

    char line[512];
    unsigned int header_fields[32];
    int header_count = 0;
    int has_header = 0;
    int line_number = 0;

    memset(header_fields, 0, sizeof(header_fields));
    data->data_frame_count = 0;
    data->data_file_has_time = 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        ++line_number;

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

        char *content = trim_whitespace(line);
        if (content == NULL || content[0] == '\0')
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

        if (!has_header && data->data_frame_count == 0 && contains_alpha(content))
        {
            has_header = 1;
            header_count = token_count;
            for (int i = 0; i < header_count; ++i)
            {
                header_fields[i] = frame_field_from_header(tokens[i]);
                if (header_fields[i] == ND_FRAME_TIME)
                {
                    data->data_file_has_time = 1;
                }
            }
            continue;
        }

        if (data->data_frame_count >= ND_MAX_DATA_FRAMES)
        {
            break;
        }

        ND_DataFrame frame;
        memset(&frame, 0, sizeof(frame));

        if (has_header)
        {
            for (int i = 0; i < token_count && i < header_count; ++i)
            {
                char *end = NULL;
                const double value = strtod(tokens[i], &end);
                if (end != tokens[i])
                {
                    set_frame_value(&frame, header_fields[i], value);
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
            infer_unlabeled_frame(&frame, values, value_count);
        }

        if (frame.fields == 0)
        {
            printf("ND data: ignored unrecognized row %d in %s.\n", line_number, path);
            continue;
        }

        if (frame.fields & ND_FRAME_TIME)
        {
            data->data_file_has_time = 1;
        }

        data->data_frames[data->data_frame_count++] = frame;
    }

    fclose(file);

    if (data->data_frame_count <= 0)
    {
        printf("ND data: no usable rows in %s, using internal mock data.\n", path);
        data->data_file_loaded = 0;
        data->data_file_has_time = 0;
        return 0;
    }

    data->data_file_loaded = 1;
    data->data_frame_index = 0;
    data->data_frame_elapsed = 0.0f;
    data->data_frame_step_sec = ND_DATA_DEFAULT_STEP_SEC;

    if (!has_header)
    {
        printf("ND data: loaded %d rows from %s (CSV without header: GS,TAS,heading,track).\n",
               data->data_frame_count,
               path);
    }
    else
    {
        printf("ND data: loaded %d rows from %s (header detected).\n", data->data_frame_count, path);
    }

    return 1;
}

static void update_active_waypoint_info(ND_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->active_waypoint_name[0] = '\0';
    data->active_waypoint_distance_nm = 0.0f;
    data->active_waypoint_bearing_deg = 0.0f;
    data->active_waypoint_eta_min = 0.0f;

    if (data->active_point_index < 0 || data->active_point_index >= data->nav_point_count)
    {
        snprintf(data->active_waypoint_name, sizeof(data->active_waypoint_name), "----");
        return;
    }

    const ND_NavPoint *active = &data->nav_points[data->active_point_index];
    snprintf(data->active_waypoint_name, sizeof(data->active_waypoint_name), "%s", active->ident);
    data->active_waypoint_distance_nm = active->distance_nm;
    data->active_waypoint_bearing_deg = active->bearing_deg;

    if (data->ground_speed > 1.0f)
    {
        data->active_waypoint_eta_min = active->distance_nm / data->ground_speed * 60.0f;
    }
}

void nd_data_recalculate_nav_points(ND_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    const float aircraft_lat_rad = (float)data->latitude * ND_DEG_TO_RAD;
    const float lon_scale = cosf(aircraft_lat_rad);
    int visible_fix_indices[ND_MAX_VISIBLE_FIX_POINTS];
    float visible_fix_distances[ND_MAX_VISIBLE_FIX_POINTS];
    int visible_fix_count = 0;
    int visible_nav_indices[ND_MAX_VISIBLE_NAV_POINTS];
    float visible_nav_distances[ND_MAX_VISIBLE_NAV_POINTS];
    int visible_nav_count = 0;
    int visible_airport_indices[ND_MAX_VISIBLE_AIRPORT_POINTS];
    float visible_airport_distances[ND_MAX_VISIBLE_AIRPORT_POINTS];
    int visible_airport_count = 0;
    const int real_nav_data_loaded = data->earth_fix_loaded || data->earth_nav_loaded || data->apt_loaded;

    for (int i = 0; i < data->nav_point_count; ++i)
    {
        ND_NavPoint *point = &data->nav_points[i];
        const double delta_lat = point->latitude - data->latitude;
        const double delta_lon = point->longitude - data->longitude;
        const float north_nm = (float)(delta_lat * 60.0);
        const float east_nm = (float)(delta_lon * 60.0 * (double)lon_scale);
        float bearing = atan2f(east_nm, north_nm) * ND_RAD_TO_DEG;

        point->distance_nm = sqrtf(north_nm * north_nm + east_nm * east_nm);
        point->bearing_deg = normalize_degrees(bearing);
        point->active = i == data->active_point_index;

        if (i < data->mock_nav_point_count)
        {
            point->visible = !real_nav_data_loaded && (point->active || point->distance_nm <= data->range_nm);
        }
        else
        {
            point->visible = 0;
            if (point->distance_nm <= data->range_nm)
            {
                const int is_real_nav_station = point->type == ND_POINT_VOR || point->type == ND_POINT_NDB || point->type == ND_POINT_ILS;
                const int is_real_airport_point = point->type == ND_POINT_AIRPORT || point->type == ND_POINT_TOWER;
                int *visible_indices = visible_fix_indices;
                float *visible_distances = visible_fix_distances;
                int *visible_count = &visible_fix_count;
                int visible_limit = ND_MAX_VISIBLE_FIX_POINTS;

                if (is_real_nav_station)
                {
                    visible_indices = visible_nav_indices;
                    visible_distances = visible_nav_distances;
                    visible_count = &visible_nav_count;
                    visible_limit = ND_MAX_VISIBLE_NAV_POINTS;
                }
                else if (is_real_airport_point)
                {
                    visible_indices = visible_airport_indices;
                    visible_distances = visible_airport_distances;
                    visible_count = &visible_airport_count;
                    visible_limit = ND_MAX_VISIBLE_AIRPORT_POINTS;
                }

                int insert_at = *visible_count;
                while (insert_at > 0 && visible_distances[insert_at - 1] > point->distance_nm)
                {
                    if (insert_at < visible_limit)
                    {
                        visible_indices[insert_at] = visible_indices[insert_at - 1];
                        visible_distances[insert_at] = visible_distances[insert_at - 1];
                    }
                    --insert_at;
                }

                if (insert_at < visible_limit)
                {
                    visible_indices[insert_at] = i;
                    visible_distances[insert_at] = point->distance_nm;
                    if (*visible_count < visible_limit)
                    {
                        ++(*visible_count);
                    }
                }
            }
        }
    }

    for (int i = 0; i < visible_fix_count; ++i)
    {
        data->nav_points[visible_fix_indices[i]].visible = 1;
    }
    for (int i = 0; i < visible_nav_count; ++i)
    {
        data->nav_points[visible_nav_indices[i]].visible = 1;
    }
    for (int i = 0; i < visible_airport_count; ++i)
    {
        data->nav_points[visible_airport_indices[i]].visible = 1;
    }

    update_active_waypoint_info(data);
}

void nd_data_set_range(ND_Data *data, float range_nm)
{
    if (data == NULL)
    {
        return;
    }

    if (range_nm < 5.0f)
    {
        range_nm = 5.0f;
    }

    data->range_nm = range_nm;
}

static void apply_data_frame(ND_Data *data, const ND_DataFrame *frame, float delta_time)
{
    if (data == NULL || frame == NULL)
    {
        return;
    }

    const int has_latitude = (frame->fields & ND_FRAME_LATITUDE) != 0;
    const int has_longitude = (frame->fields & ND_FRAME_LONGITUDE) != 0;

    if (has_latitude)
    {
        data->latitude = frame->latitude;
    }
    if (has_longitude)
    {
        data->longitude = frame->longitude;
    }
    if (frame->fields & ND_FRAME_HEADING)
    {
        data->heading = frame->heading;
    }
    if (frame->fields & ND_FRAME_TRACK)
    {
        data->track = frame->track;
    }
    else if (frame->fields & ND_FRAME_HEADING)
    {
        data->track = data->heading;
    }
    if (frame->fields & ND_FRAME_GROUND_SPEED)
    {
        data->ground_speed = frame->ground_speed;
    }
    if (frame->fields & ND_FRAME_TRUE_AIR_SPEED)
    {
        data->true_air_speed = frame->true_air_speed;
    }
    if (frame->fields & ND_FRAME_RANGE)
    {
        nd_data_set_range(data, frame->range_nm);
    }

    if (!has_latitude || !has_longitude)
    {
        integrate_aircraft_position(data, delta_time);
    }

    nd_data_recalculate_nav_points(data);

    if (frame->fields & ND_FRAME_ACTIVE_DISTANCE)
    {
        data->active_waypoint_distance_nm = frame->active_waypoint_distance_nm;
    }
    if (frame->fields & ND_FRAME_ACTIVE_ETA)
    {
        data->active_waypoint_eta_min = frame->active_waypoint_eta_min;
    }
}

static void update_from_data_file(ND_Data *data, float delta_time)
{
    if (data == NULL || !data->data_file_loaded || data->data_frame_count <= 0)
    {
        return;
    }

    if (data->data_file_has_time)
    {
        data->data_frame_elapsed += delta_time;

        if (data->data_frame_index >= data->data_frame_count)
        {
            data->data_frame_index = 0;
            data->data_frame_elapsed = 0.0f;
        }

        while (data->data_frame_index + 1 < data->data_frame_count &&
               data->data_frames[data->data_frame_index + 1].time_sec <= data->data_frame_elapsed)
        {
            ++data->data_frame_index;
        }

        if (data->data_frame_index + 1 >= data->data_frame_count &&
            data->data_frame_elapsed > data->data_frames[data->data_frame_index].time_sec)
        {
            data->data_frame_index = 0;
            data->data_frame_elapsed = 0.0f;
        }
    }
    else
    {
        data->data_frame_elapsed += delta_time;
        while (data->data_frame_elapsed >= data->data_frame_step_sec)
        {
            data->data_frame_elapsed -= data->data_frame_step_sec;
            data->data_frame_index = (data->data_frame_index + 1) % data->data_frame_count;
        }
    }

    apply_data_frame(data, &data->data_frames[data->data_frame_index], delta_time);
}

void nd_data_init(ND_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    memset(data, 0, sizeof(*data));

    data->latitude = 39.904200;
    data->longitude = 116.407400;
    data->heading = 2.0f;
    data->track = 3.0f;
    data->ground_speed = 262.0f;
    data->true_air_speed = 262.0f;
    data->range_nm = 60.0f;
    data->nav_point_count = 8;
    data->active_point_index = 0;
    data->simulation_time = 0.0f;
    data->data_frame_step_sec = ND_DATA_DEFAULT_STEP_SEC;

    set_nav_point(&data->nav_points[0], "WPT01", ND_POINT_WAYPOINT, 40.664200, 116.427400, 1);
    set_nav_point(&data->nav_points[1], "WPT02", ND_POINT_WAYPOINT, 40.524200, 116.667400, 0);
    set_nav_point(&data->nav_points[2], "WPT03", ND_POINT_WAYPOINT, 40.374200, 116.887400, 0);
    set_nav_point(&data->nav_points[3], "WPT04", ND_POINT_WAYPOINT, 40.604200, 116.027400, 0);
    set_nav_point(&data->nav_points[4], "ZBAA", ND_POINT_AIRPORT, 40.080100, 116.584600, 0);
    set_nav_point(&data->nav_points[5], "TWR01", ND_POINT_TOWER, 40.075000, 116.592000, 0);
    set_nav_point(&data->nav_points[6], "VOR01", ND_POINT_VOR, 40.324200, 116.157400, 0);
    set_nav_point(&data->nav_points[7], "NDB01", ND_POINT_NDB, 40.024200, 116.087400, 0);
    data->mock_nav_point_count = data->nav_point_count;

    load_earth_fix_file(data, ND_EARTH_FIX_PATH);
    load_earth_nav_file(data, ND_EARTH_NAV_PATH);
    load_apt_file(data, ND_APT_PATH);
    nd_data_recalculate_nav_points(data);

    if (load_nd_data_file(data, ND_DATA_PATH))
    {
        apply_data_frame(data, &data->data_frames[0], 0.0f);
    }
}

static void update_internal_mock_data(ND_Data *data, float delta_time)
{
    if (data == NULL)
    {
        return;
    }

    if (delta_time < 0.0f)
    {
        delta_time = 0.0f;
    }

    const float t = data->simulation_time;

    data->heading = normalize_degrees(2.0f + 0.8f * sinf(t * 0.12f));
    data->track = normalize_degrees(data->heading + 1.0f + 0.4f * sinf(t * 0.20f));
    data->ground_speed = 262.0f + 2.0f * sinf(t * 0.35f) + 0.8f * cosf(t * 0.70f);
    data->true_air_speed = 262.0f + 1.5f * sinf(t * 0.28f + 0.8f);

    integrate_aircraft_position(data, delta_time);
    nd_data_recalculate_nav_points(data);
}

void nd_data_update_mock(ND_Data *data, float delta_time)
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

    if (data->data_file_loaded)
    {
        update_from_data_file(data, delta_time);
    }
    else
    {
        update_internal_mock_data(data, delta_time);
    }
}
