#include "fmc_data.h"

// AVL树树根
AirportAVLNode *airport_avl_root = NULL;
WaypointAVLNode *waypoint_avl_root = NULL;

// 统计信息
int airport_count = 0;
int waypoint_count = 0;

//飞行高度相关
int trans_alt = 12000;
int crz_alt = 0;
int trans_fl = 300;
float vpa = 2.5;

// 速度相关
TgtSpeed tgt_speed1 = {290, 74};
TgtSpeed tgt_speed2 = {300, 74};
TgtSpeed tgt_speed3 = {290, 74};
SpdAltLimit spd_alt_limit1 = {250, 10000};
SpdAltLimit spd_alt_limit2 = {0, 0};
// VIATO列表
VIATO *via_to_list = NULL;
int via_to_list_count = 0;
int rte_index = 1;
// 起飞，目标机场，航路设置，航班号
char origin[24] = {0};
char dest[24] = {0};
char co_route[24] = {0};
char flt_no[24] = {0};

// 元素与关联关系AAA
AirportElement g_elements[200];
Relation g_relations[2000];
int g_element_count = 0;
int g_relation_count = 0;
// 展示列表AAA
char **runway = NULL;
char **proc = NULL;
char **runway_trans = NULL;
char **proc_trans = NULL;
int runway_count = 0;
int proc_count = 0;
int runway_trans_count = 0;
int proc_trans_count = 0;

// 选择项
SelectDepArr select_dep_arr[3] = {{0},{0},{0}};
void initVIATO() {
    if (via_to_list != NULL) {
        free(via_to_list);
        via_to_list = NULL;
    }
    via_to_list = (VIATO *)malloc(sizeof(VIATO) * MAX_VIATO_NUM);
    via_to_list_count = 0;
}

/************************* AVL树核心工具函数（私有） *************************/
// 获取节点高度（空节点高度为0）
static int avl_get_height(void *node, int is_airport)
{
    if (node == NULL)
    {
        return 0;
    }
    if (is_airport)
    {
        return ((AirportAVLNode *)node)->height;
    }
    else
    {
        return ((WaypointAVLNode *)node)->height;
    }
}

// 计算平衡因子（左子树高度 - 右子树高度）
static int avl_get_balance_factor(void *node, int is_airport)
{
    if (node == NULL)
    {
        return 0;
    }
    if (is_airport)
    {
        AirportAVLNode *n = (AirportAVLNode *)node;
        return avl_get_height(n->left, 1) - avl_get_height(n->right, 1);
    }
    else
    {
        WaypointAVLNode *n = (WaypointAVLNode *)node;
        return avl_get_height(n->left, 0) - avl_get_height(n->right, 0);
    }
}

// 去除字符串末尾空格
static void trim_trailing_spaces(char *str, int max_len)
{
    if (str == NULL || max_len <= 0)
    {
        return;
    }
    int len = strlen(str);
    while (len > 0 && str[len - 1] == ' ' && len <= max_len)
    {
        str[len - 1] = '\0';
        len--;
    }
}

// 机场AVL树右旋
static AirportAVLNode *airport_avl_right_rotate(AirportAVLNode *y)
{
    AirportAVLNode *x = y->left;
    AirportAVLNode *T2 = x->right;

    // 执行旋转
    x->right = y;
    y->left = T2;

    // 更新高度（先下层后上层）
    y->height = 1 + (avl_get_height(y->left, 1) > avl_get_height(y->right, 1) ? avl_get_height(y->left, 1) : avl_get_height(y->right, 1));
    x->height = 1 + (avl_get_height(x->left, 1) > avl_get_height(x->right, 1) ? avl_get_height(x->left, 1) : avl_get_height(x->right, 1));

    return x;
}

// 机场AVL树左旋
static AirportAVLNode *airport_avl_left_rotate(AirportAVLNode *x)
{
    AirportAVLNode *y = x->right;
    AirportAVLNode *T2 = y->left;

    // 执行旋转
    y->left = x;
    x->right = T2;

    // 更新高度
    x->height = 1 + (avl_get_height(x->left, 1) > avl_get_height(x->right, 1) ? avl_get_height(x->left, 1) : avl_get_height(x->right, 1));
    y->height = 1 + (avl_get_height(y->left, 1) > avl_get_height(y->right, 1) ? avl_get_height(y->left, 1) : avl_get_height(y->right, 1));

    return y;
}

// 航路点AVL树右旋
static WaypointAVLNode *waypoint_avl_right_rotate(WaypointAVLNode *y)
{
    WaypointAVLNode *x = y->left;
    WaypointAVLNode *T2 = x->right;

    // 执行旋转
    x->right = y;
    y->left = T2;

    // 更新高度
    y->height = 1 + (avl_get_height(y->left, 0) > avl_get_height(y->right, 0) ? avl_get_height(y->left, 0) : avl_get_height(y->right, 0));
    x->height = 1 + (avl_get_height(x->left, 0) > avl_get_height(x->right, 0) ? avl_get_height(x->left, 0) : avl_get_height(x->right, 0));

    return x;
}

// 航路点AVL树左旋
static WaypointAVLNode *waypoint_avl_left_rotate(WaypointAVLNode *x)
{
    WaypointAVLNode *y = x->right;
    WaypointAVLNode *T2 = y->left;

    // 执行旋转
    y->left = x;
    x->right = T2;

    // 更新高度
    x->height = 1 + (avl_get_height(x->left, 0) > avl_get_height(x->right, 0) ? avl_get_height(x->left, 0) : avl_get_height(x->right, 0));
    y->height = 1 + (avl_get_height(y->left, 0) > avl_get_height(y->right, 0) ? avl_get_height(y->left, 0) : avl_get_height(y->right, 0));

    return y;
}

// 插入机场数据到AVL树（递归）
static AirportAVLNode *airport_avl_insert(AirportAVLNode *root, const Airport *airport)
{
    // 1. 普通BST插入
    if (root == NULL)
    {
        AirportAVLNode *new_node = (AirportAVLNode *)malloc(sizeof(AirportAVLNode));
        if (new_node == NULL)
        {
            fprintf(stderr, "错误：机场AVL节点内存分配失败\n");
            return NULL;
        }
        memcpy(&new_node->data, airport, sizeof(Airport));
        new_node->left = NULL;
        new_node->right = NULL;
        new_node->height = 1;
        airport_count++; // 统计加载数量
        return new_node;
    }

    // 按ICAO代码排序（字符串比较，先去空格）
    char root_icao_trim[MAX_ICAO_CODE_LEN] = {0};
    char new_icao_trim[MAX_ICAO_CODE_LEN] = {0};
    strncpy(root_icao_trim, root->data.icao_code, MAX_ICAO_CODE_LEN - 1);
    strncpy(new_icao_trim, airport->icao_code, MAX_ICAO_CODE_LEN - 1);
    trim_trailing_spaces(root_icao_trim, MAX_ICAO_CODE_LEN);
    trim_trailing_spaces(new_icao_trim, MAX_ICAO_CODE_LEN);

    int cmp = strcmp(new_icao_trim, root_icao_trim);
    if (cmp < 0)
    {
        root->left = airport_avl_insert(root->left, airport);
    }
    else if (cmp > 0)
    {
        root->right = airport_avl_insert(root->right, airport);
    }
    else
    {
        // 重复ICAO代码，不插入
        return root;
    }

    // 2. 更新当前节点高度
    root->height = 1 + (avl_get_height(root->left, 1) > avl_get_height(root->right, 1) ? avl_get_height(root->left, 1) : avl_get_height(root->right, 1));

    // 3. 计算平衡因子
    int balance = avl_get_balance_factor(root, 1);

    // 4. 处理四种失衡场景
    // 左左失衡
    if (balance > 1 && strcmp(new_icao_trim, root->left->data.icao_code) < 0)
    {
        return airport_avl_right_rotate(root);
    }
    // 右右失衡
    if (balance < -1 && strcmp(new_icao_trim, root->right->data.icao_code) > 0)
    {
        return airport_avl_left_rotate(root);
    }
    // 左右失衡
    if (balance > 1 && strcmp(new_icao_trim, root->left->data.icao_code) > 0)
    {
        root->left = airport_avl_left_rotate(root->left);
        return airport_avl_right_rotate(root);
    }
    // 右左失衡
    if (balance < -1 && strcmp(new_icao_trim, root->right->data.icao_code) < 0)
    {
        root->right = airport_avl_right_rotate(root->right);
        return airport_avl_left_rotate(root);
    }

    return root;
}

// 插入航路点数据到AVL树（递归）
static WaypointAVLNode *waypoint_avl_insert(WaypointAVLNode *root, const Waypoint *waypoint)
{
    // 1. 普通BST插入
    if (root == NULL)
    {
        WaypointAVLNode *new_node = (WaypointAVLNode *)malloc(sizeof(WaypointAVLNode));
        if (new_node == NULL)
        {
            fprintf(stderr, "错误：航路点AVL节点内存分配失败\n");
            return NULL;
        }
        memcpy(&new_node->data, waypoint, sizeof(Waypoint));
        new_node->left = NULL;
        new_node->right = NULL;
        new_node->height = 1;
        waypoint_count++; // 统计加载数量
        return new_node;
    }

    // 按航路点代码排序（先去空格）
    char root_wp_trim[MAX_WAYPOINT_CODE_LEN] = {0};
    char new_wp_trim[MAX_WAYPOINT_CODE_LEN] = {0};
    strncpy(root_wp_trim, root->data.wp_code, MAX_WAYPOINT_CODE_LEN - 1);
    strncpy(new_wp_trim, waypoint->wp_code, MAX_WAYPOINT_CODE_LEN - 1);
    trim_trailing_spaces(root_wp_trim, MAX_WAYPOINT_CODE_LEN);
    trim_trailing_spaces(new_wp_trim, MAX_WAYPOINT_CODE_LEN);

    int cmp = strcmp(new_wp_trim, root_wp_trim);
    if (cmp < 0)
    {
        root->left = waypoint_avl_insert(root->left, waypoint);
    }
    else if (cmp > 0)
    {
        root->right = waypoint_avl_insert(root->right, waypoint);
    }
    else
    {
        // 重复航路点代码，不插入
        return root;
    }

    // 2. 更新当前节点高度
    root->height = 1 + (avl_get_height(root->left, 0) > avl_get_height(root->right, 0) ? avl_get_height(root->left, 0) : avl_get_height(root->right, 0));

    // 3. 计算平衡因子
    int balance = avl_get_balance_factor(root, 0);

    // 4. 处理四种失衡场景
    // 左左失衡
    if (balance > 1 && strcmp(new_wp_trim, root->left->data.wp_code) < 0)
    {
        return waypoint_avl_right_rotate(root);
    }
    // 右右失衡
    if (balance < -1 && strcmp(new_wp_trim, root->right->data.wp_code) > 0)
    {
        return waypoint_avl_left_rotate(root);
    }
    // 左右失衡
    if (balance > 1 && strcmp(new_wp_trim, root->left->data.wp_code) > 0)
    {
        root->left = waypoint_avl_left_rotate(root->left);
        return waypoint_avl_right_rotate(root);
    }
    // 右左失衡
    if (balance < -1 && strcmp(new_wp_trim, root->right->data.wp_code) < 0)
    {
        root->right = waypoint_avl_right_rotate(root->right);
        return waypoint_avl_left_rotate(root);
    }

    return root;
}

/************************* 加载函数（直接插入AVL树，无数组） *************************/
// 加载机场数据（直接插入AVL树）
bool load_airport_data()
{
    // 参数检查
    char *file_path = "assets/apt.dat";
    // 打开文件
    FILE *fp = fopen(file_path, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "错误：无法打开机场文件 %s\n", file_path);
        return false;
    }

    // 先销毁旧的AVL树
    destroy_airport_avl(airport_avl_root);
    airport_avl_root = NULL;
    airport_count = 0;

    // 逐行读取并插入AVL树
    char line_buf[MAX_LINE_BUF_LEN] = {0};
    bool is_end = false;

    while (fgets(line_buf, sizeof(line_buf), fp) != NULL && !is_end)
    {
        // 去除换行符
        size_t line_len = strlen(line_buf);
        while (line_len > 0 && (line_buf[line_len - 1] == '\n' || line_buf[line_len - 1] == '\r'))
        {
            line_buf[--line_len] = '\0';
        }

        // 空行终止
        if (line_len == 0)
        {
            is_end = true;
            continue;
        }

        // 超出最大数量限制
        if (airport_count >= MAX_AIRPORT_NUM)
        {
            fprintf(stderr, "警告：机场数量超出最大限制 %d\n", MAX_AIRPORT_NUM);
            break;
        }

        // 解析机场数据
        Airport temp_airport = {0};
        int parsed = sscanf(line_buf, "%7s %lf %lf",
                            temp_airport.icao_code,
                            &temp_airport.datum_lat,
                            &temp_airport.datum_lon);

        // 确保字符串结尾
        temp_airport.icao_code[MAX_ICAO_CODE_LEN - 1] = '\0';

        // 解析失败则跳过
        if (parsed != 3)
        {
            fprintf(stderr, "警告：机场行解析失败：%s\n", line_buf);
            continue;
        }

        // 直接插入AVL树
        airport_avl_root = airport_avl_insert(airport_avl_root, &temp_airport);
        if (airport_avl_root == NULL && airport_count == 0)
        {
            fprintf(stderr, "错误：机场AVL树插入失败\n");
            fclose(fp);
            return false;
        }
    }

    fclose(fp);

    // 无有效数据
    if (airport_count == 0)
    {
        fprintf(stderr, "错误：未加载到有效机场数据\n");
        return false;
    }

    printf("成功加载 %d 个机场到AVL树\n", airport_count);
    return true;
}

// 加载航路点数据（直接插入AVL树）
bool load_waypoint_data()
{
    char *file_path = "assets/earth_fix.dat";
    // 打开文件
    FILE *fp = fopen(file_path, "r");
    if (!fp)
    {
        perror("Failed to open waypoint file");
        return false;
    }

    // 先销毁旧的AVL树
    destroy_waypoint_avl(waypoint_avl_root);
    waypoint_avl_root = NULL;
    waypoint_count = 0;

    char line_buf[MAX_LINE_BUF_LEN];
    int line_num = 0;
    bool data_started = false;

    // 逐行读取并插入AVL树
    while (fgets(line_buf, sizeof(line_buf), fp) != NULL)
    {
        line_num++;
        // 去除换行符
        line_buf[strcspn(line_buf, "\n\r")] = '\0';

        // 跳过头部非数据行
        if (!data_started)
        {
            if (strlen(line_buf) > 0 && (line_buf[0] == '-' || (line_buf[0] >= '0' && line_buf[0] <= '9')))
            {
                data_started = true;
            }
            else
            {
                continue;
            }
        }

        // 空行终止
        if (strlen(line_buf) == 0)
        {
            break;
        }

        // 超出最大数量限制
        if (waypoint_count >= MAX_WAYPOINT_NUM)
        {
            fprintf(stderr, "Waypoint array reach max size(%d)\n", MAX_WAYPOINT_NUM);
            break;
        }

        // 解析航路点数据
        Waypoint temp_wp = {0};
        int ret = sscanf(line_buf, "%lf %lf %7s %7s %3s %ld",
                         &temp_wp.lat, &temp_wp.lon, temp_wp.wp_code,
                         temp_wp.icao_airport_code, temp_wp.route_procedure,
                         &temp_wp.record_index);

        // 插入AVL树
        if (ret == 6)
        {
            waypoint_avl_root = waypoint_avl_insert(waypoint_avl_root, &temp_wp);
            if (waypoint_avl_root == NULL && waypoint_count == 0)
            {
                fprintf(stderr, "错误：航路点AVL树插入失败\n");
                fclose(fp);
                return false;
            }
        }
        else
        {
            fprintf(stderr, "Warning: Failed to parse waypoint line: %s\n", line_buf);
        }
    }

    fclose(fp);
    printf("Loaded %d waypoints to AVL tree\n", waypoint_count);
    return true;
}
// 销毁机场AVL树（后序遍历）
void destroy_airport_avl(AirportAVLNode *root)
{
    if (root == NULL)
    {
        return;
    }
    destroy_airport_avl(root->left);
    destroy_airport_avl(root->right);
    free(root);
}
// 销毁航路点AVL树（后序遍历）
void destroy_waypoint_avl(WaypointAVLNode *root)
{
    if (root == NULL)
    {
        return;
    }
    destroy_waypoint_avl(root->left);
    destroy_waypoint_avl(root->right);
    free(root);
}
// 按ICAO代码查询机场（AVL树迭代查询）
Airport *fmc_query_airport_by_icao(const char *icao_code)
{
    if (airport_avl_root == NULL || icao_code == NULL)
    {
        printf("入参错误: icao_code=%p, airport_avl_root=%p\n", (void *)icao_code, (void *)airport_avl_root);
        return NULL;
    }

    // 预处理查询关键字
    char query_icao_trim[MAX_ICAO_CODE_LEN] = {0};
    strncpy(query_icao_trim, icao_code, MAX_ICAO_CODE_LEN - 1);
    trim_trailing_spaces(query_icao_trim, MAX_ICAO_CODE_LEN);

    // 迭代查询（效率更高）
    AirportAVLNode *current = airport_avl_root;
    while (current != NULL)
    {
        char curr_icao_trim[MAX_ICAO_CODE_LEN] = {0};
        strncpy(curr_icao_trim, current->data.icao_code, MAX_ICAO_CODE_LEN - 1);
        trim_trailing_spaces(curr_icao_trim, MAX_ICAO_CODE_LEN);

        int cmp = strcmp(query_icao_trim, curr_icao_trim);
        if (cmp == 0)
        {
            return &(current->data); // 找到匹配
        }
        else if (cmp < 0)
        {
            current = current->left; // 左子树查找
        }
        else
        {
            current = current->right; // 右子树查找
        }
    }

    printf("未找到机场: %s\n", icao_code);
    return NULL;
}
// 按航路点代码查询航路点（AVL树迭代查询）
Waypoint *fmc_query_waypoint_by_code(const char *wp_code)
{
    if (waypoint_avl_root == NULL || wp_code == NULL)
    {
        printf("查询航路点入参错误: wp_code=%p, waypoint_avl_root=%p\n", (void *)wp_code, (void *)waypoint_avl_root);
        return NULL;
    }

    // 预处理查询关键字
    char query_wp_trim[MAX_WAYPOINT_CODE_LEN] = {0};
    strncpy(query_wp_trim, wp_code, MAX_WAYPOINT_CODE_LEN - 1);
    trim_trailing_spaces(query_wp_trim, MAX_WAYPOINT_CODE_LEN);

    // 迭代查询
    WaypointAVLNode *current = waypoint_avl_root;
    while (current != NULL)
    {
        char curr_wp_trim[MAX_WAYPOINT_CODE_LEN] = {0};
        strncpy(curr_wp_trim, current->data.wp_code, MAX_WAYPOINT_CODE_LEN - 1);
        trim_trailing_spaces(curr_wp_trim, MAX_WAYPOINT_CODE_LEN);

        int cmp = strcmp(query_wp_trim, curr_wp_trim);
        if (cmp == 0)
        {
            return &(current->data); // 找到匹配
        }
        else if (cmp < 0)
        {
            current = current->left; // 左子树查找
        }
        else
        {
            current = current->right; // 右子树查找
        }
    }

    printf("未找到航路点: %s\n", wp_code);
    return NULL;
}
// 设置速度高度限制
int setSpdAltLimit(const char *spd_alt_str, SpdAltLimit *spd_alt_limit) {
    if (spd_alt_str == NULL || spd_alt_limit == NULL) {
        return -1;
    }

    char *slash_pos = strchr(spd_alt_str, '/');
    if (slash_pos == NULL) {
        return -1;
    }

    char speed_str[16] = {0};
    char altitude_str[16] = {0};
    int speed_len = slash_pos - spd_alt_str;
    if (speed_len >= 16) {
        return -1;
    }

    strncpy(speed_str, spd_alt_str, speed_len);
    strcpy(altitude_str, slash_pos + 1);

    int speed = atoi(speed_str);
    int altitude = atoi(altitude_str);

    if (speed < 100 || speed > 399) {
        return -2;
    }

    if (altitude < 1000 || altitude > 99990) {
        return -3;
    }

    spd_alt_limit->spd_limit = speed;
    spd_alt_limit->alt_limit = altitude;

    return 0;
}

// 设置目标速度
int setTgtSpeed(char *speed, TgtSpeed *target_speed) {
    if (speed == NULL || target_speed == NULL) {
        return -1;
    }

    if (speed[0] == '/' && speed[1] == '.') {
        int mach = atoi(speed + 2);
        if (mach >= 40 && mach <= 95) {
            target_speed->speed2 = mach;
            return 2;
        } else {
            return -3;
        }
    } else {
        int spd = atoi(speed);
        if (spd >= 100 && spd <= 399) {
            target_speed->speed1 = spd;
            return 1;
        } else {
            return -2;
        }
    }

    return 0;
}

// 添加元素AAA
void add_element(char *name, ElementType type, char *airport) {
    if (g_element_count >= 200)
        return;
    AirportElement *elem = &g_elements[g_element_count++];
    strcpy(elem->name, name);
    strcpy(elem->airport, airport);
    elem->type = type;
}

// 添加关联关系
void add_relation(char *airport, char *elem1, ElementType el1_type, char *elem2, ElementType el2_type) {
    if (g_relation_count >= 2000)
        return;
    Relation *rel = &g_relations[g_relation_count++];
    strcpy(rel->airport, airport);
    strcpy(rel->elem1, elem1);
    rel->elem1_type = el1_type;
    strcpy(rel->elem2, elem2);
    rel->elem2_type = el2_type;
}
// 初始化机场数据AAA
void init_airport_data(void) {
    // 初始化展示列表内存
    runway = (char **)malloc(sizeof(char *) * 100);
    proc = (char **)malloc(sizeof(char *) * 100);
    runway_trans = (char **)malloc(sizeof(char *) * 100);
    proc_trans = (char **)malloc(sizeof(char *) * 100);
    for (int i = 0; i < 100; i++) {
        runway[i] = (char *)malloc(sizeof(char) * 20);
        proc[i] = (char *)malloc(sizeof(char) * 20);
        runway_trans[i] = (char *)malloc(sizeof(char) * 20);
        proc_trans[i] = (char *)malloc(sizeof(char) * 20);
    }

    // 初始化KSEA机场元素
    add_element("RW16C", TYPE_RUNWAY, "KSEA");
    add_element("RW16L", TYPE_RUNWAY, "KSEA");
    add_element("RW34C", TYPE_RUNWAY, "KSEA");
    add_element("ATOM E2", TYPE_TAKEOFF_PROC, "KSEA");
    add_element("BANGR 9", TYPE_TAKEOFF_PROC, "KSEA");
    add_element("SUMMA 1", TYPE_TAKEOFF_PROC, "KSEA");
    add_element("COV", TYPE_WAYPOINT, "KSEA");
    add_element("HBM", TYPE_WAYPOINT, "KSEA");
    add_element("XYZ", TYPE_WAYPOINT, "KSEA");

    // 初始化KBFI机场元素
    add_element("RW14R", TYPE_RUNWAY, "KBFI");
    add_element("RW14L", TYPE_RUNWAY, "KBFI");
    add_element("RW32L", TYPE_RUNWAY, "KBFI");
    add_element("RW32R", TYPE_RUNWAY, "KBFI");
    add_element("CHINS 3", TYPE_TAKEOFF_PROC, "KBFI");
    add_element("EPH 8", TYPE_TAKEOFF_PROC, "KBFI");
    add_element("HAROB 4", TYPE_TAKEOFF_PROC, "KBFI");
    add_element("JAKSN 5", TYPE_TAKEOFF_PROC, "KBFI");
    add_element("PAINE 2", TYPE_TAKEOFF_PROC, "KBFI");
    add_element("SUMMA 6", TYPE_TAKEOFF_PROC, "KBFI");

    // 初始化关联关系（KBFI跑道与程序）
    add_relation("KBFI", "RW14R", TYPE_RUNWAY, "CHINS 3", TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW14R", TYPE_RUNWAY, "HAROB 4", TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW14L", TYPE_RUNWAY, "EPH 8", TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW14L", TYPE_RUNWAY, "JAKSN 5", TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW32L", TYPE_RUNWAY, "CHINS 3", TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW32L", TYPE_RUNWAY, "PAINE 2", TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW32R", TYPE_RUNWAY, "EPH 8", TYPE_TAKEOFF_PROC);
    add_relation("KBFI", "RW32R", TYPE_RUNWAY, "SUMMA 6", TYPE_TAKEOFF_PROC);

    // 初始化关联关系
    add_relation("KSEA", "RW16C", TYPE_RUNWAY, "ATOM E2", TYPE_TAKEOFF_PROC);
    add_relation("KSEA", "RW16C", TYPE_RUNWAY, "SUMMA 1", TYPE_TAKEOFF_PROC);
    add_relation("KSEA", "RW16L", TYPE_RUNWAY, "BANGR 9", TYPE_TAKEOFF_PROC);
    add_relation("KSEA", "RW34C", TYPE_RUNWAY, "SUMMA 1", TYPE_TAKEOFF_PROC);
    add_relation("KSEA", "RW16C", TYPE_RUNWAY, "COV", TYPE_WAYPOINT);
    add_relation("KSEA", "RW16C", TYPE_RUNWAY, "HBM", TYPE_WAYPOINT);
    add_relation("KSEA", "RW34C", TYPE_RUNWAY, "XYZ", TYPE_WAYPOINT);
    add_relation("KSEA", "ATOM E2", TYPE_TAKEOFF_PROC, "COV", TYPE_WAYPOINT);
    add_relation("KSEA", "SUMMA 1", TYPE_TAKEOFF_PROC, "HBM", TYPE_WAYPOINT);

    // 初始化ZUUU（成都双流）机场元素
    add_element("RW02L", TYPE_RUNWAY, "ZUUU");
    add_element("RW02R", TYPE_RUNWAY, "ZUUU");
    add_element("RW20L", TYPE_RUNWAY, "ZUUU");
    add_element("RW20R", TYPE_RUNWAY, "ZUUU");
    add_element("CTU 1X", TYPE_TAKEOFF_PROC, "ZUUU");
    add_element("GORGY 1X", TYPE_TAKEOFF_PROC, "ZUUU");
    add_element("POMOK 1X", TYPE_TAKEOFF_PROC, "ZUUU");
    add_element("ROBIG 1X", TYPE_TAKEOFF_PROC, "ZUUU");
    add_element("XARPI 1X", TYPE_TAKEOFF_PROC, "ZUUU");
    add_element("ZYU 1X", TYPE_TAKEOFF_PROC, "ZUUU");

    // 初始化关联关系（ZUUU跑道与程序）
    add_relation("ZUUU", "RW02L", TYPE_RUNWAY, "CTU 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW02L", TYPE_RUNWAY, "GORGY 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW02R", TYPE_RUNWAY, "POMOK 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW02R", TYPE_RUNWAY, "ROBIG 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW20L", TYPE_RUNWAY, "CTU 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW20L", TYPE_RUNWAY, "XARPI 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW20R", TYPE_RUNWAY, "POMOK 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUUU", "RW20R", TYPE_RUNWAY, "ZYU 1X", TYPE_TAKEOFF_PROC);

    // 初始化ZUCK（重庆江北）机场元素
    add_element("RW02", TYPE_RUNWAY, "ZUCK");
    add_element("RW03", TYPE_RUNWAY, "ZUCK");
    add_element("RW20", TYPE_RUNWAY, "ZUCK");
    add_element("RW21", TYPE_RUNWAY, "ZUCK");
    add_element("CKG 1X", TYPE_TAKEOFF_PROC, "ZUCK");
    add_element("DUGUB 1X", TYPE_TAKEOFF_PROC, "ZUCK");
    add_element("GOVSA 1X", TYPE_TAKEOFF_PROC, "ZUCK");
    add_element("IDLUN 1X", TYPE_TAKEOFF_PROC, "ZUCK");
    add_element("MEKNO 1X", TYPE_TAKEOFF_PROC, "ZUCK");
    add_element("NILOM 1X", TYPE_TAKEOFF_PROC, "ZUCK");

    // 初始化关联关系（ZUCK跑道与程序）
    add_relation("ZUCK", "RW02", TYPE_RUNWAY, "CKG 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW02", TYPE_RUNWAY, "DUGUB 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW02", TYPE_RUNWAY, "GOVSA 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW03", TYPE_RUNWAY, "GOVSA 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW03", TYPE_RUNWAY, "IDLUN 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW03", TYPE_RUNWAY, "MEKNO 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW20", TYPE_RUNWAY, "CKG 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW20", TYPE_RUNWAY, "DUGUB 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW20", TYPE_RUNWAY, "MEKNO 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW21", TYPE_RUNWAY, "GOVSA 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW21", TYPE_RUNWAY, "IDLUN 1X", TYPE_TAKEOFF_PROC);
    add_relation("ZUCK", "RW21", TYPE_RUNWAY, "NILOM 1X", TYPE_TAKEOFF_PROC);
}

// 销毁机场数据
void destroy_airport_data(void) {
    // 释放展示列表内存
    if (runway) {
        for (int i = 0; i < 100; i++) {
            if (runway[i]) free(runway[i]);
        }
        free(runway);
        runway = NULL;
    }

    if (proc) {
        for (int i = 0; i < 100; i++) {
            if (proc[i]) free(proc[i]);
        }
        free(proc);
        proc = NULL;
    }

    if (runway_trans) {
        for (int i = 0; i < 100; i++) {
            if (runway_trans[i]) free(runway_trans[i]);
        }
        free(runway_trans);
        runway_trans = NULL;
    }

    if (proc_trans) {
        for (int i = 0; i < 100; i++) {
            if (proc_trans[i]) free(proc_trans[i]);
        }
        free(proc_trans);
        proc_trans = NULL;
    }

    // 释放VIATO
    if (via_to_list) {
        free(via_to_list);
        via_to_list = NULL;
        via_to_list_count = 0;
    }

    // 重置计数器
    g_element_count = 0;
    g_relation_count = 0;
    runway_count = 0;
    proc_count = 0;
    runway_trans_count = 0;
    proc_trans_count = 0;
}

// 查询机场对应的跑道和程序
int query_runway_proc_by_airport(const char *airport) {
    runway_count = 0;
    proc_count = 0;
    for (int i = 0; i < g_element_count; i++) {
        if (strcmp(g_elements[i].airport, airport) == 0) {
            if (g_elements[i].type == TYPE_RUNWAY) {
                strcpy(runway[runway_count], g_elements[i].name);
                runway_count++;
            } else if (g_elements[i].type == TYPE_TAKEOFF_PROC) {
                strcpy(proc[proc_count], g_elements[i].name);
                proc_count++;
            }
        }
    }
    return runway_count + proc_count;
}

// 按机场+跑道查询程序
int query_proc_by_runway(const char *airport, const char *runway) {
    proc_count = 0;
    for (int i = 0; i < g_relation_count; i++) {
        if (strcmp(g_relations[i].airport, airport) == 0 && 
            (strcmp(g_relations[i].elem1, runway) == 0 || strcmp(g_relations[i].elem2, runway) == 0)) {
            if (strcmp(g_relations[i].elem1, runway) == 0 && g_relations[i].elem2_type == TYPE_TAKEOFF_PROC) {
                strcpy(proc[proc_count], g_relations[i].elem2);
                proc_count++;
            } else if (strcmp(g_relations[i].elem2, runway) == 0 && g_relations[i].elem1_type == TYPE_TAKEOFF_PROC) {
                strcpy(proc[proc_count], g_relations[i].elem1);
                proc_count++;
            }
        }
    }
    return proc_count;
}

// 按机场+跑道查询过渡点
int query_trans_by_runway(const char *airport, const char *runway) {
    runway_trans_count = 0;
    for (int i = 0; i < g_relation_count; i++) {
        if (strcmp(g_relations[i].airport, airport) == 0 && 
            (strcmp(g_relations[i].elem1, runway) == 0 || strcmp(g_relations[i].elem2, runway) == 0)) {
            if (strcmp(g_relations[i].elem1, runway) == 0 && g_relations[i].elem2_type == TYPE_WAYPOINT) {
                strcpy(runway_trans[runway_trans_count], g_relations[i].elem2);
                runway_trans_count++;
            } else if (strcmp(g_relations[i].elem2, runway) == 0 && g_relations[i].elem1_type == TYPE_WAYPOINT) {
                strcpy(runway_trans[runway_trans_count], g_relations[i].elem1);
                runway_trans_count++;
            }
        }
    }
    return runway_trans_count;
}

// 按机场+程序查询跑道
int query_runway_by_proc(const char *airport, const char *proc) {
    runway_count = 0;
    for (int i = 0; i < g_relation_count; i++) {
        if (strcmp(g_relations[i].airport, airport) == 0 && 
            (strcmp(g_relations[i].elem1, proc) == 0 || strcmp(g_relations[i].elem2, proc) == 0)) {
            if (strcmp(g_relations[i].elem1, proc) == 0 && g_relations[i].elem2_type == TYPE_RUNWAY) {
                strcpy(runway[runway_count], g_relations[i].elem2);
                runway_count++;
            } else if (strcmp(g_relations[i].elem2, proc) == 0 && g_relations[i].elem1_type == TYPE_RUNWAY) {
                strcpy(runway[runway_count], g_relations[i].elem1);
                runway_count++;
            }
        }
    }
    return runway_count;
}

// 按机场+程序查询过渡点
int query_trans_by_proc(const char *airport, const char *proc) {
    proc_trans_count = 0;
    for (int i = 0; i < g_relation_count; i++) {
        if (strcmp(g_relations[i].airport, airport) == 0 && 
            (strcmp(g_relations[i].elem1, proc) == 0 || strcmp(g_relations[i].elem2, proc) == 0)) {
            if (strcmp(g_relations[i].elem1, proc) == 0 && g_relations[i].elem2_type == TYPE_WAYPOINT) {
                strcpy(proc_trans[proc_trans_count], g_relations[i].elem2);
                proc_trans_count++;
            } else if (strcmp(g_relations[i].elem2, proc) == 0 && g_relations[i].elem1_type == TYPE_WAYPOINT) {
                strcpy(proc_trans[proc_trans_count], g_relations[i].elem1);
                proc_trans_count++;
            }
        }
    }
    return proc_trans_count;
}
