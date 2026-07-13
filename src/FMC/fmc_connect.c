//完成设置后将信息同步给xplane
#include "fmc_connect.h"
#include <stdio.h>
// 补充缺失的头文件：strlen()函数声明在此处
#include <string.h>

XPCSocket sock;

// 1. 功能按键指令（单独用const char*声明，每个按键对应唯一只读变量）
// 注：参考数据中无RTE、MENU对应项，保留变量名但更新命令格式；其余项严格匹配参考数据
const char *FMC1_KEY_INIT = "sim/FMS/init";   // 初始化按键
const char *FMC1_KEY_RTE = "sim/FMS/fpln";    // 航路按键（参考数据中FPLN对应航路/航段相关）
const char *FMC1_KEY_LEG = "sim/FMS/legs";    // 航段按键
const char *FMC1_KEY_CLB = "sim/FMS/clb";     // 爬升按键
const char *FMC1_KEY_CRZ = "sim/FMS/crz";     // 巡航按键
const char *FMC1_KEY_DES = "sim/FMS/des";     // 下降按键
const char *FMC1_KEY_EXEC = "sim/FMS/exec";   // 执行按键（核心确认按键）
const char *FMC1_KEY_CLEAR = "sim/FMS/clear"; // 清除按键
const char *FMC1_KEY_PROG = "sim/FMS/prog";   // 程序按键
const char *FMC1_KEY_MENU = "sim/FMS/index";  // 菜单按键（参考数据中INDEX对应索引/菜单功能）

// 2. 屏幕左侧按键（LSK L1~L6，单独const char*声明）
const char *FMC1_KEY_LSK_L1 = "sim/FMS/ls_1l"; // 左侧第1行选择键
const char *FMC1_KEY_LSK_L2 = "sim/FMS/ls_2l"; // 左侧第2行选择键
const char *FMC1_KEY_LSK_L3 = "sim/FMS/ls_3l"; // 左侧第3行选择键
const char *FMC1_KEY_LSK_L4 = "sim/FMS/ls_4l"; // 左侧第4行选择键
const char *FMC1_KEY_LSK_L5 = "sim/FMS/ls_5l"; // 左侧第5行选择键
const char *FMC1_KEY_LSK_L6 = "sim/FMS/ls_6l"; // 左侧第6行选择键

// 3. 屏幕右侧按键（LSK R1~R6，单独const char*声明）
const char *FMC1_KEY_LSK_R1 = "sim/FMS/ls_1r"; // 右侧第1行选择键
const char *FMC1_KEY_LSK_R2 = "sim/FMS/ls_2r"; // 右侧第2行选择键
const char *FMC1_KEY_LSK_R3 = "sim/FMS/ls_3r"; // 右侧第3行选择键
const char *FMC1_KEY_LSK_R4 = "sim/FMS/ls_4r"; // 右侧第4行选择键
const char *FMC1_KEY_LSK_R5 = "sim/FMS/ls_5r"; // 右侧第5行选择键
const char *FMC1_KEY_LSK_R6 = "sim/FMS/ls_6r"; // 右侧第6行选择键

// 4. 字母按键指令（const数组存储A~Z，统一格式，便于批量遍历）
const char *FMC1_ALPHA_KEYS[] = {
    "sim/FMS/key_A", "sim/FMS/key_B", "sim/FMS/key_C", "sim/FMS/key_D", "sim/FMS/key_E",
    "sim/FMS/key_F", "sim/FMS/key_G", "sim/FMS/key_H", "sim/FMS/key_I", "sim/FMS/key_J",
    "sim/FMS/key_K", "sim/FMS/key_L", "sim/FMS/key_M", "sim/FMS/key_N", "sim/FMS/key_O",
    "sim/FMS/key_P", "sim/FMS/key_Q", "sim/FMS/key_R", "sim/FMS/key_S", "sim/FMS/key_T",
    "sim/FMS/key_U", "sim/FMS/key_V", "sim/FMS/key_W", "sim/FMS/key_X", "sim/FMS/key_Y",
    "sim/FMS/key_Z"};

// 5. 数字/符号按键指令（const数组存储，包含0~9和常用格式符号）
const char *FMC1_NUM_SYM_KEYS[] = {
    "sim/FMS/key_0", "sim/FMS/key_1", "sim/FMS/key_2", "sim/FMS/key_3", "sim/FMS/key_4",
    "sim/FMS/key_5", "sim/FMS/key_6", "sim/FMS/key_7", "sim/FMS/key_8", "sim/FMS/key_9",
    "sim/FMS/key_period", // 小数点 .（匹配参考数据key_period）
    "sim/FMS/key_minus",  // 负号 -（匹配参考数据key_minus）
    "sim/FMS/key_slash"   // 斜杠 /（用于航路、日期格式，匹配参考数据key_slash）
};

// 根据传入char字符获取字母或者数字的const char*（返回值保持const char*）
const char *get_char_key(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return FMC1_ALPHA_KEYS[c - 'A'];
    }
    else if (c >= '0' && c <= '9')
    {
        return FMC1_NUM_SYM_KEYS[c - '0'];
    }
    else
    {
        return NULL;
    }
}

void initXpc(XPCSocket xpc)
{
    sock = xpc;
}
// 通用实现：FMC1 按键序列+字符输入 公共函数（static限定仅本文件可见）
static int fmc1_send_seq_with_input(char *input_str, const char *confirm_key, const char *func_name)
{
    int flag = 0;
    // 步骤1：点击RTE → CLEAR
    flag = sendCOMM(sock, FMC1_KEY_RTE);
    if (flag > -1)
        flag = sendCOMM(sock, FMC1_KEY_CLEAR);

    // 校验：输入空指针 或 前序指令失败，直接返回错误
    if (input_str == NULL || flag <= -1)
    {
        printf("%s failed: invalid input or previous command error\n", func_name);
        return -1;
    }

    // 步骤2：遍历输入字符，逐个发送按键指令
    int len = strlen(input_str);
    for (size_t i = 0; i < len && flag > -1; i++)
    {
        const char *cmd = get_char_key(input_str[i]);
        if (cmd == NULL) // 无效字符判空
        {
            printf("%s warning: invalid character '%c' at index %zu\n", func_name, input_str[i], i);
            flag = -1;
            break;
        }
        flag = sendCOMM(sock, cmd);
    }

    // 步骤3：发送确认按键
    if (flag > -1)
        flag = sendCOMM(sock, confirm_key);

    // 执行结果打印
    if (flag > -1)
        printf("%s success\n", func_name);
    else
        printf("%s failed\n", func_name);

    return flag;
}

// 设置起飞地：RTE→CLEAR→输入origin→LSK L1确认
void setOrigin(char *origin)
{
    fmc1_send_seq_with_input(origin, FMC1_KEY_LSK_L1, "setOrigin");
}

// 设置目的地：RTE→CLEAR→输入destination→LSK R1确认
void setDestination(char *destination)
{
    fmc1_send_seq_with_input(destination, FMC1_KEY_LSK_R1, "setDestination");
}

// 设置航班号：RTE→CLEAR→输入flt_no→LSK R1确认（修正原复制粘贴错误）
void setFlt_no(char *flt_no)
{
    fmc1_send_seq_with_input(flt_no, FMC1_KEY_LSK_R3, "setFlt_no");
}
void setExec()
{
    int flag = sendCOMM(sock, FMC1_KEY_EXEC);
    if (flag > -1)
        printf("success\n");
    else
        printf("failed\n");
}