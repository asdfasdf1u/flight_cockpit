#ifndef COCKPIT_MAIN_H
#define COCKPIT_MAIN_H

struct SimDataCenter;

int cockpit_main_run(void);
int cockpit_main_run_with_sim_data_center(struct SimDataCenter *sim_data_center);

#endif
