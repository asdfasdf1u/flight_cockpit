#include "fmc_airport.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FMC_AirportNode
{
    FMC_Airport airport;
    int height;
    struct FMC_AirportNode *left;
    struct FMC_AirportNode *right;
} FMC_AirportNode;

static FMC_AirportNode *airport_root = NULL;
static int airport_count = 0;
static int airport_loaded = 0;

static int max_int(int a, int b)
{
    return a > b ? a : b;
}

static int node_height(FMC_AirportNode *node)
{
    return node != NULL ? node->height : 0;
}

static int node_balance(FMC_AirportNode *node)
{
    return node != NULL ? node_height(node->left) - node_height(node->right) : 0;
}

static void update_height(FMC_AirportNode *node)
{
    if (node != NULL)
    {
        node->height = 1 + max_int(node_height(node->left), node_height(node->right));
    }
}

static FMC_AirportNode *rotate_right(FMC_AirportNode *y)
{
    FMC_AirportNode *x = y->left;
    FMC_AirportNode *t2 = x->right;
    x->right = y;
    y->left = t2;
    update_height(y);
    update_height(x);
    return x;
}

static FMC_AirportNode *rotate_left(FMC_AirportNode *x)
{
    FMC_AirportNode *y = x->right;
    FMC_AirportNode *t2 = y->left;
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

static FMC_AirportNode *new_node(const FMC_Airport *airport)
{
    FMC_AirportNode *node = (FMC_AirportNode *)malloc(sizeof(FMC_AirportNode));
    if (node == NULL)
    {
        return NULL;
    }

    node->airport = *airport;
    node->height = 1;
    node->left = NULL;
    node->right = NULL;
    return node;
}

static FMC_AirportNode *insert_node(FMC_AirportNode *node, const FMC_Airport *airport, int *inserted)
{
    if (node == NULL)
    {
        FMC_AirportNode *created = new_node(airport);
        if (created != NULL && inserted != NULL)
        {
            *inserted = 1;
        }
        return created;
    }

    int cmp = strcmp(airport->ident, node->airport.ident);
    if (cmp < 0)
    {
        node->left = insert_node(node->left, airport, inserted);
    }
    else if (cmp > 0)
    {
        node->right = insert_node(node->right, airport, inserted);
    }
    else
    {
        node->airport = *airport;
        return node;
    }

    update_height(node);
    int balance = node_balance(node);

    if (balance > 1 && strcmp(airport->ident, node->left->airport.ident) < 0)
    {
        return rotate_right(node);
    }
    if (balance < -1 && strcmp(airport->ident, node->right->airport.ident) > 0)
    {
        return rotate_left(node);
    }
    if (balance > 1 && strcmp(airport->ident, node->left->airport.ident) > 0)
    {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }
    if (balance < -1 && strcmp(airport->ident, node->right->airport.ident) < 0)
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

static void add_match(FMC_AirportMatchList *matches, const FMC_Airport *airport)
{
    if (matches == NULL || airport == NULL || matches->count >= FMC_AIRPORT_MAX_MATCHES)
    {
        return;
    }

    for (int i = 0; i < matches->count; ++i)
    {
        if (strcmp(matches->items[i].ident, airport->ident) == 0)
        {
            return;
        }
    }

    matches->items[matches->count++] = *airport;
}

static void collect_prefix(FMC_AirportNode *node, const char *query, FMC_AirportMatchList *matches)
{
    if (node == NULL || matches == NULL || matches->count >= FMC_AIRPORT_MAX_MATCHES)
    {
        return;
    }

    int cmp = strcmp(node->airport.ident, query);
    if (cmp >= 0)
    {
        collect_prefix(node->left, query, matches);
    }

    if (matches->count < FMC_AIRPORT_MAX_MATCHES && starts_with(node->airport.ident, query))
    {
        add_match(matches, &node->airport);
    }

    if (matches->count < FMC_AIRPORT_MAX_MATCHES &&
        (cmp < 0 || starts_with(node->airport.ident, query) || strncmp(node->airport.ident, query, strlen(query)) <= 0))
    {
        collect_prefix(node->right, query, matches);
    }
}

static void collect_contains(FMC_AirportNode *node, const char *query, FMC_AirportMatchList *matches)
{
    if (node == NULL || matches == NULL || matches->count >= FMC_AIRPORT_MAX_MATCHES)
    {
        return;
    }

    collect_contains(node->left, query, matches);
    if (matches->count < FMC_AIRPORT_MAX_MATCHES && contains_text(node->airport.ident, query))
    {
        add_match(matches, &node->airport);
    }
    collect_contains(node->right, query, matches);
}

int fmc_airport_index_load(const char *path)
{
    if (airport_loaded)
    {
        return airport_count;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        return 0;
    }

    char line[160];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char ident_raw[32];
        FMC_Airport airport;
        if (sscanf(line, "%31s %lf %lf", ident_raw, &airport.latitude, &airport.longitude) != 3)
        {
            continue;
        }

        normalize_ident(ident_raw, airport.ident, sizeof(airport.ident));
        if (airport.ident[0] == '\0')
        {
            continue;
        }

        int inserted = 0;
        airport_root = insert_node(airport_root, &airport, &inserted);
        if (inserted)
        {
            airport_count++;
        }
    }

    fclose(file);
    airport_loaded = 1;
    return airport_count;
}

int fmc_airport_index_is_ready(void)
{
    return airport_loaded && airport_root != NULL;
}

int fmc_airport_index_count(void)
{
    return airport_count;
}

int fmc_airport_find_exact(const char *ident, FMC_Airport *airport)
{
    char normalized[FMC_AIRPORT_IDENT_LEN];
    normalize_ident(ident, normalized, sizeof(normalized));

    FMC_AirportNode *node = airport_root;
    while (node != NULL)
    {
        int cmp = strcmp(normalized, node->airport.ident);
        if (cmp == 0)
        {
            if (airport != NULL)
            {
                *airport = node->airport;
            }
            return 1;
        }
        node = cmp < 0 ? node->left : node->right;
    }

    return 0;
}

int fmc_airport_search(const char *query, FMC_AirportMatchList *matches)
{
    char normalized[FMC_AIRPORT_IDENT_LEN];
    if (matches == NULL)
    {
        return 0;
    }

    matches->count = 0;
    normalize_ident(query, normalized, sizeof(normalized));
    if (normalized[0] == '\0' || airport_root == NULL)
    {
        return 0;
    }

    FMC_Airport exact;
    if (fmc_airport_find_exact(normalized, &exact))
    {
        add_match(matches, &exact);
    }

    collect_prefix(airport_root, normalized, matches);
    if (matches->count < FMC_AIRPORT_MAX_MATCHES)
    {
        collect_contains(airport_root, normalized, matches);
    }

    return matches->count;
}
