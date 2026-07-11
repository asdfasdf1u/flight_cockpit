#include "fmc_data.h"

#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fmc_xpc.h"

#define FMC_ROUTE_PLAN_PATH_LEN 192

typedef struct FMC_RoutePlanNode
{
    char key[FMC_TEXT_LEN * 2];
    char origin[FMC_TEXT_LEN];
    char destination[FMC_TEXT_LEN];
    char path[FMC_ROUTE_PLAN_PATH_LEN];
    int route_count;
    int cruise_altitude;
    int height;
    struct FMC_RoutePlanNode *left;
    struct FMC_RoutePlanNode *right;
} FMC_RoutePlanNode;

static FMC_RoutePlanNode *route_plan_root = NULL;
static int route_plan_count = 0;
static int route_plan_loaded = 0;

static void set_text(char *dest, int dest_size, const char *src)
{
    if (dest == NULL || dest_size <= 0 || src == NULL)
    {
        return;
    }

    snprintf(dest, (size_t)dest_size, "%s", src);
}

static int scratchpad_number(const FMC_Data *data, int *value)
{
    int parsed = 0;
    int has_digit = 0;
    if (data == NULL || value == NULL)
    {
        return 0;
    }

    for (int i = 0; data->scratchpad[i] != '\0'; ++i)
    {
        if (isdigit((unsigned char)data->scratchpad[i]))
        {
            parsed = parsed * 10 + (data->scratchpad[i] - '0');
            has_digit = 1;
        }
    }

    if (!has_digit)
    {
        return 0;
    }

    *value = parsed;
    return 1;
}

static int parse_first_number(const char *text, int *value)
{
    int parsed = 0;
    int has_digit = 0;
    if (text == NULL || value == NULL)
    {
        return 0;
    }

    for (int i = 0; text[i] != '\0'; ++i)
    {
        if (isdigit((unsigned char)text[i]))
        {
            parsed = parsed * 10 + (text[i] - '0');
            has_digit = 1;
        }
        else if (has_digit)
        {
            break;
        }
    }

    if (!has_digit)
    {
        return 0;
    }

    *value = parsed;
    return 1;
}

static int parse_two_numbers(const char *text, int *first, int *second)
{
    int values[2] = {0, 0};
    int count = 0;
    if (text == NULL || first == NULL || second == NULL)
    {
        return 0;
    }

    for (int i = 0; text[i] != '\0' && count < 2; ++i)
    {
        if (isdigit((unsigned char)text[i]))
        {
            int value = 0;
            while (isdigit((unsigned char)text[i]))
            {
                value = value * 10 + (text[i] - '0');
                ++i;
            }
            values[count++] = value;
        }
    }

    if (count < 2)
    {
        return 0;
    }

    *first = values[0];
    *second = values[1];
    return 1;
}

static int set_checked_int(FMC_Data *data, int *field, int value, int min_value, int max_value, const char *label)
{
    if (data == NULL || field == NULL || label == NULL)
    {
        return 0;
    }

    if (value < min_value || value > max_value)
    {
        snprintf(data->message, sizeof(data->message), "%s RANGE %d-%d", label, min_value, max_value);
        return 0;
    }

    *field = value;
    fmc_data_clear_scratchpad(data);
    snprintf(data->message, sizeof(data->message), "%s SET %d", label, value);
    return 1;
}

static int scratchpad_airport_code(FMC_Data *data, char *dest, int dest_size, const char *label)
{
    if (data == NULL || dest == NULL || dest_size <= 0 || label == NULL)
    {
        return 0;
    }

    if (data->scratchpad_len != 4)
    {
        snprintf(data->message, sizeof(data->message), "%s MUST BE 4 LETTERS", label);
        return 0;
    }

    for (int i = 0; i < data->scratchpad_len; ++i)
    {
        if (!isalpha((unsigned char)data->scratchpad[i]))
        {
            snprintf(data->message, sizeof(data->message), "%s FORMAT INVALID", label);
            return 0;
        }
    }

    set_text(dest, dest_size, data->scratchpad);
    fmc_data_clear_scratchpad(data);
    snprintf(data->message, sizeof(data->message), "%s SET", label);
    return 1;
}

static int scratchpad_text(FMC_Data *data, char *dest, int dest_size, const char *label)
{
    if (data == NULL || dest == NULL || dest_size <= 0 || label == NULL)
    {
        return 0;
    }

    if (data->scratchpad_len <= 0)
    {
        snprintf(data->message, sizeof(data->message), "ENTER %s", label);
        return 0;
    }

    set_text(dest, dest_size, data->scratchpad);
    fmc_data_clear_scratchpad(data);
    snprintf(data->message, sizeof(data->message), "%s SET", label);
    return 1;
}

static int load_airport_index(void)
{
    const char *paths[] = {
        "assets/apt.dat",
        "../assets/apt.dat",
        "../../assets/apt.dat",
        NULL};

    for (int i = 0; paths[i] != NULL; ++i)
    {
        int count = fmc_airport_index_load(paths[i]);
        if (count > 0)
        {
            return count;
        }
    }

    return fmc_airport_index_count();
}

typedef struct FMC_DepArrProcedure
{
    const char *airport;
    const char *runway;
    const char *procedure;
    const char *transition;
    int arrival;
} FMC_DepArrProcedure;

static const FMC_DepArrProcedure DEP_ARR_PROCEDURES[] = {
    {"ZBAA", "36R", "BOBAK1D", "BOBAK", 0},
    {"ZBAA", "36R", "LADIX1D", "LADIX", 0},
    {"ZBAA", "01", "RENOB1D", "RENOB", 0},
    {"ZSPD", "34L", "PUD9A", "PUD", 1},
    {"ZSPD", "34L", "AND1A", "AND", 1},
    {"ZSPD", "35R", "NXD2A", "NXD", 1},
    {"KSEA", "RW16C", "ATOM E2", "COV", 0},
    {"KSEA", "RW16L", "BANGR 9", "BANGR", 0},
    {"KSEA", "RW34C", "SUMMA 1", "SUMMA", 0},
    {"KBFI", "14R", "GLASR1", "GLASR", 1},
    {"KBFI", "32L", "OLM7", "OLM", 1},
};

#define DEP_ARR_PROCEDURE_COUNT ((int)(sizeof(DEP_ARR_PROCEDURES) / sizeof(DEP_ARR_PROCEDURES[0])))

static int dep_arr_option_matches(const FMC_DepArrProcedure *option, const char *airport, const char *runway, int arrival)
{
    return option != NULL &&
           strcmp(option->airport, airport) == 0 &&
           strcmp(option->runway, runway) == 0 &&
           option->arrival == arrival;
}

static int runway_supported(const char *airport, const char *runway, int arrival)
{
    for (int i = 0; i < DEP_ARR_PROCEDURE_COUNT; ++i)
    {
        if (dep_arr_option_matches(&DEP_ARR_PROCEDURES[i], airport, runway, arrival))
        {
            return 1;
        }
    }
    return 0;
}

static int procedure_supported(const char *airport, const char *runway, const char *procedure, int arrival)
{
    for (int i = 0; i < DEP_ARR_PROCEDURE_COUNT; ++i)
    {
        if (dep_arr_option_matches(&DEP_ARR_PROCEDURES[i], airport, runway, arrival) &&
            strcmp(DEP_ARR_PROCEDURES[i].procedure, procedure) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static int procedure_known_for_airport(const char *airport, const char *procedure, int arrival)
{
    for (int i = 0; i < DEP_ARR_PROCEDURE_COUNT; ++i)
    {
        if (strcmp(DEP_ARR_PROCEDURES[i].airport, airport) == 0 &&
            strcmp(DEP_ARR_PROCEDURES[i].procedure, procedure) == 0 &&
            DEP_ARR_PROCEDURES[i].arrival == arrival)
        {
            return 1;
        }
    }
    return 0;
}

static int transition_supported(const char *airport, const char *runway, const char *procedure, const char *transition, int arrival)
{
    for (int i = 0; i < DEP_ARR_PROCEDURE_COUNT; ++i)
    {
        if (dep_arr_option_matches(&DEP_ARR_PROCEDURES[i], airport, runway, arrival) &&
            strcmp(DEP_ARR_PROCEDURES[i].procedure, procedure) == 0 &&
            strcmp(DEP_ARR_PROCEDURES[i].transition, transition) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static int transition_known_for_procedure(const char *airport, const char *procedure, const char *transition, int arrival)
{
    for (int i = 0; i < DEP_ARR_PROCEDURE_COUNT; ++i)
    {
        if (strcmp(DEP_ARR_PROCEDURES[i].airport, airport) == 0 &&
            strcmp(DEP_ARR_PROCEDURES[i].procedure, procedure) == 0 &&
            strcmp(DEP_ARR_PROCEDURES[i].transition, transition) == 0 &&
            DEP_ARR_PROCEDURES[i].arrival == arrival)
        {
            return 1;
        }
    }
    return 0;
}

static int load_waypoint_index(void)
{
    const char *paths[] = {
        "assets/earth_fix.dat",
        "../assets/earth_fix.dat",
        "../../assets/earth_fix.dat",
        NULL};

    for (int i = 0; paths[i] != NULL; ++i)
    {
        int count = fmc_waypoint_index_load(paths[i]);
        if (count > 0)
        {
            return count;
        }
    }

    return fmc_waypoint_index_count();
}

static void clear_airport_selection(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->airport_matches.count = 0;
    data->selected_airport_index = -1;
    data->airport_query[0] = '\0';
}

static void clear_waypoint_selection(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->waypoint_matches.count = 0;
    data->selected_waypoint_index = -1;
    data->has_selected_waypoint = 0;
    data->waypoint_query[0] = '\0';
}

static void clear_route_point(FMC_RoutePoint *point)
{
    if (point == NULL)
    {
        return;
    }

    point->ident[0] = '\0';
    point->type[0] = '\0';
    point->latitude = 0.0;
    point->longitude = 0.0;
    point->altitude = 0.0;
    point->speed_restriction[0] = '\0';
    point->altitude_restriction[0] = '\0';
}

static void set_route_point_basic(FMC_RoutePoint *point, const char *ident, const char *type, double latitude, double longitude, double altitude)
{
    if (point == NULL || ident == NULL || type == NULL)
    {
        return;
    }

    clear_route_point(point);
    set_text(point->ident, sizeof(point->ident), ident);
    set_text(point->type, sizeof(point->type), type);
    point->latitude = latitude;
    point->longitude = longitude;
    point->altitude = altitude;
}

static FMC_RoutePoint route_point_from_waypoint(const FMC_Waypoint *waypoint)
{
    FMC_RoutePoint point;
    clear_route_point(&point);
    if (waypoint != NULL)
    {
        set_route_point_basic(&point, waypoint->ident, waypoint->type[0] ? waypoint->type : "FIX", waypoint->latitude, waypoint->longitude, 0.0);
    }
    return point;
}

static const char *fms_point_type_name(int type)
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

static int parse_fms_route_line(const char *line, FMC_RoutePoint *point)
{
    int type = 0;
    char ident[FMC_TEXT_LEN];
    double altitude = 0.0;
    double latitude = 0.0;
    double longitude = 0.0;

    if (line == NULL || point == NULL)
    {
        return 0;
    }

    ident[0] = '\0';
    if (sscanf(line, "%d %63s %lf %lf %lf", &type, ident, &altitude, &latitude, &longitude) != 5)
    {
        return 0;
    }

    set_route_point_basic(point, ident, fms_point_type_name(type), latitude, longitude, altitude);
    return 1;
}

static int is_airport_route_point(const FMC_RoutePoint *point)
{
    return point != NULL && strcmp(point->type, "AIRPORT") == 0;
}

static void sync_legacy_route_points(FMC_Data *data);

static int route_plan_max_int(int a, int b)
{
    return a > b ? a : b;
}

static int route_plan_height(FMC_RoutePlanNode *node)
{
    return node != NULL ? node->height : 0;
}

static int route_plan_balance(FMC_RoutePlanNode *node)
{
    return node != NULL ? route_plan_height(node->left) - route_plan_height(node->right) : 0;
}

static void route_plan_update_height(FMC_RoutePlanNode *node)
{
    if (node != NULL)
    {
        node->height = 1 + route_plan_max_int(route_plan_height(node->left), route_plan_height(node->right));
    }
}

static FMC_RoutePlanNode *route_plan_rotate_right(FMC_RoutePlanNode *y)
{
    FMC_RoutePlanNode *x = y->left;
    FMC_RoutePlanNode *t2 = x->right;
    x->right = y;
    y->left = t2;
    route_plan_update_height(y);
    route_plan_update_height(x);
    return x;
}

static FMC_RoutePlanNode *route_plan_rotate_left(FMC_RoutePlanNode *x)
{
    FMC_RoutePlanNode *y = x->right;
    FMC_RoutePlanNode *t2 = y->left;
    y->left = x;
    x->right = t2;
    route_plan_update_height(x);
    route_plan_update_height(y);
    return y;
}

static void make_route_plan_key(const char *origin, const char *destination, char *key, int key_size)
{
    if (key == NULL || key_size <= 0)
    {
        return;
    }

    snprintf(key, (size_t)key_size, "%s-%s", origin != NULL ? origin : "", destination != NULL ? destination : "");
}

static int has_fms_extension(const char *name)
{
    size_t len = name != NULL ? strlen(name) : 0;
    if (len < 4)
    {
        return 0;
    }

    const char *ext = name + len - 4;
    return toupper((unsigned char)ext[0]) == '.' &&
           toupper((unsigned char)ext[1]) == 'F' &&
           toupper((unsigned char)ext[2]) == 'M' &&
           toupper((unsigned char)ext[3]) == 'S';
}

static int read_fms_route_metadata(const char *path,
                                   char *origin,
                                   int origin_size,
                                   char *destination,
                                   int destination_size,
                                   int *route_count,
                                   int *cruise_altitude)
{
    FILE *file = NULL;
    char line[256];
    int line_no = 0;
    int expected_count = -1;
    int point_count = 0;
    int origin_index = -1;
    int destination_index = -1;
    double max_altitude = 0.0;
    FMC_RoutePoint points[FMC_MAX_ROUTE_POINTS];

    if (path == NULL || origin == NULL || destination == NULL)
    {
        return 0;
    }

    for (int i = 0; i < FMC_MAX_ROUTE_POINTS; ++i)
    {
        clear_route_point(&points[i]);
    }

    file = fopen(path, "r");
    if (file == NULL)
    {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *cursor = line;
        line_no++;
        line[strcspn(line, "\r\n")] = '\0';

        while (*cursor != '\0' && isspace((unsigned char)*cursor))
        {
            ++cursor;
        }
        if (*cursor == '\0')
        {
            continue;
        }

        if (line_no == 1)
        {
            if (strcmp(cursor, "I") != 0)
            {
                fclose(file);
                return 0;
            }
            continue;
        }
        if (line_no == 2)
        {
            if (strstr(cursor, "version") == NULL)
            {
                fclose(file);
                return 0;
            }
            continue;
        }
        if (line_no == 3)
        {
            continue;
        }
        if (line_no == 4)
        {
            if (sscanf(cursor, "%d", &expected_count) != 1 || expected_count <= 0)
            {
                fclose(file);
                return 0;
            }
            continue;
        }

        if (point_count >= FMC_MAX_ROUTE_POINTS)
        {
            break;
        }

        if (parse_fms_route_line(cursor, &points[point_count]))
        {
            if (is_airport_route_point(&points[point_count]))
            {
                if (origin_index < 0)
                {
                    origin_index = point_count;
                }
                destination_index = point_count;
            }
            if (points[point_count].altitude > max_altitude)
            {
                max_altitude = points[point_count].altitude;
            }
            point_count++;
        }
    }

    fclose(file);

    if (point_count <= 0 || (expected_count > 0 && point_count <= 0))
    {
        return 0;
    }
    if (origin_index < 0)
    {
        origin_index = 0;
    }
    if (destination_index < 0)
    {
        destination_index = point_count - 1;
    }

    set_text(origin, origin_size, points[origin_index].ident);
    set_text(destination, destination_size, points[destination_index].ident);
    if (route_count != NULL)
    {
        *route_count = point_count;
    }
    if (cruise_altitude != NULL)
    {
        *cruise_altitude = max_altitude > 0.0 ? (int)max_altitude : 0;
    }
    return 1;
}

static FMC_RoutePlanNode *new_route_plan_node(const char *origin,
                                              const char *destination,
                                              const char *path,
                                              int route_count,
                                              int cruise_altitude)
{
    FMC_RoutePlanNode *node = (FMC_RoutePlanNode *)calloc(1, sizeof(FMC_RoutePlanNode));
    if (node == NULL)
    {
        return NULL;
    }

    set_text(node->origin, sizeof(node->origin), origin);
    set_text(node->destination, sizeof(node->destination), destination);
    set_text(node->path, sizeof(node->path), path);
    make_route_plan_key(origin, destination, node->key, sizeof(node->key));
    node->route_count = route_count;
    node->cruise_altitude = cruise_altitude;
    node->height = 1;
    return node;
}

static FMC_RoutePlanNode *insert_route_plan_node(FMC_RoutePlanNode *node,
                                                 const char *origin,
                                                 const char *destination,
                                                 const char *path,
                                                 int route_count,
                                                 int cruise_altitude,
                                                 int *inserted)
{
    char key[FMC_TEXT_LEN * 2];
    make_route_plan_key(origin, destination, key, sizeof(key));

    if (node == NULL)
    {
        FMC_RoutePlanNode *created = new_route_plan_node(origin, destination, path, route_count, cruise_altitude);
        if (created != NULL && inserted != NULL)
        {
            *inserted = 1;
        }
        return created;
    }

    int cmp = strcmp(key, node->key);
    if (cmp < 0)
    {
        node->left = insert_route_plan_node(node->left, origin, destination, path, route_count, cruise_altitude, inserted);
    }
    else if (cmp > 0)
    {
        node->right = insert_route_plan_node(node->right, origin, destination, path, route_count, cruise_altitude, inserted);
    }
    else
    {
        set_text(node->path, sizeof(node->path), path);
        node->route_count = route_count;
        node->cruise_altitude = cruise_altitude;
        return node;
    }

    route_plan_update_height(node);
    int balance = route_plan_balance(node);

    if (balance > 1 && strcmp(key, node->left->key) < 0)
    {
        return route_plan_rotate_right(node);
    }
    if (balance < -1 && strcmp(key, node->right->key) > 0)
    {
        return route_plan_rotate_left(node);
    }
    if (balance > 1 && strcmp(key, node->left->key) > 0)
    {
        node->left = route_plan_rotate_left(node->left);
        return route_plan_rotate_right(node);
    }
    if (balance < -1 && strcmp(key, node->right->key) < 0)
    {
        node->right = route_plan_rotate_right(node->right);
        return route_plan_rotate_left(node);
    }

    return node;
}

static FMC_RoutePlanNode *find_route_plan_node(const char *origin, const char *destination)
{
    char key[FMC_TEXT_LEN * 2];
    make_route_plan_key(origin, destination, key, sizeof(key));

    FMC_RoutePlanNode *node = route_plan_root;
    while (node != NULL)
    {
        int cmp = strcmp(key, node->key);
        if (cmp == 0)
        {
            return node;
        }
        node = cmp < 0 ? node->left : node->right;
    }

    return NULL;
}

static void index_route_plan_file(const char *path)
{
    char origin[FMC_TEXT_LEN];
    char destination[FMC_TEXT_LEN];
    int route_count = 0;
    int cruise_altitude = 0;

    if (!read_fms_route_metadata(path, origin, sizeof(origin), destination, sizeof(destination), &route_count, &cruise_altitude))
    {
        printf("FMC route index: skipped invalid plan %s\n", path);
        return;
    }

    int inserted = 0;
    route_plan_root = insert_route_plan_node(route_plan_root, origin, destination, path, route_count, cruise_altitude, &inserted);
    if (inserted)
    {
        route_plan_count++;
    }
    printf("FMC route index: %s -> %s (%d points) from %s\n", origin, destination, route_count, path);
}

static void scan_route_plan_dir(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        printf("FMC route index: route dir not found %s\n", dir_path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        char path[FMC_ROUTE_PLAN_PATH_LEN];
        if (!has_fms_extension(entry->d_name))
        {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        index_route_plan_file(path);
    }

    closedir(dir);
}

static int fmc_route_plan_index_load(void)
{
    if (route_plan_loaded)
    {
        return route_plan_count;
    }

    const char *route_dirs[] = {
        "assets",
        "../assets",
        "../../assets",
        NULL};

    printf("FMC route index: scanning route files\n");
    for (int i = 0; route_dirs[i] != NULL; ++i)
    {
        scan_route_plan_dir(route_dirs[i]);
    }

    route_plan_loaded = 1;
    printf("FMC route index: loaded %d route plan(s)\n", route_plan_count);
    return route_plan_count;
}

static void clear_configured_route(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    free(data->configured_route);
    data->configured_route = NULL;
    data->configured_route_count = 0;
    data->configured_route_capacity = 0;
    data->configured_route_page = 0;
    data->route_loaded_from_file = 0;
    data->fms_plan_path[0] = '\0';
    sync_legacy_route_points(data);
}

static void sync_legacy_route_points(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    int out = 0;
    if (data->origin[0] != '\0' && out < FMC_MAX_ROUTE_POINTS)
    {
        set_text(data->route_points[out++], sizeof(data->route_points[0]), data->origin);
        set_route_point_basic(&data->route_point_data[out - 1], data->origin, "AIRPORT", 0.0, 0.0, 0.0);
    }

    for (int i = 0; i < data->configured_route_count && out < FMC_MAX_ROUTE_POINTS - 1; ++i)
    {
        set_text(data->route_points[out++], sizeof(data->route_points[0]), data->configured_route[i].ident);
        data->route_point_data[out - 1] = data->configured_route[i];
    }

    if (data->destination[0] != '\0' && out < FMC_MAX_ROUTE_POINTS)
    {
        set_text(data->route_points[out++], sizeof(data->route_points[0]), data->destination);
        set_route_point_basic(&data->route_point_data[out - 1], data->destination, "AIRPORT", 0.0, 0.0, 0.0);
    }

    data->route_count = out;
    if (data->active_leg_index < 0)
    {
        data->active_leg_index = 0;
    }
    if (data->active_leg_index >= data->route_count)
    {
        data->active_leg_index = data->route_count > 0 ? data->route_count - 1 : 0;
    }
    for (int i = out; i < FMC_MAX_ROUTE_POINTS; ++i)
    {
        data->route_points[i][0] = '\0';
        clear_route_point(&data->route_point_data[i]);
    }
}

static void load_default_mock_route(FMC_Data *data)
{
    static const FMC_RoutePoint default_points[] = {
        {"BLI", "FIX", 48.905200, -122.510800, 0.0, "", ""},
        {"FREDY", "FIX", 47.557800, -122.289200, 0.0, "", ""},
        {"RENTO", "FIX", 47.484700, -122.231900, 0.0, "", ""},
        {"TOTEM", "FIX", 47.450000, -122.183300, 0.0, "", ""},
        {"BOTLL", "FIX", 47.415800, -122.135800, 0.0, "", ""},
        {"KIRBY", "FIX", 47.380800, -122.087500, 0.0, "", ""},
    };
    const int default_count = (int)(sizeof(default_points) / sizeof(default_points[0]));

    if (data == NULL)
    {
        return;
    }

    set_text(data->origin, sizeof(data->origin), "KSEA");
    set_text(data->destination, sizeof(data->destination), "KBFI");

    free(data->configured_route);
    data->configured_route = NULL;
    data->configured_route_count = 0;
    data->configured_route_capacity = 0;
    data->configured_route_page = 0;
    data->active_leg_index = 0;
    data->route_loaded_from_file = 0;
    data->fms_plan_path[0] = '\0';

    data->configured_route = (FMC_RoutePoint *)calloc((size_t)default_count, sizeof(FMC_RoutePoint));
    if (data->configured_route != NULL)
    {
        for (int i = 0; i < default_count; ++i)
        {
            data->configured_route[i] = default_points[i];
        }
        data->configured_route_count = default_count;
        data->configured_route_capacity = default_count;
    }
    else
    {
        printf("FMC init warning: default mock route memory allocation failed\n");
    }

    sync_legacy_route_points(data);
    if (data->route_count > 0)
    {
        set_route_point_basic(&data->route_point_data[0], data->origin, "AIRPORT", 47.448900, -122.309400, 0.0);
    }
    if (data->route_count > 1)
    {
        set_route_point_basic(&data->route_point_data[data->route_count - 1], data->destination, "AIRPORT", 47.540100, -122.309400, 0.0);
    }
}

int fmc_data_load_fms_plan(FMC_Data *data, const char *path)
{
    FILE *file = NULL;
    char line[256];
    int line_no = 0;
    int expected_count = -1;
    int point_count = 0;
    int origin_index = -1;
    int destination_index = -1;
    double max_altitude = 0.0;
    FMC_RoutePoint points[FMC_MAX_ROUTE_POINTS];

    if (data == NULL || path == NULL)
    {
        return 0;
    }

    for (int i = 0; i < FMC_MAX_ROUTE_POINTS; ++i)
    {
        clear_route_point(&points[i]);
    }

    errno = 0;
    file = fopen(path, "r");
    if (file == NULL)
    {
        printf("FMC FMS load failed: %s (%s)\n", path, errno != 0 ? strerror(errno) : "open failed");
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *cursor = line;
        line_no++;
        line[strcspn(line, "\r\n")] = '\0';

        while (*cursor != '\0' && isspace((unsigned char)*cursor))
        {
            ++cursor;
        }
        if (*cursor == '\0')
        {
            continue;
        }

        if (line_no == 1)
        {
            if (strcmp(cursor, "I") != 0)
            {
                printf("FMC FMS load failed: %s invalid header\n", path);
                fclose(file);
                return 0;
            }
            continue;
        }

        if (line_no == 2)
        {
            if (strstr(cursor, "version") == NULL)
            {
                printf("FMC FMS load failed: %s missing version line\n", path);
                fclose(file);
                return 0;
            }
            continue;
        }

        if (line_no == 3)
        {
            continue;
        }

        if (line_no == 4)
        {
            if (sscanf(cursor, "%d", &expected_count) != 1 || expected_count <= 0)
            {
                printf("FMC FMS load failed: %s invalid route count\n", path);
                fclose(file);
                return 0;
            }
            continue;
        }

        if (point_count >= FMC_MAX_ROUTE_POINTS)
        {
            printf("FMC FMS load warning: %s route truncated at %d points\n", path, FMC_MAX_ROUTE_POINTS);
            break;
        }

        if (parse_fms_route_line(cursor, &points[point_count]))
        {
            if (is_airport_route_point(&points[point_count]))
            {
                if (origin_index < 0)
                {
                    origin_index = point_count;
                }
                destination_index = point_count;
            }
            if (points[point_count].altitude > max_altitude)
            {
                max_altitude = points[point_count].altitude;
            }
            point_count++;
        }
    }

    fclose(file);

    if (point_count <= 0)
    {
        printf("FMC FMS load failed: %s no route points\n", path);
        return 0;
    }

    if (expected_count > 0 && expected_count != point_count)
    {
        printf("FMC FMS load warning: %s expected %d points, parsed %d\n", path, expected_count, point_count);
    }

    if (origin_index < 0)
    {
        origin_index = 0;
    }
    if (destination_index < 0)
    {
        destination_index = point_count - 1;
    }

    int interior_count = 0;
    for (int i = 0; i < point_count; ++i)
    {
        if (i != origin_index && i != destination_index)
        {
            interior_count++;
        }
    }

    FMC_RoutePoint *new_route = NULL;
    if (interior_count > 0)
    {
        new_route = (FMC_RoutePoint *)calloc((size_t)interior_count, sizeof(FMC_RoutePoint));
        if (new_route == NULL)
        {
            printf("FMC FMS load failed: %s route memory allocation failed\n", path);
            return 0;
        }
    }

    free(data->configured_route);
    data->configured_route = new_route;
    data->configured_route_count = 0;
    data->configured_route_capacity = interior_count;

    set_text(data->origin, sizeof(data->origin), points[origin_index].ident);
    set_text(data->destination, sizeof(data->destination), points[destination_index].ident);

    for (int i = 0; i < point_count; ++i)
    {
        if (i != origin_index && i != destination_index)
        {
            data->configured_route[data->configured_route_count++] = points[i];
        }
    }

    data->configured_route_page = 0;
    data->active_leg_index = 0;
    if (max_altitude > 0.0)
    {
        data->cruise_altitude = (int)max_altitude;
        data->cruise_altitude_from_file = 1;
    }
    else
    {
        data->cruise_altitude_from_file = 0;
    }
    sync_legacy_route_points(data);

    if (data->route_count > 0)
    {
        data->route_point_data[0] = points[origin_index];
    }
    if (data->route_count > 1)
    {
        data->route_point_data[data->route_count - 1] = points[destination_index];
    }

    data->route_loaded_from_file = 1;
    set_text(data->fms_plan_path, sizeof(data->fms_plan_path), path);

    printf("FMC FMS: load success\n");
    printf("  path: %s\n", path);
    printf("  route points: %d\n", point_count);
    printf("  origin: %s\n", data->origin);
    printf("  destination: %s\n", data->destination);
    if (data->cruise_altitude_from_file)
    {
        printf("  cruise altitude: %d ft (from file)\n", data->cruise_altitude);
    }
    else
    {
        printf("  cruise altitude: %d ft (default, no file altitude)\n", data->cruise_altitude);
    }
    fflush(stdout);
    return 1;
}

static int apply_indexed_route_for_airports(FMC_Data *data, int clear_on_miss)
{
    if (data == NULL)
    {
        return 0;
    }

    fmc_route_plan_index_load();

    FMC_RoutePlanNode *plan = find_route_plan_node(data->origin, data->destination);
    if (plan == NULL)
    {
        if (clear_on_miss)
        {
            clear_configured_route(data);
        }
        snprintf(data->message, sizeof(data->message), "NO RTE %s-%s", data->origin, data->destination);
        printf("FMC route query: no indexed route for %s-%s, %s\n",
               data->origin,
               data->destination,
               clear_on_miss ? "using direct legs only" : "keeping fallback route");
        fflush(stdout);
        return 0;
    }

    printf("FMC route query: matched %s-%s using %s\n",
           plan->origin,
           plan->destination,
           plan->path);
    if (fmc_data_load_fms_plan(data, plan->path))
    {
        snprintf(data->message, sizeof(data->message), "RTE %s-%s LOADED", data->origin, data->destination);
        return 1;
    }

    clear_configured_route(data);
    snprintf(data->message, sizeof(data->message), "RTE LOAD FAILED");
    return 0;
}

void fmc_data_init(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->current_page = FMC_PAGE_INDEX;
    data->previous_page = FMC_PAGE_INDEX;
    data->page_switch_count = 0;
    data->active_phase = FMC_PHASE_PREFLIGHT;

    set_text(data->company_route, sizeof(data->company_route), "");
    set_text(data->route_via, sizeof(data->route_via), "");
    set_text(data->flight_no, sizeof(data->flight_no), "CA001");

    data->configured_route = NULL;
    data->configured_route_count = 0;
    data->configured_route_capacity = 0;
    data->configured_route_page = 0;
    data->route_loaded_from_file = 0;
    data->fms_plan_path[0] = '\0';
    data->active_leg_index = 0;
    load_default_mock_route(data);

    data->cruise_altitude = 35000;
    data->cruise_altitude_from_file = 0;
    data->target_speed = 280;
    data->cost_index = 45.0f;
    data->climb_speed = 250;
    data->climb_accel_altitude = 1500;
    data->climb_thrust_limit = 92;
    set_text(data->climb_target_speed_text, sizeof(data->climb_target_speed_text), "250/.74");
    set_text(data->climb_spd_alt_limit_text, sizeof(data->climb_spd_alt_limit_text), "250/10000");
    set_text(data->climb_transition_alt_text, sizeof(data->climb_transition_alt_text), "18000");
    data->cruise_speed = 280;
    data->descent_speed = 280;
    data->descent_vertical_speed = 1800;
    data->descent_transition_level = 18000;

    set_text(data->departure_runway, sizeof(data->departure_runway), "");
    set_text(data->arrival_runway, sizeof(data->arrival_runway), "");
    set_text(data->departure_procedure, sizeof(data->departure_procedure), "");
    set_text(data->arrival_procedure, sizeof(data->arrival_procedure), "");
    set_text(data->departure_transition, sizeof(data->departure_transition), "");
    set_text(data->arrival_transition, sizeof(data->arrival_transition), "");
    set_text(data->legs_sequence, sizeof(data->legs_sequence), "AUTO/INHIBIT");
    set_text(data->hold_fix, sizeof(data->hold_fix), "");
    set_text(data->hold_inbound_course, sizeof(data->hold_inbound_course), "");
    set_text(data->hold_turn_direction, sizeof(data->hold_turn_direction), "");
    set_text(data->hold_leg_time, sizeof(data->hold_leg_time), "");
    set_text(data->hold_speed_altitude, sizeof(data->hold_speed_altitude), "");

    data->scratchpad[0] = '\0';
    data->scratchpad_len = 0;
    data->message[0] = '\0';
    data->airport_index_count = fmc_airport_index_count();
    clear_airport_selection(data);
    data->has_selected_origin_airport = 0;
    data->xpc_status[0] = '\0';
    data->route_input_mode = FMC_RTE_INPUT_WAYPOINT;
    data->waypoint_index_count = fmc_waypoint_index_count();
    clear_waypoint_selection(data);

    printf("FMC init: default mock data ready\n");
    printf("  route: %s-%s (%d points)\n", data->origin, data->destination, data->route_count);
    printf("  flight no: %s\n", data->flight_no);
    printf("  cruise altitude: %d ft (default)\n", data->cruise_altitude);
    printf("  target speed: %d kt (default)\n", data->target_speed);
    printf("  cost index: %.0f (default)\n", data->cost_index);
    printf("  departure runway: ---- (default placeholder)\n");
    printf("  arrival runway: ---- (default placeholder)\n");

    if (!apply_indexed_route_for_airports(data, 0))
    {
        printf("FMC fallback: using default mock route\n");
        printf("  route: %s-%s (%d points)\n", data->origin, data->destination, data->route_count);
        printf("  cruise altitude: %d ft (default)\n", data->cruise_altitude);
        fflush(stdout);
    }
}

void fmc_data_update_mock(FMC_Data *data, float delta_time)
{
    (void)delta_time;

    if (data == NULL)
    {
        return;
    }
}

void fmc_data_set_page(FMC_Data *data, FMC_Page page)
{
    if (data == NULL)
    {
        return;
    }

    if (page < FMC_PAGE_HOME || page >= FMC_PAGE_COUNT)
    {
        return;
    }

    if (data->current_page != page)
    {
        data->previous_page = data->current_page;
        data->page_switch_count++;
        fmc_data_clear_scratchpad(data);
    }

    data->current_page = page;
    data->message[0] = '\0';
}

void fmc_data_append_char(FMC_Data *data, char c)
{
    if (data == NULL)
    {
        return;
    }

    if (c < 32 || c > 126)
    {
        return;
    }

    if (data->scratchpad_len >= FMC_TEXT_LEN - 1)
    {
        return;
    }

    data->scratchpad[data->scratchpad_len++] = (char)toupper((unsigned char)c);
    data->scratchpad[data->scratchpad_len] = '\0';
    data->message[0] = '\0';
    if (data->current_page == FMC_PAGE_ROUTE)
    {
        if (data->route_input_mode == FMC_RTE_INPUT_AIRPORT)
        {
            fmc_data_query_airports(data);
        }
        else
        {
            fmc_data_query_waypoints(data);
        }
    }
}

void fmc_data_backspace(FMC_Data *data)
{
    if (data == NULL || data->scratchpad_len <= 0)
    {
        return;
    }

    data->scratchpad_len--;
    data->scratchpad[data->scratchpad_len] = '\0';
    data->message[0] = '\0';
    if (data->current_page == FMC_PAGE_ROUTE)
    {
        if (data->route_input_mode == FMC_RTE_INPUT_AIRPORT)
        {
            fmc_data_query_airports(data);
        }
        else
        {
            fmc_data_query_waypoints(data);
        }
    }
}

void fmc_data_clear_scratchpad(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->scratchpad[0] = '\0';
    data->scratchpad_len = 0;
    data->message[0] = '\0';
    if (data->current_page == FMC_PAGE_ROUTE)
    {
        clear_airport_selection(data);
        clear_waypoint_selection(data);
    }
}

void fmc_data_destroy(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    free(data->configured_route);
    data->configured_route = NULL;
    data->configured_route_count = 0;
    data->configured_route_capacity = 0;
}

int fmc_data_commit_scratchpad_to_origin(FMC_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    fmc_data_query_airports(data);
    if (data->airport_matches.count > 0)
    {
        fmc_data_select_airport_candidate(data, 0);
        return 1;
    }

    if (data->scratchpad_len != 4)
    {
        set_text(data->message, sizeof(data->message), "ORIGIN MUST BE 4 LETTERS");
        return 0;
    }

    for (int i = 0; i < data->scratchpad_len; ++i)
    {
        if (!isalpha((unsigned char)data->scratchpad[i]))
        {
            set_text(data->message, sizeof(data->message), "ORIGIN FORMAT INVALID");
            return 0;
        }
    }

    set_text(data->origin, sizeof(data->origin), data->scratchpad);
    data->departure_runway[0] = '\0';
    data->departure_procedure[0] = '\0';
    data->departure_transition[0] = '\0';
    fmc_data_clear_scratchpad(data);
    apply_indexed_route_for_airports(data, 1);
    snprintf(data->message, sizeof(data->message), "ORIGIN %s %s", data->origin, data->route_loaded_from_file ? "RTE LOADED" : "DIRECT RTE");
    return 1;
}

void fmc_data_query_airports(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->airport_matches.count = 0;
    data->selected_airport_index = -1;
    set_text(data->airport_query, sizeof(data->airport_query), data->scratchpad);

    if (data->scratchpad_len <= 0)
    {
        data->message[0] = '\0';
        return;
    }

    int count = fmc_airport_search(data->scratchpad, &data->airport_matches);
    if (count > 0)
    {
        data->selected_airport_index = 0;
        set_text(data->message, sizeof(data->message), "SELECT AIRPORT THEN EXEC");
    }
    else
    {
        set_text(data->message, sizeof(data->message), "NO AIRPORT MATCH");
    }
}

int fmc_data_select_airport_candidate(FMC_Data *data, int index)
{
    if (data == NULL || index < 0 || index >= data->airport_matches.count)
    {
        return 0;
    }

    data->selected_airport_index = index;
    data->selected_origin_airport = data->airport_matches.items[index];
    data->has_selected_origin_airport = 1;
    snprintf(data->message, sizeof(data->message), "SELECTED %s - EXEC", data->selected_origin_airport.ident);
    return 1;
}

int fmc_data_confirm_selected_airport(FMC_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    if (data->selected_airport_index < 0 && data->scratchpad_len > 0)
    {
        fmc_data_query_airports(data);
    }

    if (data->selected_airport_index < 0 ||
        data->selected_airport_index >= data->airport_matches.count)
    {
        set_text(data->message, sizeof(data->message), "NO AIRPORT SELECTED");
        return 0;
    }

    data->selected_origin_airport = data->airport_matches.items[data->selected_airport_index];
    data->has_selected_origin_airport = 1;
    set_text(data->origin, sizeof(data->origin), data->selected_origin_airport.ident);
    data->departure_runway[0] = '\0';
    data->departure_procedure[0] = '\0';
    data->departure_transition[0] = '\0';
    apply_indexed_route_for_airports(data, 1);
    if (data->route_count > 0)
    {
        set_route_point_basic(&data->route_point_data[0], data->origin, "AIRPORT", data->selected_origin_airport.latitude, data->selected_origin_airport.longitude, 0.0);
    }

    fmc_xpc_sync_origin_airport(&data->selected_origin_airport, data->xpc_status, sizeof(data->xpc_status));
    fmc_data_clear_scratchpad(data);
    snprintf(data->message, sizeof(data->message), "ORIGIN %s %s", data->origin, data->route_loaded_from_file ? "RTE LOADED" : "DIRECT RTE");
    return 1;
}

void fmc_data_set_route_input_mode(FMC_Data *data, FMC_RTE_InputMode mode)
{
    if (data == NULL)
    {
        return;
    }

    data->route_input_mode = mode;
    clear_airport_selection(data);
    clear_waypoint_selection(data);
    if (data->scratchpad_len > 0)
    {
        if (mode == FMC_RTE_INPUT_AIRPORT)
        {
            fmc_data_query_airports(data);
        }
        else
        {
            fmc_data_query_waypoints(data);
        }
    }
}

void fmc_data_query_waypoints(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->waypoint_matches.count = 0;
    data->selected_waypoint_index = -1;
    data->has_selected_waypoint = 0;
    set_text(data->waypoint_query, sizeof(data->waypoint_query), data->scratchpad);

    if (data->scratchpad_len <= 0)
    {
        data->message[0] = '\0';
        return;
    }

    int count = fmc_waypoint_search(data->scratchpad, &data->waypoint_matches);
    if (count > 0)
    {
        data->selected_waypoint_index = 0;
        data->selected_waypoint = data->waypoint_matches.items[0];
        data->has_selected_waypoint = 1;
        set_text(data->message, sizeof(data->message), "SELECT FIX THEN EXEC");
    }
    else
    {
        set_text(data->message, sizeof(data->message), "NO FIX MATCH");
    }
}

int fmc_data_select_waypoint_candidate(FMC_Data *data, int index)
{
    if (data == NULL || index < 0 || index >= data->waypoint_matches.count)
    {
        return 0;
    }

    data->selected_waypoint_index = index;
    data->selected_waypoint = data->waypoint_matches.items[index];
    data->has_selected_waypoint = 1;
    snprintf(data->message, sizeof(data->message), "SELECTED %s - EXEC", data->selected_waypoint.ident);
    return 1;
}

int fmc_data_add_selected_waypoint(FMC_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    if (!data->has_selected_waypoint && data->scratchpad_len > 0)
    {
        fmc_data_query_waypoints(data);
    }

    if (!data->has_selected_waypoint)
    {
        set_text(data->message, sizeof(data->message), "NO FIX SELECTED");
        return 0;
    }

    for (int i = 0; i < data->configured_route_count; ++i)
    {
        if (strcmp(data->configured_route[i].ident, data->selected_waypoint.ident) == 0)
        {
            set_text(data->message, sizeof(data->message), "FIX ALREADY IN RTE");
            return 0;
        }
    }

    if (data->configured_route_count >= data->configured_route_capacity)
    {
        int new_capacity = data->configured_route_capacity == 0 ? 8 : data->configured_route_capacity * 2;
        FMC_RoutePoint *new_items = (FMC_RoutePoint *)realloc(data->configured_route, (size_t)new_capacity * sizeof(FMC_RoutePoint));
        if (new_items == NULL)
        {
            set_text(data->message, sizeof(data->message), "RTE MEMORY FULL");
            return 0;
        }
        data->configured_route = new_items;
        data->configured_route_capacity = new_capacity;
    }

    data->configured_route[data->configured_route_count++] = route_point_from_waypoint(&data->selected_waypoint);
    data->configured_route_page = fmc_data_route_page_count(data) - 1;
    sync_legacy_route_points(data);
    fmc_data_clear_scratchpad(data);
    snprintf(data->message, sizeof(data->message), "ADDED %s", data->configured_route[data->configured_route_count - 1].ident);
    return 1;
}

int fmc_data_route_page_count(const FMC_Data *data)
{
    if (data == NULL || data->route_count <= 0)
    {
        return 1;
    }

    return (data->route_count + FMC_RTE_PAGE_SIZE - 1) / FMC_RTE_PAGE_SIZE;
}

int fmc_data_route_next_page(FMC_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    int page_count = fmc_data_route_page_count(data);
    if (data->configured_route_page + 1 >= page_count)
    {
        set_text(data->message, sizeof(data->message), "LAST RTE PAGE");
        return 0;
    }

    data->configured_route_page++;
    return 1;
}

int fmc_data_route_prev_page(FMC_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    if (data->configured_route_page <= 0)
    {
        set_text(data->message, sizeof(data->message), "FIRST RTE PAGE");
        return 0;
    }

    data->configured_route_page--;
    return 1;
}

int fmc_data_set_route_field(FMC_Data *data, FMC_RouteField field)
{
    if (data == NULL)
    {
        return 0;
    }

    if (data->scratchpad_len <= 0)
    {
        set_text(data->message, sizeof(data->message), "ENTER RTE VALUE");
        return 0;
    }

    if (field == FMC_ROUTE_FIELD_ORIGIN)
    {
        if (!scratchpad_airport_code(data, data->origin, sizeof(data->origin), "ORIGIN"))
        {
            return 0;
        }
        data->departure_runway[0] = '\0';
        data->departure_procedure[0] = '\0';
        data->departure_transition[0] = '\0';
        apply_indexed_route_for_airports(data, 1);
        return 1;
    }

    if (field == FMC_ROUTE_FIELD_DESTINATION)
    {
        if (!scratchpad_airport_code(data, data->destination, sizeof(data->destination), "DEST"))
        {
            return 0;
        }
        data->arrival_runway[0] = '\0';
        data->arrival_procedure[0] = '\0';
        data->arrival_transition[0] = '\0';
        apply_indexed_route_for_airports(data, 1);
        return 1;
    }

    if (field == FMC_ROUTE_FIELD_COMPANY_ROUTE)
    {
        return scratchpad_text(data, data->company_route, sizeof(data->company_route), "CO ROUTE");
    }

    if (field == FMC_ROUTE_FIELD_FLIGHT_NO)
    {
        return scratchpad_text(data, data->flight_no, sizeof(data->flight_no), "FLT NO");
    }

    if (field == FMC_ROUTE_FIELD_VIA)
    {
        return scratchpad_text(data, data->route_via, sizeof(data->route_via), "VIA");
    }

    if (field == FMC_ROUTE_FIELD_TO_FIX)
    {
        fmc_data_set_route_input_mode(data, FMC_RTE_INPUT_WAYPOINT);
        fmc_data_query_waypoints(data);
        return fmc_data_add_selected_waypoint(data);
    }

    set_text(data->message, sizeof(data->message), "NO RTE FIELD");
    return 0;
}

int fmc_data_set_legs_parameter(FMC_Data *data, int field_index)
{
    if (data == NULL)
    {
        return 0;
    }

    if (field_index == 1)
    {
        return scratchpad_text(data, data->legs_sequence, sizeof(data->legs_sequence), "SEQUENCE");
    }

    set_text(data->message, sizeof(data->message), "NO LEGS FIELD");
    return 0;
}

int fmc_data_set_hold_parameter(FMC_Data *data, int field_index)
{
    if (data == NULL)
    {
        return 0;
    }

    if (field_index == 1)
    {
        return scratchpad_text(data, data->hold_fix, sizeof(data->hold_fix), "HOLD FIX");
    }
    if (field_index == 2)
    {
        return scratchpad_text(data, data->hold_inbound_course, sizeof(data->hold_inbound_course), "INBD CRS");
    }
    if (field_index == 3)
    {
        return scratchpad_text(data, data->hold_turn_direction, sizeof(data->hold_turn_direction), "TURN DIR");
    }
    if (field_index == 4)
    {
        return scratchpad_text(data, data->hold_leg_time, sizeof(data->hold_leg_time), "LEG TIME");
    }
    if (field_index == 5)
    {
        return scratchpad_text(data, data->hold_speed_altitude, sizeof(data->hold_speed_altitude), "SPD/ALT");
    }

    set_text(data->message, sizeof(data->message), "NO HOLD FIELD");
    return 0;
}

int fmc_data_exec_route_selection(FMC_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    if (data->current_page == FMC_PAGE_ROUTE && data->route_input_mode == FMC_RTE_INPUT_WAYPOINT)
    {
        return fmc_data_add_selected_waypoint(data);
    }

    return fmc_data_confirm_selected_airport(data);
}

int fmc_data_set_phase_parameter(FMC_Data *data, int line_select_index)
{
    int value = 0;
    if (data == NULL)
    {
        return 0;
    }

    if (data->current_page == FMC_PAGE_CLIMB)
    {
        if (data->scratchpad_len <= 0)
        {
            set_text(data->message, sizeof(data->message), "ENTER CLB VALUE");
            return 0;
        }

        if (line_select_index == 1)
        {
            if (!parse_first_number(data->scratchpad, &value) || value < 180 || value > 340)
            {
                set_text(data->message, sizeof(data->message), "CLB SPD RANGE 180-340");
                return 0;
            }
            set_text(data->climb_target_speed_text, sizeof(data->climb_target_speed_text), data->scratchpad);
            data->climb_speed = value;
            fmc_data_clear_scratchpad(data);
            set_text(data->message, sizeof(data->message), "TGT SPEED SET");
            return 1;
        }

        if (line_select_index == 2)
        {
            int speed = 0;
            int altitude = 0;
            if (!parse_two_numbers(data->scratchpad, &speed, &altitude) ||
                speed < 180 || speed > 340 ||
                altitude < 400 || altitude > 25000)
            {
                set_text(data->message, sizeof(data->message), "SPD/ALT FORMAT 250/10000");
                return 0;
            }
            set_text(data->climb_spd_alt_limit_text, sizeof(data->climb_spd_alt_limit_text), data->scratchpad);
            data->climb_speed = speed;
            data->climb_accel_altitude = altitude;
            fmc_data_clear_scratchpad(data);
            set_text(data->message, sizeof(data->message), "SPD/ALT LIMIT SET");
            return 1;
        }

        if (line_select_index == 3)
        {
            if (!parse_first_number(data->scratchpad, &value))
            {
                set_text(data->message, sizeof(data->message), "ENTER TRANS ALT");
                return 0;
            }
            if (value < 1000)
            {
                value *= 100;
            }
            if (value < 3000 || value > 25000)
            {
                set_text(data->message, sizeof(data->message), "TRANS ALT RANGE 3000-25000");
                return 0;
            }
            set_text(data->climb_transition_alt_text, sizeof(data->climb_transition_alt_text), data->scratchpad);
            data->descent_transition_level = value;
            fmc_data_clear_scratchpad(data);
            set_text(data->message, sizeof(data->message), "TRANS ALT SET");
            return 1;
        }
    }

    if (!scratchpad_number(data, &value))
    {
        set_text(data->message, sizeof(data->message), "ENTER NUMERIC VALUE");
        return 0;
    }

    if (data->current_page == FMC_PAGE_CRUISE)
    {
        if (line_select_index == 1)
        {
            return set_checked_int(data, &data->cruise_speed, value, 250, 330, "CRZ SPD");
        }
        if (line_select_index == 2)
        {
            if (value < 1000)
            {
                value *= 100;
            }
            return set_checked_int(data, &data->cruise_altitude, value, 18000, 45000, "CRZ ALT");
        }
        if (line_select_index == 3)
        {
            if (value < 0 || value > 999)
            {
                set_text(data->message, sizeof(data->message), "CI RANGE 0-999");
                return 0;
            }
            data->cost_index = (float)value;
            fmc_data_clear_scratchpad(data);
            snprintf(data->message, sizeof(data->message), "COST INDEX SET %d", value);
            return 1;
        }
    }
    else if (data->current_page == FMC_PAGE_DESCENT)
    {
        if (line_select_index == 1)
        {
            return set_checked_int(data, &data->descent_speed, value, 180, 340, "DES SPD");
        }
        if (line_select_index == 2)
        {
            if (value < 1000)
            {
                value *= 100;
            }
            return set_checked_int(data, &data->descent_transition_level, value, 3000, 25000, "TRANS LVL");
        }
        if (line_select_index == 3)
        {
            return set_checked_int(data, &data->descent_vertical_speed, value, 500, 4000, "DES V/S");
        }
    }

    set_text(data->message, sizeof(data->message), "NO PARAM ON LSK");
    return 0;
}

int fmc_data_activate_current_phase(FMC_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    if (data->current_page == FMC_PAGE_CLIMB)
    {
        data->active_phase = FMC_PHASE_CLIMB;
        data->target_speed = data->climb_speed;
        set_text(data->message, sizeof(data->message), "CLB PHASE ACTIVE");
        return 1;
    }
    if (data->current_page == FMC_PAGE_CRUISE)
    {
        data->active_phase = FMC_PHASE_CRUISE;
        data->target_speed = data->cruise_speed;
        set_text(data->message, sizeof(data->message), "CRZ PHASE ACTIVE");
        return 1;
    }
    if (data->current_page == FMC_PAGE_DESCENT)
    {
        data->active_phase = FMC_PHASE_DESCENT;
        data->target_speed = data->descent_speed;
        set_text(data->message, sizeof(data->message), "DES PHASE ACTIVE");
        return 1;
    }

    return 0;
}

int fmc_data_set_dep_arr_parameter(FMC_Data *data, int arrival_side, int field_index)
{
    if (data == NULL)
    {
        return 0;
    }

    if (data->scratchpad_len <= 0)
    {
        set_text(data->message, sizeof(data->message), "ENTER DEP/ARR VALUE");
        return 0;
    }

    const char *airport = arrival_side ? data->destination : data->origin;
    char *runway = arrival_side ? data->arrival_runway : data->departure_runway;
    char *procedure = arrival_side ? data->arrival_procedure : data->departure_procedure;
    char *transition = arrival_side ? data->arrival_transition : data->departure_transition;
    const char *side = arrival_side ? "ARR" : "DEP";

    if (field_index == 1)
    {
        if (!runway_supported(airport, data->scratchpad, arrival_side))
        {
            snprintf(data->message, sizeof(data->message), "%s RWY NOT MATCH", side);
            return 0;
        }
        if (procedure[0] != '\0' && !procedure_supported(airport, data->scratchpad, procedure, arrival_side))
        {
            snprintf(data->message, sizeof(data->message), "%s RWY/PROC MISMATCH", side);
            return 0;
        }
        if (procedure[0] != '\0' &&
            transition[0] != '\0' &&
            !transition_supported(airport, data->scratchpad, procedure, transition, arrival_side))
        {
            snprintf(data->message, sizeof(data->message), "%s RWY/TRANS MISMATCH", side);
            return 0;
        }

        set_text(runway, FMC_TEXT_LEN, data->scratchpad);
        fmc_data_clear_scratchpad(data);
        snprintf(data->message, sizeof(data->message), "%s RWY SET", side);
        return 1;
    }

    if (field_index == 2)
    {
        if (runway[0] != '\0' && !procedure_supported(airport, runway, data->scratchpad, arrival_side))
        {
            snprintf(data->message, sizeof(data->message), "%s PROC/RWY MISMATCH", side);
            return 0;
        }
        if (runway[0] == '\0' && !procedure_known_for_airport(airport, data->scratchpad, arrival_side))
        {
            snprintf(data->message, sizeof(data->message), "%s PROC UNKNOWN", side);
            return 0;
        }

        set_text(procedure, FMC_TEXT_LEN, data->scratchpad);
        transition[0] = '\0';
        fmc_data_clear_scratchpad(data);
        snprintf(data->message, sizeof(data->message), "%s PROC SET", side);
        return 1;
    }

    if (field_index == 3)
    {
        if (procedure[0] == '\0')
        {
            snprintf(data->message, sizeof(data->message), "%s PROC REQUIRED", side);
            return 0;
        }

        if (runway[0] != '\0' && !transition_supported(airport, runway, procedure, data->scratchpad, arrival_side))
        {
            snprintf(data->message, sizeof(data->message), "%s TRANS MISMATCH", side);
            return 0;
        }
        if (runway[0] == '\0' && !transition_known_for_procedure(airport, procedure, data->scratchpad, arrival_side))
        {
            snprintf(data->message, sizeof(data->message), "%s TRANS UNKNOWN", side);
            return 0;
        }

        set_text(transition, FMC_TEXT_LEN, data->scratchpad);
        fmc_data_clear_scratchpad(data);
        snprintf(data->message, sizeof(data->message), "%s TRANS SET", side);
        return 1;
    }

    set_text(data->message, sizeof(data->message), "NO DEP/ARR FIELD");
    return 0;
}

static int fmc_route_point_has_position(const FMC_RoutePoint *point)
{
    if (point == NULL)
    {
        return 0;
    }

    return point->latitude >= -90.0 && point->latitude <= 90.0 &&
           point->longitude >= -180.0 && point->longitude <= 180.0 &&
           !(point->latitude == 0.0 && point->longitude == 0.0);
}

int fmc_data_export_planned_route(const FMC_Data *data, SimPlannedRoute *route)
{
    if (data == NULL || route == NULL)
    {
        return 0;
    }

    memset(route, 0, sizeof(*route));
    set_text(route->origin, sizeof(route->origin), data->origin);
    set_text(route->destination, sizeof(route->destination), data->destination);
    set_text(route->source_path, sizeof(route->source_path), data->fms_plan_path);
    route->loaded_from_file = data->route_loaded_from_file;

    int coordinate_count = 0;
    for (int i = 0; i < data->route_count && route->point_count < SIM_ROUTE_MAX_POINTS; ++i)
    {
        const FMC_RoutePoint *source = &data->route_point_data[i];
        const char *ident = source->ident[0] != '\0' ? source->ident : data->route_points[i];
        if (ident == NULL || ident[0] == '\0')
        {
            continue;
        }

        SimRoutePoint *target = &route->points[route->point_count++];
        set_text(target->ident, sizeof(target->ident), ident);
        set_text(target->type, sizeof(target->type), source->type[0] != '\0' ? source->type : "POINT");
        target->latitude = source->latitude;
        target->longitude = source->longitude;
        target->altitude = source->altitude;
        target->has_position = fmc_route_point_has_position(source);
        if (target->has_position)
        {
            coordinate_count++;
        }
    }

    route->has_coordinates = route->point_count > 0 && coordinate_count == route->point_count;
    printf("FMC planned route export: %s -> %s, points=%d, coordinates=%s, source=%s.\n",
           route->origin[0] != '\0' ? route->origin : "----",
           route->destination[0] != '\0' ? route->destination : "----",
           route->point_count,
           route->has_coordinates ? "yes" : "partial/missing",
           route->source_path[0] != '\0' ? route->source_path : "fallback");
    fflush(stdout);
    return route->point_count > 0;
}

const char *fmc_data_phase_name(FMC_FlightPhase phase)
{
    switch (phase)
    {
    case FMC_PHASE_CLIMB:
        return "CLB";
    case FMC_PHASE_CRUISE:
        return "CRZ";
    case FMC_PHASE_DESCENT:
        return "DES";
    case FMC_PHASE_PREFLIGHT:
    default:
        return "PREFLT";
    }
}

const char *fmc_data_page_name(FMC_Page page)
{
    switch (page)
    {
    case FMC_PAGE_ROUTE:
        return "RTE";
    case FMC_PAGE_DEP_ARR:
        return "DEP/ARR";
    case FMC_PAGE_PERF:
        return "PERF";
    case FMC_PAGE_CLIMB:
        return "CLB";
    case FMC_PAGE_CRUISE:
        return "CRZ";
    case FMC_PAGE_DESCENT:
        return "DES";
    case FMC_PAGE_LEGS:
        return "LEGS";
    case FMC_PAGE_HOLD:
        return "HOLD";
    case FMC_PAGE_STATUS:
        return "STATUS";
    case FMC_PAGE_HOME:
    default:
        return "HOME";
    }
}
