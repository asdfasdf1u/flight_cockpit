#ifndef FMC_AIRPORT_H
#define FMC_AIRPORT_H

#define FMC_AIRPORT_IDENT_LEN 8
#define FMC_AIRPORT_MAX_MATCHES 6

typedef struct FMC_Airport
{
    char ident[FMC_AIRPORT_IDENT_LEN];
    double latitude;
    double longitude;
} FMC_Airport;

typedef struct FMC_AirportMatchList
{
    FMC_Airport items[FMC_AIRPORT_MAX_MATCHES];
    int count;
} FMC_AirportMatchList;

int fmc_airport_index_load(const char *path);
int fmc_airport_index_is_ready(void);
int fmc_airport_index_count(void);
int fmc_airport_find_exact(const char *ident, FMC_Airport *airport);
int fmc_airport_search(const char *query, FMC_AirportMatchList *matches);

#endif
