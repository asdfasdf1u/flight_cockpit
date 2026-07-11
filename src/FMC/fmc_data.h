#ifndef FMC_DATA_H
#define FMC_DATA_H

#include "fmc_airport.h"
#include "fmc_waypoint.h"
#include "../Data/sim_route.h"

#define FMC_MAX_ROUTE_POINTS 64
#define FMC_TEXT_LEN 64
#define FMC_RTE_PAGE_SIZE 5

typedef enum FMC_RTE_InputMode
{
    FMC_RTE_INPUT_AIRPORT,
    FMC_RTE_INPUT_WAYPOINT
} FMC_RTE_InputMode;

typedef enum FMC_Page
{
    FMC_PAGE_HOME,
    FMC_PAGE_INDEX = FMC_PAGE_HOME,
    FMC_PAGE_ROUTE,
    FMC_PAGE_DEP_ARR,
    FMC_PAGE_PERF,
    FMC_PAGE_CLIMB,
    FMC_PAGE_CRUISE,
    FMC_PAGE_DESCENT,
    FMC_PAGE_LEGS,
    FMC_PAGE_HOLD,
    FMC_PAGE_STATUS,
    FMC_PAGE_COUNT
} FMC_Page;

typedef enum FMC_FlightPhase
{
    FMC_PHASE_PREFLIGHT,
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

typedef struct FMC_RoutePoint
{
    char ident[FMC_TEXT_LEN];
    char type[FMC_TEXT_LEN];
    double latitude;
    double longitude;
    double altitude;
    char speed_restriction[FMC_TEXT_LEN];
    char altitude_restriction[FMC_TEXT_LEN];
} FMC_RoutePoint;

typedef struct FMC_Data
{
    FMC_Page current_page;
    FMC_Page previous_page;
    int page_switch_count;
    FMC_FlightPhase active_phase;

    char origin[FMC_TEXT_LEN];
    char destination[FMC_TEXT_LEN];
    char company_route[FMC_TEXT_LEN];
    char route_via[FMC_TEXT_LEN];
    char flight_no[FMC_TEXT_LEN];

    char route_points[FMC_MAX_ROUTE_POINTS][FMC_TEXT_LEN];
    FMC_RoutePoint route_point_data[FMC_MAX_ROUTE_POINTS];
    int route_count;
    int active_leg_index;
    FMC_RoutePoint *configured_route;
    int configured_route_count;
    int configured_route_capacity;
    int configured_route_page;
    int route_loaded_from_file;
    char fms_plan_path[FMC_TEXT_LEN];

    int cruise_altitude;
    int cruise_altitude_from_file;
    int target_speed;
    float cost_index;
    int climb_speed;
    int climb_accel_altitude;
    int climb_thrust_limit;
    char climb_target_speed_text[FMC_TEXT_LEN];
    char climb_spd_alt_limit_text[FMC_TEXT_LEN];
    char climb_transition_alt_text[FMC_TEXT_LEN];
    int cruise_speed;
    int descent_speed;
    int descent_vertical_speed;
    int descent_transition_level;

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
    char airport_query[FMC_TEXT_LEN];
    FMC_AirportMatchList airport_matches;
    int selected_airport_index;
    FMC_Airport selected_origin_airport;
    int has_selected_origin_airport;
    int airport_index_count;
    char xpc_status[FMC_TEXT_LEN];

    FMC_RTE_InputMode route_input_mode;
    char waypoint_query[FMC_TEXT_LEN];
    FMC_WaypointMatchList waypoint_matches;
    int selected_waypoint_index;
    FMC_Waypoint selected_waypoint;
    int has_selected_waypoint;
    int waypoint_index_count;
} FMC_Data;

void fmc_data_init(FMC_Data *data);
void fmc_data_update_mock(FMC_Data *data, float delta_time);
void fmc_data_set_page(FMC_Data *data, FMC_Page page);
void fmc_data_append_char(FMC_Data *data, char c);
void fmc_data_backspace(FMC_Data *data);
void fmc_data_clear_scratchpad(FMC_Data *data);
void fmc_data_destroy(FMC_Data *data);
int fmc_data_load_fms_plan(FMC_Data *data, const char *path);
int fmc_data_commit_scratchpad_to_origin(FMC_Data *data);
void fmc_data_query_airports(FMC_Data *data);
int fmc_data_select_airport_candidate(FMC_Data *data, int index);
int fmc_data_confirm_selected_airport(FMC_Data *data);
void fmc_data_set_route_input_mode(FMC_Data *data, FMC_RTE_InputMode mode);
void fmc_data_query_waypoints(FMC_Data *data);
int fmc_data_select_waypoint_candidate(FMC_Data *data, int index);
int fmc_data_add_selected_waypoint(FMC_Data *data);
int fmc_data_route_next_page(FMC_Data *data);
int fmc_data_route_prev_page(FMC_Data *data);
int fmc_data_route_page_count(const FMC_Data *data);
int fmc_data_set_route_field(FMC_Data *data, FMC_RouteField field);
int fmc_data_exec_route_selection(FMC_Data *data);
int fmc_data_set_phase_parameter(FMC_Data *data, int line_select_index);
int fmc_data_activate_current_phase(FMC_Data *data);
int fmc_data_set_dep_arr_parameter(FMC_Data *data, int arrival_side, int field_index);
int fmc_data_set_legs_parameter(FMC_Data *data, int field_index);
int fmc_data_set_hold_parameter(FMC_Data *data, int field_index);
int fmc_data_export_planned_route(const FMC_Data *data, SimPlannedRoute *route);
const char *fmc_data_phase_name(FMC_FlightPhase phase);
const char *fmc_data_page_name(FMC_Page page);

#endif
