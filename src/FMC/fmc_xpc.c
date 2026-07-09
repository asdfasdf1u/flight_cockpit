#include "fmc_xpc.h"

#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define FMC_XPC_HOST "127.0.0.1"
#define FMC_XPC_PORT 49009

static void set_status(char *status, int status_size, const char *text)
{
    if (status == NULL || status_size <= 0 || text == NULL)
    {
        return;
    }

    snprintf(status, (size_t)status_size, "%s", text);
}

int fmc_xpc_sync_origin_airport(const FMC_Airport *airport, char *status, int status_size)
{
    if (airport == NULL)
    {
        set_status(status, status_size, "XPC SKIP: NO AIRPORT");
        return 0;
    }

    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        set_status(status, status_size, "XPC FAIL: WSA");
        return 0;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        WSACleanup();
        set_status(status, status_size, "XPC FAIL: SOCKET");
        return 0;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(FMC_XPC_PORT);
    addr.sin_addr.s_addr = inet_addr(FMC_XPC_HOST);

    char payload[160];
    snprintf(payload, sizeof(payload),
             "FMC_ORIGIN,%s,%.8f,%.8f",
             airport->ident,
             airport->latitude,
             airport->longitude);

    int sent = sendto(sock,
                      payload,
                      (int)strlen(payload),
                      0,
                      (const struct sockaddr *)&addr,
                      sizeof(addr));

    closesocket(sock);
    WSACleanup();

    if (sent == SOCKET_ERROR)
    {
        set_status(status, status_size, "XPC FAIL: SEND");
        return 0;
    }

    snprintf(status, (size_t)status_size, "XPC SENT %s", airport->ident);
    return 1;
}
