#ifndef XPLANE_CONNECT_H
#define XPLANE_CONNECT_H

typedef void *XPCSocket;

static inline int sendCOMM(XPCSocket socket, const char *command)
{
    (void)socket;
    (void)command;
    return 0;
}

#endif
