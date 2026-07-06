#ifndef PFD_DATA_H
#define PFD_DATA_H

typedef struct PFD_Data
{
    float airspeed;
    float altitude;
    float vertical_speed;
    float pitch;
    float roll;
    float heading;
    float throttle;
    int autopilot_on;
    char flight_mode[32];
    float simulation_time;
} PFD_Data;

void pfd_data_init(PFD_Data *data);
void pfd_data_update_mock(PFD_Data *data, float delta_time);

#endif
