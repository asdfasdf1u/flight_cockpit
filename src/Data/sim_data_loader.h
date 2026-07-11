#ifndef SIM_DATA_LOADER_H
#define SIM_DATA_LOADER_H

#define SIM_MAX_PFD_SAMPLES 12000
#define SIM_MAX_ND_FRAMES 4096
#define SIM_MAX_EICAS_FRAMES 4096
#define SIM_SAMPLE_INTERVAL_SEC (1.0f / 30.0f)

#define SIM_ND_FIELD_TIME (1u << 0)
#define SIM_ND_FIELD_LATITUDE (1u << 1)
#define SIM_ND_FIELD_LONGITUDE (1u << 2)
#define SIM_ND_FIELD_HEADING (1u << 3)
#define SIM_ND_FIELD_TRACK (1u << 4)
#define SIM_ND_FIELD_GROUND_SPEED (1u << 5)
#define SIM_ND_FIELD_TRUE_AIR_SPEED (1u << 6)

typedef struct SimPfdSample
{
    float airspeed;
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
} SimPfdSample;

typedef struct SimNdFrame
{
    float time_sec;
    double latitude;
    double longitude;
    float heading;
    float track;
    float ground_speed;
    float true_air_speed;
    unsigned int fields;
} SimNdFrame;

typedef struct SimEicasUpperFrame
{
    float total_air_temperature;
    float n1_left;
    float n1_right;
    float egt_left;
    float egt_right;
    float fuel_flow_left_display;
    float fuel_flow_right_display;
    float fuel_center_quantity;
    float fuel_left_quantity;
    float fuel_right_quantity;
} SimEicasUpperFrame;

typedef struct SimEicasLowerFrame
{
    float n2_left;
    float n2_right;
    float fuel_flow_left_display;
    float fuel_flow_right_display;
    float oil_pressure_left;
    float oil_pressure_right;
    float oil_temperature_left;
    float oil_temperature_right;
    float oil_quantity_left;
    float oil_quantity_right;
    float vibration_left;
    float vibration_right;
} SimEicasLowerFrame;

typedef struct SimDataStore
{
    SimPfdSample pfd_samples[SIM_MAX_PFD_SAMPLES];
    int pfd_count;

    SimNdFrame nd_frames[SIM_MAX_ND_FRAMES];
    int nd_count;
    int nd_has_time;

    SimEicasUpperFrame eicas_upper_frames[SIM_MAX_EICAS_FRAMES];
    int eicas_upper_count;

    SimEicasLowerFrame eicas_lower_frames[SIM_MAX_EICAS_FRAMES];
    int eicas_lower_count;
} SimDataStore;

void sim_data_store_init(SimDataStore *store);
int sim_data_loader_load_all(SimDataStore *store);
int sim_data_loader_load_pfd(SimDataStore *store, const char *path);
int sim_data_loader_load_nd(SimDataStore *store, const char *path);
int sim_data_loader_load_eicas_upper(SimDataStore *store, const char *path);
int sim_data_loader_load_eicas_lower(SimDataStore *store, const char *path);

#endif
