#define SDL_MAIN_HANDLED

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fmc_connect.h"

static int mock_probe_succeeds;
static int mock_probe_calls;
static int mock_send_calls;
static int mock_send_fail_at;

XPCSocket aopenUDP(const char *xp_ip, unsigned short xp_port, unsigned short port)
{
    XPCSocket socket_value;
    memset(&socket_value, 0, sizeof(socket_value));
    snprintf(socket_value.xpIP, sizeof(socket_value.xpIP), "%s", xp_ip != NULL ? xp_ip : "");
    socket_value.xpPort = xp_port;
    socket_value.port = port;
    return socket_value;
}

void closeUDP(XPCSocket socket_value)
{
    (void)socket_value;
}

int getDREFs(XPCSocket socket_value, const char *drefs[], float *values[], unsigned char count, int sizes[])
{
    (void)socket_value;
    (void)drefs;
    (void)count;
    mock_probe_calls++;
    if (!mock_probe_succeeds)
    {
        sizes[0] = 0;
        return -1;
    }
    sizes[0] = 1;
    values[0][0] = 1.0f;
    return 0;
}

int sendTEXT(XPCSocket socket_value, char *message, int x, int y)
{
    (void)socket_value;
    (void)message;
    (void)x;
    (void)y;
    return 0;
}

int sendCOMM(XPCSocket socket_value, const char *command)
{
    (void)socket_value;
    (void)command;
    mock_send_calls++;
    return mock_send_fail_at > 0 && mock_send_calls >= mock_send_fail_at ? -1 : 0;
}

void SDLCALL SDL_Delay(Uint32 milliseconds)
{
    (void)milliseconds;
}

static void reset_mock(void)
{
    mock_probe_succeeds = 1;
    mock_probe_calls = 0;
    mock_send_calls = 0;
    mock_send_fail_at = 0;
}

int main(void)
{
    reset_mock();
    assert(fmc_xplane_connect_init(NULL, 0));
    assert(fmc_xplane_connect_is_connected());
    mock_probe_calls = 0;
    assert(fmc_xplane_confirm_sync_ready());
    assert(mock_probe_calls == 1);
    assert(fmc_xplane_set_origin("ZUUU") == 0);
    assert(mock_probe_calls == 1);
    assert(mock_send_calls == 7);

    fmc_xplane_connect_shutdown();
    reset_mock();
    assert(fmc_xplane_connect_init(NULL, 0));
    mock_probe_calls = 0;
    assert(fmc_xplane_confirm_sync_ready());
    mock_send_fail_at = 3;
    assert(fmc_xplane_set_origin("ZUUU") < 0);
    assert(mock_send_calls == 3);
    assert(!fmc_xplane_connect_is_connected());
    assert(fmc_xplane_set_destination("ZUCK") < 0);
    assert(mock_send_calls == 3);
    assert(mock_probe_calls == 1);

    fmc_xplane_connect_shutdown();
    reset_mock();
    mock_probe_succeeds = 0;
    assert(fmc_xplane_connect_init(NULL, 0));
    assert(!fmc_xplane_connect_is_connected());
    mock_probe_calls = 0;
    assert(!fmc_xplane_confirm_sync_ready());
    assert(mock_probe_calls == 0);
    assert(fmc_xplane_set_origin("ZUUU") < 0);
    assert(mock_probe_calls == 0);
    assert(mock_send_calls == 0);

    fmc_xplane_connect_shutdown();
    puts("FMC connection tests passed.");
    return 0;
}
