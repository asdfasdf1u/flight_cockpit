#ifndef FMC_CONNECT_H
#define FMC_CONNECT_H

#include "../Util/SDL_Util.h"
#include "../Util/xplaneConnect.h"
//初始化xpc
void initXpc(XPCSocket xpc);
//设置起降机场
void setOrigin(char *origin);
void setDestination(char *destination);
// 设置航班号
void setFlt_no(char *flt_no);
void setExec();

#endif