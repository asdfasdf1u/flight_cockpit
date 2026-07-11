#ifndef FMC_DATA_H
#define FMC_DATA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_ICAO_CODE_LEN 8     // 机场ICAO代码最大长度
#define MAX_WAYPOINT_CODE_LEN 8 // 航路点代码最大长度
#define MAX_LINE_BUF_LEN 256    // 读取文件行缓冲区长度
#define MAX_AIRPORT_NUM 14000   // 最大机场数量（仅用于读取限制，非数组容量）
#define MAX_WAYPOINT_NUM 200000 // 最大航路点数量（仅用于读取限制，非数组容量）

// 机场结构体
typedef struct
{
    char icao_code[MAX_ICAO_CODE_LEN]; // 机场ICAO代码
    double datum_lat;                  // 基准纬度
    double datum_lon;                  // 基准经度
} Airport;
// 航路点结构体（符合ARINC 424标准）
typedef struct
{
    double lat;                                // 纬度（如33.492513889）
    double lon;                                // 经度（如9.217400000）
    char wp_code[MAX_WAYPOINT_CODE_LEN];       // 航路点代码（如07EBA）
    char icao_airport_code[MAX_ICAO_CODE_LEN]; // 所属机场（如ENRT）
    char route_procedure[20];                  // 航路 / 程序	（如DT）
    long record_index;                         // 记录编号/索引（如2118994）
} Waypoint;
// 机场AVL树节点（关键字：机场ICAO代码）
typedef struct AirportAVLNode {
    Airport data;                      // 机场数据
    struct AirportAVLNode *left;       // 左子树
    struct AirportAVLNode *right;      // 右子树
    int height;                        // 节点高度
} AirportAVLNode;
// 航路点AVL树节点（关键字：航路点代码）
typedef struct WaypointAVLNode {
    Waypoint data;                     // 航路点数据
    struct WaypointAVLNode *left;      // 左子树
    struct WaypointAVLNode *right;     // 右子树
    int height;                        // 节点高度
} WaypointAVLNode;
//航路 包括航路点名和航段名
typedef struct fmc_data
{
    char VIA[10];
    char TO[10];
} VIATO;

// 空速结构体
typedef struct
{
    int speed1; // 低空 公里数
    int speed2; // 高空 马赫数
} TgtSpeed;

// 速度高度约束结构体
typedef struct
{
    int spd_limit; // 速度限制
    int alt_limit; // 高度限制
} SpdAltLimit;


// 元素类型枚举AAA
typedef enum
{
    TYPE_RUNWAY,       // 跑道
    TYPE_TAKEOFF_PROC, // 离场/进场程序
    TYPE_WAYPOINT      // 过渡点
} ElementType;

// 基础元素结构体（跑道/起飞程序/过渡点）
typedef struct
{
    char name[20];    // 元素名称（如RW16C、ATOM E2、COV）
    ElementType type; // 元素类型
    char airport[20]; // 所属机场（KSEA/KBFI）
} AirportElement;

// 关联关系结构体（存储两个元素的关联）
typedef struct
{
    char airport[20];       // 所属机场
    char elem1[20];         // 关联元素1名称
    ElementType elem1_type; // 关联元素1的类型
    char elem2[20];         // 关联元素2名称
    ElementType elem2_type; // 关联元素2的类型

} Relation;
// 选择跑道、起飞程序、过渡点结构体
typedef struct
{
    char select_runway[20];
    char select_proc[20];
    char select_runway_trans[20];
    char select_proc_trans[20];
    int select_flag;
} SelectDepArr;
// AVL树根节点（核心存储）
extern AirportAVLNode *airport_avl_root;     // 机场AVL树根
extern WaypointAVLNode *waypoint_avl_root;   // 航路点AVL树根

// 航线和航路点列表
extern VIATO *via_to_list;
extern int via_to_list_count;
// 目标速度与速度高度限制,分别表示爬升、巡航、降落
extern TgtSpeed tgt_speed1;
extern TgtSpeed tgt_speed2;
extern TgtSpeed tgt_speed3;
// 两个速度限制，爬升和降落共用，巡航不用
extern SpdAltLimit spd_alt_limit1;
extern SpdAltLimit spd_alt_limit2;

// 过渡高度
extern int trans_alt;
extern int crz_alt;
extern int trans_fl;
extern float vpa;

// 元素与关联关系全局数组AAA
extern AirportElement g_elements[200];
extern Relation g_relations[2000];
extern int g_element_count;
extern int g_relation_count;
// 当前展示的跑道、起飞程序和过渡点AAA
extern char **runway;
extern char **proc;
extern char **runway_trans;
extern char **proc_trans;
extern int runway_count;
extern int proc_count;
extern int runway_trans_count;
extern int proc_trans_count;

// 选择的跑道、起飞程序和过渡点
extern SelectDepArr select_dep_arr[3];


// 起飞机场
extern char origin[24];
// 目的地机场
extern char dest[24];
// 公司航路
extern char co_route[24];
// 航班号
extern char flt_no[24];
void initVIATO();
// 核心数据加载与查询函数（AVL树实现）
bool load_airport_data();
bool load_waypoint_data();
Airport *fmc_query_airport_by_icao(const char *icao_code);
Waypoint *fmc_query_waypoint_by_code(const char *wp_code);
// AVL树销毁（内部调用，外部可选使用）
void destroy_airport_avl(AirportAVLNode *root);
void destroy_waypoint_avl(WaypointAVLNode *root);
// 统计信息（可选，用于查看加载数量）
extern int airport_count;          // 实际加载的机场数量
extern int waypoint_count;         // 实际加载的航路点数量
// 速度与限制设置
int setTgtSpeed(char *speed, TgtSpeed *target_speed);
int setSpdAltLimit(const char *spd_alt_str, SpdAltLimit *spd_alt_limit);
// 机场元素与关联关系 AAA
void add_element(char *name, ElementType type, char *airport);
void add_relation(char *airport, char *elem1, ElementType el1_type, char *elem2, ElementType el2_type);
void init_airport_data(void);
void destroy_airport_data(void);
// 查询相关
int query_runway_proc_by_airport(const char *airport);
int query_proc_by_runway(const char *airport, const char *runway);
int query_trans_by_runway(const char *airport, const char *runway);
int query_runway_by_proc(const char *airport, const char *proc);
int query_trans_by_proc(const char *airport, const char *proc);


#endif
