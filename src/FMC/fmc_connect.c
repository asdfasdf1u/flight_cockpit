#include "fmc_connect.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FMC_XPLANE_DEFAULT_IP "127.0.0.1"
#define FMC_XPLANE_DEFAULT_PORT 49009
#define FMC_COMMAND_DELAY_MS 60

int fmc_xplane_probe_connection(void);

static XPCSocket g_sock;
static int g_sock_ready = 0;
static int g_owns_sock = 0;
static int g_plugin_connected = 0;

static void fmc_xplane_log(const char *format, ...)
{
    FILE *fp = fopen("build/fmc_xplane_sync.log", "a");
    if (fp == NULL)
    {
        fp = fopen("fmc_xplane_sync.log", "a");
    }
    if (fp == NULL)
    {
        return;
    }

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fputc('\n', fp);
    fclose(fp);
}

const char *FMC1_KEY_INIT = "sim/FMS/init";
const char *FMC1_KEY_RTE = "sim/FMS/fpln";
const char *FMC1_KEY_LEG = "sim/FMS/legs";
const char *FMC1_KEY_CLB = "sim/FMS/clb";
const char *FMC1_KEY_CRZ = "sim/FMS/crz";
const char *FMC1_KEY_DES = "sim/FMS/des";
const char *FMC1_KEY_EXEC = "sim/FMS/exec";
const char *FMC1_KEY_CLEAR = "sim/FMS/clear";
const char *FMC1_KEY_DELETE = "sim/FMS/delete";
const char *FMC1_KEY_PROG = "sim/FMS/prog";
const char *FMC1_KEY_MENU = "sim/FMS/index";

const char *FMC1_KEY_LSK_L1 = "sim/FMS/ls_1l";
const char *FMC1_KEY_LSK_L2 = "sim/FMS/ls_2l";
const char *FMC1_KEY_LSK_L3 = "sim/FMS/ls_3l";
const char *FMC1_KEY_LSK_L4 = "sim/FMS/ls_4l";
const char *FMC1_KEY_LSK_L5 = "sim/FMS/ls_5l";
const char *FMC1_KEY_LSK_L6 = "sim/FMS/ls_6l";

const char *FMC1_KEY_LSK_R1 = "sim/FMS/ls_1r";
const char *FMC1_KEY_LSK_R2 = "sim/FMS/ls_2r";
const char *FMC1_KEY_LSK_R3 = "sim/FMS/ls_3r";
const char *FMC1_KEY_LSK_R4 = "sim/FMS/ls_4r";
const char *FMC1_KEY_LSK_R5 = "sim/FMS/ls_5r";
const char *FMC1_KEY_LSK_R6 = "sim/FMS/ls_6r";

const char *FMC1_ALPHA_KEYS[] = {
    "sim/FMS/key_A", "sim/FMS/key_B", "sim/FMS/key_C", "sim/FMS/key_D", "sim/FMS/key_E",
    "sim/FMS/key_F", "sim/FMS/key_G", "sim/FMS/key_H", "sim/FMS/key_I", "sim/FMS/key_J",
    "sim/FMS/key_K", "sim/FMS/key_L", "sim/FMS/key_M", "sim/FMS/key_N", "sim/FMS/key_O",
    "sim/FMS/key_P", "sim/FMS/key_Q", "sim/FMS/key_R", "sim/FMS/key_S", "sim/FMS/key_T",
    "sim/FMS/key_U", "sim/FMS/key_V", "sim/FMS/key_W", "sim/FMS/key_X", "sim/FMS/key_Y",
    "sim/FMS/key_Z"};

const char *FMC1_NUM_SYM_KEYS[] = {
    "sim/FMS/key_0", "sim/FMS/key_1", "sim/FMS/key_2", "sim/FMS/key_3", "sim/FMS/key_4",
    "sim/FMS/key_5", "sim/FMS/key_6", "sim/FMS/key_7", "sim/FMS/key_8", "sim/FMS/key_9",
    "sim/FMS/key_period", "sim/FMS/key_minus", "sim/FMS/key_slash"};
const char *FMC1_KEY_SPACE = "sim/FMS/key_space";

static unsigned short parse_port_env(const char *text, unsigned short fallback)
{
    char *end = NULL;
    long value = 0;

    if (text == NULL || text[0] == '\0')
    {
        return fallback;
    }

    value = strtol(text, &end, 10);
    if (end == text || value <= 0 || value > 65535)
    {
        return fallback;
    }

    return (unsigned short)value;
}

static const char *resolve_xplane_ip(const char *xp_ip)
{
    const char *env_ip = getenv("XPLANE_IP");

    if (xp_ip != NULL && xp_ip[0] != '\0')
    {
        return xp_ip;
    }
    if (env_ip != NULL && env_ip[0] != '\0')
    {
        return env_ip;
    }

    return FMC_XPLANE_DEFAULT_IP;
}

static unsigned short resolve_xplane_port(unsigned short xp_port)
{
    if (xp_port != 0)
    {
        return xp_port;
    }
    return parse_port_env(getenv("XPLANE_PORT"), FMC_XPLANE_DEFAULT_PORT);
}

const char *get_char_key(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return FMC1_ALPHA_KEYS[c - 'A'];
    }
    if (c >= 'a' && c <= 'z')
    {
        return FMC1_ALPHA_KEYS[c - 'a'];
    }
    if (c >= '0' && c <= '9')
    {
        return FMC1_NUM_SYM_KEYS[c - '0'];
    }
    if (c == '.')
    {
        return FMC1_NUM_SYM_KEYS[10];
    }
    if (c == '-')
    {
        return FMC1_NUM_SYM_KEYS[11];
    }
    if (c == '/')
    {
        return FMC1_NUM_SYM_KEYS[12];
    }
    if (c == ' ')
    {
        return FMC1_KEY_SPACE;
    }
    return NULL;
}

static int fmc_send_command(const char *command, const char *context)
{
    int result = 0;

    if (command == NULL || command[0] == '\0')
    {
        printf("FMC X-Plane send failed: empty command in %s.\n", context != NULL ? context : "unknown");
        fmc_xplane_log("send failed empty command context=%s", context != NULL ? context : "unknown");
        return -1;
    }
    if (!g_sock_ready)
    {
        printf("FMC X-Plane send skipped: UDP socket is not initialized.\n");
        fmc_xplane_log("send skipped socket_not_ready command=%s context=%s", command, context != NULL ? context : "unknown");
        return -1;
    }
    if (!g_plugin_connected && !fmc_xplane_probe_connection())
    {
        printf("FMC X-Plane send warning: X-Plane Connect is not confirmed; sending command anyway.\n");
        fmc_xplane_log("send warning plugin_not_confirmed command=%s context=%s", command, context != NULL ? context : "unknown");
        fflush(stdout);
    }

    result = sendCOMM(g_sock, command);
    if (result < 0)
    {
        printf("FMC X-Plane send failed in %s: %s.\n", context != NULL ? context : "unknown", command);
        fmc_xplane_log("send failed result=%d command=%s context=%s", result, command, context != NULL ? context : "unknown");
        return result;
    }

    fmc_xplane_log("send ok command=%s context=%s", command, context != NULL ? context : "unknown");
    SDL_Delay(FMC_COMMAND_DELAY_MS);
    return result;
}

int fmc_xplane_connect_init(const char *xp_ip, unsigned short xp_port)
{
    const char *ip = resolve_xplane_ip(xp_ip);
    const unsigned short port = resolve_xplane_port(xp_port);

    if (g_sock_ready)
    {
        return 1;
    }

    g_sock = aopenUDP(ip, port, 0);
    g_sock_ready = 1;
    g_owns_sock = 1;

    printf("FMC X-Plane: UDP command socket ready for %s:%u.\n", ip, port);
    fmc_xplane_log("socket ready ip=%s port=%u", ip, port);
    if (fmc_xplane_probe_connection())
    {
        printf("FMC X-Plane: X-Plane Connect plugin responded.\n");
        fmc_xplane_log("probe ok");
    }
    else
    {
        printf("FMC X-Plane: no X-Plane Connect response yet. Check plugin and port.\n");
        fmc_xplane_log("probe failed initial");
    }
    fflush(stdout);
    return 1;
}

void fmc_xplane_connect_shutdown(void)
{
    if (!g_sock_ready)
    {
        return;
    }

    if (g_owns_sock)
    {
        closeUDP(g_sock);
    }
    g_sock_ready = 0;
    g_owns_sock = 0;
    g_plugin_connected = 0;
}

int fmc_xplane_connect_is_ready(void)
{
    return g_sock_ready;
}

int fmc_xplane_connect_is_connected(void)
{
    return g_plugin_connected;
}

int fmc_xplane_probe_connection(void)
{
    const char *dref = "sim/time/total_running_time_sec";

    if (!g_sock_ready)
    {
        g_plugin_connected = 0;
        return 0;
    }

    for (int attempt = 0; attempt < 5; ++attempt)
    {
        float running_time[1] = {0.0f};
        float *values[1] = {running_time};
        int sizes[1] = {1};

        if (getDREFs(g_sock, &dref, values, 1, sizes) == 0 && sizes[0] > 0)
        {
            if (!g_plugin_connected)
            {
                char message[] = "FMC CONNECTED";
                printf("FMC X-Plane: connected to X-Plane Connect.\n");
                sendTEXT(g_sock, message, 20, 20);
                fmc_xplane_log("probe connected running_time=%.3f", running_time[0]);
                fflush(stdout);
            }
            g_plugin_connected = 1;
            return 1;
        }

        if (attempt < 4)
        {
            SDL_Delay(20);
        }
    }

    if (g_plugin_connected)
    {
        printf("FMC X-Plane: X-Plane Connect response lost.\n");
        fmc_xplane_log("probe lost");
        fflush(stdout);
    }
    g_plugin_connected = 0;
    fmc_xplane_log("probe failed");
    return 0;
}

int fmc_xplane_send_command(const char *command)
{
    return fmc_send_command(command, "button");
}

int fmc_xplane_send_input_char(char c)
{
    const char *command = get_char_key(c);
    if (command == NULL)
    {
        printf("FMC X-Plane send skipped: unsupported input character '%c'.\n", c);
        return -1;
    }

    return fmc_send_command(command, "input");
}

void initXpc(XPCSocket xpc)
{
    if (g_sock_ready && g_owns_sock)
    {
        closeUDP(g_sock);
    }

    g_sock = xpc;
    g_sock_ready = 1;
    g_owns_sock = 0;
    g_plugin_connected = 0;
    fmc_xplane_probe_connection();
}

static int fmc1_type_text(const char *input_str, const char *func_name)
{
    const size_t len = strlen(input_str);

    for (size_t i = 0; i < len; i++)
    {
        const char *cmd = get_char_key(input_str[i]);
        if (cmd == NULL)
        {
            printf("%s failed: unsupported character '%c' at index %zu.\n", func_name, input_str[i], i);
            return -1;
        }

        if (fmc_send_command(cmd, func_name) < 0)
        {
            printf("%s failed while sending '%c'.\n", func_name, input_str[i]);
            return -1;
        }
    }

    return 0;
}

static int fmc1_send_seq_with_input(const char *input_str, const char *confirm_key, const char *func_name)
{
    int flag = 0;

    if (input_str == NULL || input_str[0] == '\0')
    {
        printf("%s failed: empty input.\n", func_name);
        return -1;
    }

    flag = fmc_send_command(FMC1_KEY_RTE, func_name);
    if (flag < 0)
    {
        printf("%s failed: route page command error.\n", func_name);
        return -1;
    }

    flag = fmc_send_command(FMC1_KEY_CLEAR, func_name);
    if (flag < 0)
    {
        printf("%s failed: scratchpad clear error.\n", func_name);
        return -1;
    }

    if (fmc1_type_text(input_str, func_name) < 0)
    {
        return -1;
    }

    flag = fmc_send_command(confirm_key, func_name);
    if (flag < 0)
    {
        printf("%s failed.\n", func_name);
        return -1;
    }

    printf("%s success.\n", func_name);
    return 0;
}

int fmc_xplane_set_origin(const char *origin)
{
    return fmc1_send_seq_with_input(origin, FMC1_KEY_LSK_L1, "setOrigin");
}

int fmc_xplane_set_destination(const char *destination)
{
    return fmc1_send_seq_with_input(destination, FMC1_KEY_LSK_R1, "setDestination");
}

int fmc_xplane_set_co_route(const char *co_route)
{
    return fmc1_send_seq_with_input(co_route, FMC1_KEY_LSK_L2, "setCo_route");
}

int fmc_xplane_set_flt_no(const char *flt_no)
{
    return fmc1_send_seq_with_input(flt_no, FMC1_KEY_LSK_R3, "setFlt_no");
}

int fmc_xplane_set_exec(void)
{
    const int flag = fmc_send_command(FMC1_KEY_EXEC, "setExec");
    if (flag < 0)
    {
        printf("setExec failed.\n");
        return -1;
    }

    printf("setExec success.\n");
    return 0;
}

void setOrigin(char *origin)
{
    (void)fmc_xplane_set_origin(origin);
}

void setDestination(char *destination)
{
    (void)fmc_xplane_set_destination(destination);
}

void setFlt_no(char *flt_no)
{
    (void)fmc_xplane_set_flt_no(flt_no);
}

void setExec(void)
{
    (void)fmc_xplane_set_exec();
}
