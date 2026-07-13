#ifndef CABIN_MAIN_H
#define CABIN_MAIN_H

struct SimDataCenter;

int cabin_main_run(void);
int cabin_main_run_with_sim_data_center(const struct SimDataCenter *sim_data_center);

#endif
