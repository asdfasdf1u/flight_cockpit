#ifndef FMC_DATA_H
#define FMC_DATA_H

#define FMC_MAX_ROUTE_POINTS 12
#define FMC_TEXT_LEN 64

typedef enum FMC_Page
{
    FMC_PAGE_INDEX,
    FMC_PAGE_ROUTE,
    FMC_PAGE_DEP_ARR,
    FMC_PAGE_PERF,
    FMC_PAGE_LEGS
} FMC_Page;

typedef struct FMC_Data
{
    FMC_Page current_page;

    char origin[FMC_TEXT_LEN];
    char destination[FMC_TEXT_LEN];
    char flight_no[FMC_TEXT_LEN];

    char route_points[FMC_MAX_ROUTE_POINTS][FMC_TEXT_LEN];
    int route_count;

    int cruise_altitude;
    int target_speed;
    float cost_index;

    char departure_runway[FMC_TEXT_LEN];
    char arrival_runway[FMC_TEXT_LEN];

    char scratchpad[FMC_TEXT_LEN];
    int scratchpad_len;
} FMC_Data;

void fmc_data_init(FMC_Data *data);
void fmc_data_update_mock(FMC_Data *data, float delta_time);
void fmc_data_set_page(FMC_Data *data, FMC_Page page);
void fmc_data_append_char(FMC_Data *data, char c);
void fmc_data_backspace(FMC_Data *data);
void fmc_data_clear_scratchpad(FMC_Data *data);

#endif
