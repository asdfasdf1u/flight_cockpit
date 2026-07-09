#include "fmc_waypoint.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FMC_WaypointNode
{
    FMC_Waypoint waypoint;
    int height;
    struct FMC_WaypointNode *left;
    struct FMC_WaypointNode *right;
} FMC_WaypointNode;

static FMC_WaypointNode *waypoint_root = NULL;
static int waypoint_count = 0;
static int waypoint_loaded = 0;

static int max_int(int a, int b)
{
    return a > b ? a : b;
}

static int node_height(FMC_WaypointNode *node)
{
    return node != NULL ? node->height : 0;
}

static int node_balance(FMC_WaypointNode *node)
{
    return node != NULL ? node_height(node->left) - node_height(node->right) : 0;
}

static void update_height(FMC_WaypointNode *node)
{
    if (node != NULL)
    {
        node->height = 1 + max_int(node_height(node->left), node_height(node->right));
    }
}

static FMC_WaypointNode *rotate_right(FMC_WaypointNode *y)
{
    FMC_WaypointNode *x = y->left;
    FMC_WaypointNode *t2 = x->right;
    x->right = y;
    y->left = t2;
    update_height(y);
    update_height(x);
    return x;
}

static FMC_WaypointNode *rotate_left(FMC_WaypointNode *x)
{
    FMC_WaypointNode *y = x->right;
    FMC_WaypointNode *t2 = y->left;
    y->left = x;
    x->right = t2;
    update_height(x);
    update_height(y);
    return y;
}

static void normalize_ident(const char *src, char *dest, int dest_size)
{
    int out = 0;
    if (dest == NULL || dest_size <= 0)
    {
        return;
    }

    if (src != NULL)
    {
        for (int i = 0; src[i] != '\0' && out < dest_size - 1; ++i)
        {
            if (isalnum((unsigned char)src[i]))
            {
                dest[out++] = (char)toupper((unsigned char)src[i]);
            }
        }
    }
    dest[out] = '\0';
}

static void set_text(char *dest, int dest_size, const char *src)
{
    if (dest == NULL || dest_size <= 0 || src == NULL)
    {
        return;
    }
    snprintf(dest, (size_t)dest_size, "%s", src);
}

static FMC_WaypointNode *new_node(const FMC_Waypoint *waypoint)
{
    FMC_WaypointNode *node = (FMC_WaypointNode *)malloc(sizeof(FMC_WaypointNode));
    if (node == NULL)
    {
        return NULL;
    }

    node->waypoint = *waypoint;
    node->height = 1;
    node->left = NULL;
    node->right = NULL;
    return node;
}

static FMC_WaypointNode *insert_node(FMC_WaypointNode *node, const FMC_Waypoint *waypoint, int *inserted)
{
    if (node == NULL)
    {
        FMC_WaypointNode *created = new_node(waypoint);
        if (created != NULL && inserted != NULL)
        {
            *inserted = 1;
        }
        return created;
    }

    int cmp = strcmp(waypoint->ident, node->waypoint.ident);
    if (cmp < 0)
    {
        node->left = insert_node(node->left, waypoint, inserted);
    }
    else if (cmp > 0)
    {
        node->right = insert_node(node->right, waypoint, inserted);
    }
    else
    {
        node->waypoint = *waypoint;
        return node;
    }

    update_height(node);
    int balance = node_balance(node);

    if (balance > 1 && strcmp(waypoint->ident, node->left->waypoint.ident) < 0)
    {
        return rotate_right(node);
    }
    if (balance < -1 && strcmp(waypoint->ident, node->right->waypoint.ident) > 0)
    {
        return rotate_left(node);
    }
    if (balance > 1 && strcmp(waypoint->ident, node->left->waypoint.ident) > 0)
    {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }
    if (balance < -1 && strcmp(waypoint->ident, node->right->waypoint.ident) < 0)
    {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }

    return node;
}

static int starts_with(const char *text, const char *prefix)
{
    size_t len = strlen(prefix);
    return len > 0 && strncmp(text, prefix, len) == 0;
}

static int contains_text(const char *text, const char *query)
{
    return query != NULL && query[0] != '\0' && strstr(text, query) != NULL;
}

static void add_match(FMC_WaypointMatchList *matches, const FMC_Waypoint *waypoint)
{
    if (matches == NULL || waypoint == NULL || matches->count >= FMC_WAYPOINT_MAX_MATCHES)
    {
        return;
    }

    for (int i = 0; i < matches->count; ++i)
    {
        if (strcmp(matches->items[i].ident, waypoint->ident) == 0)
        {
            return;
        }
    }

    matches->items[matches->count++] = *waypoint;
}

static void collect_prefix(FMC_WaypointNode *node, const char *query, FMC_WaypointMatchList *matches)
{
    if (node == NULL || matches == NULL || matches->count >= FMC_WAYPOINT_MAX_MATCHES)
    {
        return;
    }

    int cmp = strcmp(node->waypoint.ident, query);
    if (cmp >= 0)
    {
        collect_prefix(node->left, query, matches);
    }

    if (matches->count < FMC_WAYPOINT_MAX_MATCHES && starts_with(node->waypoint.ident, query))
    {
        add_match(matches, &node->waypoint);
    }

    if (matches->count < FMC_WAYPOINT_MAX_MATCHES &&
        (cmp < 0 || starts_with(node->waypoint.ident, query) || strncmp(node->waypoint.ident, query, strlen(query)) <= 0))
    {
        collect_prefix(node->right, query, matches);
    }
}

static void collect_contains(FMC_WaypointNode *node, const char *query, FMC_WaypointMatchList *matches)
{
    if (node == NULL || matches == NULL || matches->count >= FMC_WAYPOINT_MAX_MATCHES)
    {
        return;
    }

    collect_contains(node->left, query, matches);
    if (matches->count < FMC_WAYPOINT_MAX_MATCHES && contains_text(node->waypoint.ident, query))
    {
        add_match(matches, &node->waypoint);
    }
    collect_contains(node->right, query, matches);
}

int fmc_waypoint_index_load(const char *path)
{
    if (waypoint_loaded)
    {
        return waypoint_count;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        return 0;
    }

    char line[192];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char ident_raw[32];
        char type_raw[16];
        char region_raw[16];
        FMC_Waypoint waypoint;
        if (sscanf(line, "%lf %lf %31s %15s %15s",
                   &waypoint.latitude,
                   &waypoint.longitude,
                   ident_raw,
                   type_raw,
                   region_raw) != 5)
        {
            continue;
        }

        normalize_ident(ident_raw, waypoint.ident, sizeof(waypoint.ident));
        set_text(waypoint.type, sizeof(waypoint.type), type_raw);
        set_text(waypoint.region, sizeof(waypoint.region), region_raw);
        if (waypoint.ident[0] == '\0')
        {
            continue;
        }

        int inserted = 0;
        waypoint_root = insert_node(waypoint_root, &waypoint, &inserted);
        if (inserted)
        {
            waypoint_count++;
        }
    }

    fclose(file);
    waypoint_loaded = 1;
    return waypoint_count;
}

int fmc_waypoint_index_count(void)
{
    return waypoint_count;
}

int fmc_waypoint_find_exact(const char *ident, FMC_Waypoint *waypoint)
{
    char normalized[FMC_WAYPOINT_IDENT_LEN];
    normalize_ident(ident, normalized, sizeof(normalized));

    FMC_WaypointNode *node = waypoint_root;
    while (node != NULL)
    {
        int cmp = strcmp(normalized, node->waypoint.ident);
        if (cmp == 0)
        {
            if (waypoint != NULL)
            {
                *waypoint = node->waypoint;
            }
            return 1;
        }
        node = cmp < 0 ? node->left : node->right;
    }

    return 0;
}

int fmc_waypoint_search(const char *query, FMC_WaypointMatchList *matches)
{
    char normalized[FMC_WAYPOINT_IDENT_LEN];
    if (matches == NULL)
    {
        return 0;
    }

    matches->count = 0;
    normalize_ident(query, normalized, sizeof(normalized));
    if (normalized[0] == '\0' || waypoint_root == NULL)
    {
        return 0;
    }

    FMC_Waypoint exact;
    if (fmc_waypoint_find_exact(normalized, &exact))
    {
        add_match(matches, &exact);
    }

    collect_prefix(waypoint_root, normalized, matches);
    if (matches->count < FMC_WAYPOINT_MAX_MATCHES)
    {
        collect_contains(waypoint_root, normalized, matches);
    }

    return matches->count;
}
