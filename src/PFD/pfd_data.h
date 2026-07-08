#ifndef PFD_DATA_H
#define PFD_DATA_H

typedef struct PFDData
{
    float pitch;
    float roll;
    float yaw;
    float altitude;
    float agl_altitude;
    float throttle;
    float airspeed_current;
    float airspeed_target;
    float vertical_speed;
    float heading;
    float heading_target;
    float altitude_target;

    int autopilot_on;
    char flight_mode[32];
    float simulation_time;
    int using_file_data;
    int file_sample_index;
    float file_sample_accumulator;
} PFDData;

typedef PFDData PFD_Data;

void pfd_data_init(PFD_Data *data);
void pfd_data_update_mock(PFD_Data *data, float delta_time);
int getPFDData(PFDData *data);

#endif
