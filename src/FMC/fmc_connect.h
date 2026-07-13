#ifndef FMC_CONNECT_H
#define FMC_CONNECT_H

#include "../Util/SDL_Util.h"
#include "../Util/xplaneConnect.h"

#define FMC_XPLANE_DEFAULT_IP "127.0.0.1"
#define FMC_XPLANE_DEFAULT_PORT 49009

int fmc_xplane_connect_init(const char *xp_ip, unsigned short xp_port);
void fmc_xplane_connect_shutdown(void);
int fmc_xplane_connect_is_ready(void);
int fmc_xplane_connect_is_connected(void);
int fmc_xplane_probe_connection(void);
int fmc_xplane_send_command(const char *command);
int fmc_xplane_send_input_char(char c);

void initXpc(XPCSocket xpc);

int setOrigin(const char *origin);
int setDestination(const char *destination);
int setFlt_no(const char *flt_no);
int setExec(void);

#endif
