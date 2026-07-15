#ifndef FMC_UI_ADAPTER_H
#define FMC_UI_ADAPTER_H

#include "fmc_data.h"
#include "../Data/sim_route.h"

#define FMC_TEXT_LEN 32
#define FMC_MAX_ROUTE_POINTS 64
#define FMC_RTE_PAGE_SIZE 5

typedef enum FMC_Page
{
    FMC_PAGE_HOME = 0,
    FMC_PAGE_INDEX = FMC_PAGE_HOME,
    FMC_PAGE_ROUTE,
    FMC_PAGE_DEP_ARR,
    FMC_PAGE_PERF,
    FMC_PAGE_CLIMB,
    FMC_PAGE_CRUISE,
    FMC_PAGE_DESCENT,
    FMC_PAGE_LEGS,
    FMC_PAGE_HOLD,
    FMC_PAGE_PROG,
    FMC_PAGE_STATUS,
    FMC_PAGE_DIR_INTC,
    FMC_PAGE_FIX,
    FMC_PAGE_NAV_RAD,
    FMC_PAGE_COUNT
} FMC_Page;

typedef enum FMC_FlightPhase
{
    FMC_PHASE_PREFLIGHT = 0,
    FMC_PHASE_CLIMB,
    FMC_PHASE_CRUISE,
    FMC_PHASE_DESCENT
} FMC_FlightPhase;

typedef enum FMC_RouteField
{
    FMC_ROUTE_FIELD_ORIGIN = 1,
    FMC_ROUTE_FIELD_DESTINATION,
    FMC_ROUTE_FIELD_COMPANY_ROUTE,
    FMC_ROUTE_FIELD_FLIGHT_NO,
    FMC_ROUTE_FIELD_VIA,
    FMC_ROUTE_FIELD_TO_FIX
} FMC_RouteField;

typedef enum FMC_CoordinateSource
{
    FMC_COORD_SOURCE_INVALID = 0,
    FMC_COORD_SOURCE_ROUTE,
    FMC_COORD_SOURCE_OVERRIDE,
    FMC_COORD_SOURCE_APT_DAT
} FMC_CoordinateSource;

typedef struct FMC_Data
{
    FMC_Page current_page;
    FMC_Page previous_page;
    FMC_FlightPhase active_phase;
    int page_switch_count;

    char origin[FMC_TEXT_LEN];
    char destination[FMC_TEXT_LEN];
    char company_route[FMC_TEXT_LEN];
    char route_via[FMC_TEXT_LEN];
    char flight_no[FMC_TEXT_LEN];
    char route_points[FMC_MAX_ROUTE_POINTS][FMC_TEXT_LEN];
    double origin_latitude;
    double origin_longitude;
    int origin_has_position;
    FMC_CoordinateSource origin_coordinate_source;
    double route_latitudes[FMC_MAX_ROUTE_POINTS];
    double route_longitudes[FMC_MAX_ROUTE_POINTS];
    int route_has_position[FMC_MAX_ROUTE_POINTS];
    FMC_CoordinateSource route_coordinate_sources[FMC_MAX_ROUTE_POINTS];
    int route_count;
    int configured_route_page;
    int route_loaded_from_file;
    SimRouteSource route_source;
    int active_waypoint_index;
    char fms_plan_path[192];

    int cruise_altitude;
    int target_speed;
    float cost_index;
    char climb_target_speed_text[FMC_TEXT_LEN];
    char climb_spd_alt_limit_text[FMC_TEXT_LEN];
    char climb_transition_alt_text[FMC_TEXT_LEN];
    int cruise_speed;
    int descent_speed;
    int descent_transition_level;
    int descent_vertical_speed;

    char departure_runway[FMC_TEXT_LEN];
    char arrival_runway[FMC_TEXT_LEN];
    char departure_procedure[FMC_TEXT_LEN];
    char arrival_procedure[FMC_TEXT_LEN];
    char departure_transition[FMC_TEXT_LEN];
    char arrival_transition[FMC_TEXT_LEN];
    char legs_sequence[FMC_TEXT_LEN];
    char hold_fix[FMC_TEXT_LEN];
    char hold_inbound_course[FMC_TEXT_LEN];
    char hold_turn_direction[FMC_TEXT_LEN];
    char hold_leg_time[FMC_TEXT_LEN];
    char hold_speed_altitude[FMC_TEXT_LEN];

    char scratchpad[FMC_TEXT_LEN];
    int scratchpad_len;
    char message[FMC_TEXT_LEN];
    int origin_exec_pending;

    /* X-Plane live state */
    double current_latitude;
    double current_longitude;
    float current_altitude_ft;
    float current_ias;
    float current_mach;
    float current_heading;
    float current_ground_speed;
    float current_vertical_speed;
    float current_wind_speed;
    float current_wind_direction;
    float current_fuel_kg;
    int live_data_active;
    int route_mod_pending;
    int route_clear_pending;
    int route_delete_pending;
} FMC_Data;

void fmc_data_init(FMC_Data *data);
void fmc_data_destroy(FMC_Data *data);
void fmc_data_update_mock(FMC_Data *data, float delta_time);
void fmc_data_set_page(FMC_Data *data, FMC_Page page);
void fmc_data_append_char(FMC_Data *data, char c);
void fmc_data_backspace(FMC_Data *data);
void fmc_data_clear_scratchpad(FMC_Data *data);
void fmc_data_show_delete(FMC_Data *data);
int fmc_data_route_page_count(const FMC_Data *data);
int fmc_data_route_next_page(FMC_Data *data);
int fmc_data_route_prev_page(FMC_Data *data);
int fmc_data_set_route_field(FMC_Data *data, FMC_RouteField field);
int fmc_data_set_phase_parameter(FMC_Data *data, int field_index);
int fmc_data_set_dep_arr_parameter(FMC_Data *data, int arrival_side, int field_index);
int fmc_data_handle_dep_arr_lsk(FMC_Data *data, int right_side, int index);
int fmc_data_dep_arr_next_page(FMC_Data *data);
int fmc_data_dep_arr_prev_page(FMC_Data *data);
void fmc_data_dep_arr_back_to_index(FMC_Data *data);
int fmc_data_set_legs_parameter(FMC_Data *data, int field_index);
int fmc_data_set_hold_parameter(FMC_Data *data, int field_index);
int fmc_data_activate_current_phase(FMC_Data *data);
int fmc_data_exec_route_selection(FMC_Data *data);
int fmc_data_sync_route_to_xplane(FMC_Data *data);
int fmc_data_apply_planned_route(FMC_Data *data, const SimPlannedRoute *route);
int fmc_data_export_planned_route(const FMC_Data *data, SimPlannedRoute *route);
int fmc_data_route_has_uncommitted_changes(const FMC_Data *data);
int fmc_data_route_clear_pending(const FMC_Data *data);
void fmc_data_mark_route_committed(FMC_Data *data);

#endif
