#ifndef CABIN_MAIN_H
#define CABIN_MAIN_H

struct SimDataCenter;
struct XPlaneSharedRuntime;

int cabin_main_run(void);
int cabin_main_run_with_sim_data_center(struct SimDataCenter *sim_data_center);
int cabin_main_run_with_shared_runtime(struct XPlaneSharedRuntime *runtime);

#endif
