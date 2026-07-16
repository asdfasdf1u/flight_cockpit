#ifndef FMC_CONNECT_H
#define FMC_CONNECT_H

#include "../Util/SDL_Util.h"
#include "../Util/xplaneConnect.h"

int fmc_xplane_connect_init(const char *xp_ip, unsigned short xp_port);
void fmc_xplane_connect_shutdown(void);
int fmc_xplane_connect_is_ready(void);
int fmc_xplane_connect_is_connected(void);
int fmc_xplane_probe_connection(void);
int fmc_xplane_confirm_sync_ready(void);
int fmc_xplane_send_command(const char *command);
int fmc_xplane_send_input_char(char c);
int fmc_xplane_set_origin(const char *origin);
int fmc_xplane_set_destination(const char *destination);
int fmc_xplane_set_co_route(const char *co_route);
int fmc_xplane_set_flt_no(const char *flt_no);
int fmc_xplane_set_exec(void);

//初始化xpc
void initXpc(XPCSocket xpc);
// Legacy wrappers keep old FMC callers working; detailed errors are logged by fmc_xplane_*.
void setOrigin(const char *origin);
void setDestination(const char *destination);
// 设置航班号
void setFlt_no(const char *flt_no);
void setExec(void);

#endif
