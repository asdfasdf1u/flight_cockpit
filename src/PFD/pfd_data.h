#ifndef PFD_DATA_H
#define PFD_DATA_H

typedef struct PFDData
{
    float pitch;            // 俯仰角
    float roll;             // 滚转角
    float yaw;              // 偏航角
    float altitude;         // 当前高度
    float agl_altitude;     // 离地高度
    float throttle;         // 油门百分比
    float airspeed_current; // 当前指示空速
    float airspeed_target;  // 目标空速
    float vertical_speed;   // 垂直速度
    float heading;          // 当前航向
    float heading_target;   // 目标航向
    float altitude_target;  // 目标高度

    int autopilot_on;             // 自动驾驶状态
    char flight_mode[32];         // 飞行模式显示文本
    float simulation_time;        // 本地模拟时间
    int using_file_data;          // 是否使用本地样本数据
    int file_sample_index;        // 当前样本序号
    float file_sample_accumulator; // 样本播放计时器
    int snapshot_frame_id;        // 统一快照帧号
    int data_valid;               // 数据有效标志
} PFDData;

typedef PFDData PFD_Data;

void pfd_data_init(PFD_Data *data);                         // 初始化 PFD 数据
void pfd_data_update_mock(PFD_Data *data, float delta_time); // 更新本地样本或模拟数据
int getPFDData(PFDData *data);                              // 兼容旧接口获取 PFD 数据

#endif
