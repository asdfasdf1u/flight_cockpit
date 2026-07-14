#ifndef COCKPIT_MAIN_H
#define COCKPIT_MAIN_H

struct SimDataCenter;
struct XPlaneSharedRuntime;

int cockpit_main_run(void);
int cockpit_main_run_with_args(int argc, char *argv[]);
int cockpit_main_run_with_sim_data_center(struct SimDataCenter *sim_data_center);
int cockpit_main_run_with_shared_runtime(struct XPlaneSharedRuntime *runtime);

#endif
