//检测按的是什么键
#include "fmc_key.h"
#include "fmc_connect.h"

#include <stdio.h>
#include <string.h>

static void action_page(FMC_Data *data, const FMC_Button *button);
static void action_lsk(FMC_Data *data, const FMC_Button *button);
static void action_text(FMC_Data *data, const FMC_Button *button);
static void action_delete(FMC_Data *data, const FMC_Button *button);
static void action_clear(FMC_Data *data, const FMC_Button *button);
static void action_exec(FMC_Data *data, const FMC_Button *button);
static void action_prev_page(FMC_Data *data, const FMC_Button *button);
static void action_next_page(FMC_Data *data, const FMC_Button *button);
static int send_button_to_xplane(const FMC_Button *button);
static int button_sync_deferred_until_data_success(const FMC_Data *data, const FMC_Button *button);

#define SCREEN_WIDTH 48
#define SCREEN_HEIGHT 36
#define FUNCTION_WIDTH 72
#define FUNCTION_HEIGHT 51
#define LETTER_LENGTH 49
#define NUMB_R 25
#define FMC_BUTTON_COUNT 69

static FMC_Button fmc_buttons[FMC_BUTTON_COUNT];
static int fmc_buttons_loaded = 0;

static void set_rect_button(int index,
                            FMC_ButtonId id,
                            int x,
                            int y,
                            int w,
                            int h,
                            const char *label,
                            char input_char,
                            FMC_Page page,
                            FMC_LineSelectKey line_select,
                            FMC_ButtonAction action)
{
    fmc_buttons[index].id = id;
    fmc_buttons[index].shape = FMC_BUTTON_SHAPE_RECT;
    fmc_buttons[index].rect.x = x;
    fmc_buttons[index].rect.y = y;
    fmc_buttons[index].rect.w = w;
    fmc_buttons[index].rect.h = h;
    fmc_buttons[index].label = label;
    fmc_buttons[index].input_char = input_char;
    fmc_buttons[index].page = page;
    fmc_buttons[index].line_select = line_select;
    fmc_buttons[index].action = action;
}

static void set_circle_button(int index,
                              int x,
                              int y,
                              int radius,
                              const char *label,
                              char input_char)
{
    fmc_buttons[index].id = FMC_BUTTON_TEXT;
    fmc_buttons[index].shape = FMC_BUTTON_SHAPE_CIRCLE;
    fmc_buttons[index].center.x = x;
    fmc_buttons[index].center.y = y;
    fmc_buttons[index].radius = radius;
    fmc_buttons[index].label = label;
    fmc_buttons[index].input_char = input_char;
    fmc_buttons[index].page = FMC_PAGE_INDEX;
    fmc_buttons[index].line_select = FMC_LSK_NONE;
    fmc_buttons[index].action = action_text;
}

int fmc_buttons_load(void)
{
    static const char *letter_labels[] = {
        "A", "B", "C", "D", "E",
        "F", "G", "H", "I", "J",
        "K", "L", "M", "N", "O",
        "P", "Q", "R", "S", "T",
        "U", "V", "W", "X", "Y",
        "Z"};
    static const char *num_labels[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "+/-"};
    static const char num_chars[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '0', '-'};

    memset(fmc_buttons, 0, sizeof(fmc_buttons));

    int index = 0;

    set_rect_button(index++, FMC_BUTTON_LSK_L1, 7, 118, SCREEN_WIDTH, SCREEN_HEIGHT, "L1", '\0', FMC_PAGE_INDEX, FMC_LSK_L1, action_lsk);
    set_rect_button(index++, FMC_BUTTON_LSK_L2, 7, 166, SCREEN_WIDTH, SCREEN_HEIGHT, "L2", '\0', FMC_PAGE_INDEX, FMC_LSK_L2, action_lsk);
    set_rect_button(index++, FMC_BUTTON_LSK_L3, 7, 214, SCREEN_WIDTH, SCREEN_HEIGHT, "L3", '\0', FMC_PAGE_INDEX, FMC_LSK_L3, action_lsk);
    set_rect_button(index++, FMC_BUTTON_LSK_L4, 7, 262, SCREEN_WIDTH, SCREEN_HEIGHT, "L4", '\0', FMC_PAGE_INDEX, FMC_LSK_L4, action_lsk);
    set_rect_button(index++, FMC_BUTTON_LSK_L5, 7, 310, SCREEN_WIDTH, SCREEN_HEIGHT, "L5", '\0', FMC_PAGE_INDEX, FMC_LSK_L5, action_lsk);
    set_rect_button(index++, FMC_BUTTON_LSK_L6, 7, 358, SCREEN_WIDTH, SCREEN_HEIGHT, "L6", '\0', FMC_PAGE_INDEX, FMC_LSK_L6, action_lsk);

    set_rect_button(index++, FMC_BUTTON_LSK_R1, 587, 118, SCREEN_WIDTH, SCREEN_HEIGHT, "R1", '\0', FMC_PAGE_INDEX, FMC_LSK_R1, action_lsk);
    set_rect_button(index++, FMC_BUTTON_LSK_R2, 587, 166, SCREEN_WIDTH, SCREEN_HEIGHT, "R2", '\0', FMC_PAGE_INDEX, FMC_LSK_R2, action_lsk);
    set_rect_button(index++, FMC_BUTTON_LSK_R3, 587, 214, SCREEN_WIDTH, SCREEN_HEIGHT, "R3", '\0', FMC_PAGE_INDEX, FMC_LSK_R3, action_lsk);
    set_rect_button(index++, FMC_BUTTON_LSK_R4, 587, 262, SCREEN_WIDTH, SCREEN_HEIGHT, "R4", '\0', FMC_PAGE_INDEX, FMC_LSK_R4, action_lsk);
    set_rect_button(index++, FMC_BUTTON_LSK_R5, 587, 310, SCREEN_WIDTH, SCREEN_HEIGHT, "R5", '\0', FMC_PAGE_INDEX, FMC_LSK_R5, action_lsk);
    set_rect_button(index++, FMC_BUTTON_LSK_R6, 587, 358, SCREEN_WIDTH, SCREEN_HEIGHT, "R6", '\0', FMC_PAGE_INDEX, FMC_LSK_R6, action_lsk);

    set_rect_button(index++, FMC_BUTTON_INIT_REF, 69, 477, FUNCTION_WIDTH, FUNCTION_HEIGHT, "INIT REF", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, action_page);
    set_rect_button(index++, FMC_BUTTON_RTE, 153, 477, FUNCTION_WIDTH, FUNCTION_HEIGHT, "RTE", '\0', FMC_PAGE_ROUTE, FMC_LSK_NONE, action_page);
    set_rect_button(index++, FMC_BUTTON_CLB, 235, 477, FUNCTION_WIDTH, FUNCTION_HEIGHT, "CLB", '\0', FMC_PAGE_CLIMB, FMC_LSK_NONE, action_page);
    set_rect_button(index++, FMC_BUTTON_CRZ, 318, 477, FUNCTION_WIDTH, FUNCTION_HEIGHT, "CRZ", '\0', FMC_PAGE_CRUISE, FMC_LSK_NONE, action_page);
    set_rect_button(index++, FMC_BUTTON_DES, 401, 477, FUNCTION_WIDTH, FUNCTION_HEIGHT, "DES", '\0', FMC_PAGE_DESCENT, FMC_LSK_NONE, action_page);

    set_rect_button(index++, FMC_BUTTON_DIR_INTC, 69, 536, FUNCTION_WIDTH, FUNCTION_HEIGHT, "DIR INTC", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, NULL);
    set_rect_button(index++, FMC_BUTTON_LEGS, 153, 536, FUNCTION_WIDTH, FUNCTION_HEIGHT, "LEGS", '\0', FMC_PAGE_LEGS, FMC_LSK_NONE, action_page);
    set_rect_button(index++, FMC_BUTTON_DEP_ARR, 235, 536, FUNCTION_WIDTH, FUNCTION_HEIGHT, "DEP ARR", '\0', FMC_PAGE_DEP_ARR, FMC_LSK_NONE, action_page);
    set_rect_button(index++, FMC_BUTTON_HOLD, 318, 536, FUNCTION_WIDTH, FUNCTION_HEIGHT, "HOLD", '\0', FMC_PAGE_HOLD, FMC_LSK_NONE, action_page);
    set_rect_button(index++, FMC_BUTTON_STATUS, 401, 536, FUNCTION_WIDTH, FUNCTION_HEIGHT, "PROG", '\0', FMC_PAGE_STATUS, FMC_LSK_NONE, action_page);

    set_rect_button(index++, FMC_BUTTON_FIX, 69, 595, FUNCTION_WIDTH, FUNCTION_HEIGHT, "FIX", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, NULL);
    set_rect_button(index++, FMC_BUTTON_NAV_RAD, 153, 595, FUNCTION_WIDTH, FUNCTION_HEIGHT, "NAV RAD", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, NULL);

    set_rect_button(index++, FMC_BUTTON_PREV_PAGE, 69, 655, FUNCTION_WIDTH, FUNCTION_HEIGHT, "PREV PAGE", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, action_prev_page);
    set_rect_button(index++, FMC_BUTTON_NEXT_PAGE, 153, 655, FUNCTION_WIDTH, FUNCTION_HEIGHT, "NEXT PAGE", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, action_next_page);
    set_rect_button(index++, FMC_BUTTON_EXEC, 500, 550, 70, 34, "EXEC", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, action_exec);

    for (int i = 0; i < 26; ++i)
    {
        set_rect_button(index++,
                        FMC_BUTTON_TEXT,
                        263 + (i % 5) * 68,
                        613 + (i / 5) * 62,
                        LETTER_LENGTH,
                        LETTER_LENGTH,
                        letter_labels[i],
                        (char)('A' + i),
                        FMC_PAGE_INDEX,
                        FMC_LSK_NONE,
                        action_text);
    }

    for (int i = 0; i < 12; ++i)
    {
        set_circle_button(index++,
                          89 + (i % 3) * 62,
                          762 + (i / 3) * 62,
                          NUMB_R,
                          num_labels[i],
                          num_chars[i]);
    }

    set_rect_button(index++, FMC_BUTTON_TEXT, 333, 924, LETTER_LENGTH, LETTER_LENGTH, "SP", ' ', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text);
    set_rect_button(index++, FMC_BUTTON_DEL, 401, 924, LETTER_LENGTH, LETTER_LENGTH, "DEL", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, action_delete);
    set_rect_button(index++, FMC_BUTTON_TEXT, 468, 924, LETTER_LENGTH, LETTER_LENGTH, "/", '/', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text);
    set_rect_button(index++, FMC_BUTTON_CLR, 534, 924, LETTER_LENGTH, LETTER_LENGTH, "CLR", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, action_clear);

    fmc_buttons_loaded = 1;
    return 0;
}

static void ensure_buttons_loaded(void)
{
    if (!fmc_buttons_loaded)
    {
        fmc_buttons_load();
    }
}


static void action_page(FMC_Data *data, const FMC_Button *button)
{
    printf("FMC event: action_page %s\n", button != NULL && button->label != NULL ? button->label : "");
    if (data == NULL || button == NULL)
    {
        return;
    }

    fmc_data_set_page(data, button->page);
}

static void action_lsk(FMC_Data *data, const FMC_Button *button)
{
    printf("FMC event: action_lsk %s\n", button != NULL && button->label != NULL ? button->label : "");
    if (data == NULL || button == NULL)
    {
        return;
    }

    if (data->current_page == FMC_PAGE_CLIMB)
    {
        if (button->line_select == FMC_LSK_L1)
        {
            fmc_data_set_phase_parameter(data, 1);
            return;
        }
        if (button->line_select == FMC_LSK_L2)
        {
            fmc_data_set_phase_parameter(data, 2);
            return;
        }
        if (button->line_select == FMC_LSK_R1)
        {
            fmc_data_set_phase_parameter(data, 3);
            return;
        }
    }

    if (data->current_page == FMC_PAGE_CRUISE)
    {
        if (button->line_select == FMC_LSK_L1)
        {
            fmc_data_set_phase_parameter(data, 1);
            return;
        }
        if (button->line_select == FMC_LSK_R1)
        {
            fmc_data_set_phase_parameter(data, 4);
            return;
        }
    }

    if (data->current_page == FMC_PAGE_DESCENT)
    {
        if (button->line_select == FMC_LSK_L1)
        {
            fmc_data_set_phase_parameter(data, 1);
            return;
        }
        if (button->line_select == FMC_LSK_L2)
        {
            fmc_data_set_phase_parameter(data, 2);
            return;
        }
        if (button->line_select == FMC_LSK_R1)
        {
            fmc_data_set_phase_parameter(data, 5);
            return;
        }
        if (button->line_select == FMC_LSK_R3)
        {
            fmc_data_set_phase_parameter(data, 6);
            return;
        }
    }

    if (data->current_page == FMC_PAGE_DEP_ARR)
    {
        if (button->line_select >= FMC_LSK_L1 && button->line_select <= FMC_LSK_L5)
        {
            fmc_data_handle_dep_arr_lsk(data, 0, button->line_select - FMC_LSK_L1 + 1);
            return;
        }
        if (button->line_select >= FMC_LSK_R1 && button->line_select <= FMC_LSK_R5)
        {
            fmc_data_handle_dep_arr_lsk(data, 1, button->line_select - FMC_LSK_R1 + 1);
            return;
        }
        if (button->line_select == FMC_LSK_L6)
        {
            fmc_data_dep_arr_back_to_index(data);
            return;
        }
    }

    if (data->current_page == FMC_PAGE_HOME)
    {
        if (button->line_select == FMC_LSK_L1 ||
            button->line_select == FMC_LSK_R1 ||
            button->line_select == FMC_LSK_R2)
        {
            fmc_data_set_page(data, FMC_PAGE_DEP_ARR);
        }
        else if (button->line_select == FMC_LSK_L3)
        {
            fmc_data_set_page(data, FMC_PAGE_ROUTE);
        }
        else if (button->line_select == FMC_LSK_L4)
        {
            fmc_data_set_page(data, FMC_PAGE_PERF);
        }
        else if (button->line_select == FMC_LSK_R4)
        {
            fmc_data_set_page(data, FMC_PAGE_LEGS);
        }
        return;
    }

    if (data->current_page == FMC_PAGE_PERF)
    {
        if (button->line_select == FMC_LSK_L6)
        {
            fmc_data_set_page(data, FMC_PAGE_HOME);
            return;
        }
        if (button->line_select == FMC_LSK_R6)
        {
            fmc_data_set_page(data, FMC_PAGE_CLIMB);
            return;
        }
        return;
    }

    if (data->current_page == FMC_PAGE_ROUTE)
    {
        if (button->line_select == FMC_LSK_L1)
        {
            fmc_data_set_route_field(data, FMC_ROUTE_FIELD_ORIGIN);
            return;
        }
        if (button->line_select == FMC_LSK_R1)
        {
            fmc_data_set_route_field(data, FMC_ROUTE_FIELD_DESTINATION);
            return;
        }
        if (button->line_select == FMC_LSK_L2)
        {
            fmc_data_set_route_field(data, FMC_ROUTE_FIELD_COMPANY_ROUTE);
            return;
        }
        if (button->line_select == FMC_LSK_R3)
        {
            fmc_data_set_route_field(data, FMC_ROUTE_FIELD_FLIGHT_NO);
            return;
        }
        if (button->line_select == FMC_LSK_R4)
        {
            if (rte_index == 1 && fmc_data_route_page_count(data) > 1)
            {
                fmc_data_route_next_page(data);
            }
            return;
        }
        if (button->line_select == FMC_LSK_L5)
        {
            if (rte_index == 1 && data->scratchpad_len == 0 && fmc_data_route_page_count(data) > 1)
            {
                fmc_data_route_next_page(data);
                return;
            }
            fmc_data_set_route_field(data, FMC_ROUTE_FIELD_VIA);
            return;
        }
        if (button->line_select == FMC_LSK_R5)
        {
            fmc_data_set_route_field(data, FMC_ROUTE_FIELD_TO_FIX);
            return;
        }
        if (button->line_select == FMC_LSK_L6)
        {
            fmc_data_set_page(data, FMC_PAGE_HOME);
            return;
        }
        if (button->line_select == FMC_LSK_R6)
        {
            fmc_data_set_page(data, FMC_PAGE_CLIMB);
            return;
        }
        return;
    }

    if (data->current_page == FMC_PAGE_LEGS)
    {
        if (button->line_select == FMC_LSK_R1)
        {
            fmc_data_set_legs_parameter(data, 1);
            return;
        }
        if (button->line_select == FMC_LSK_L6)
        {
            fmc_data_set_page(data, FMC_PAGE_ROUTE);
            return;
        }
        return;
    }

    if (data->current_page == FMC_PAGE_HOLD)
    {
        if (button->line_select >= FMC_LSK_L1 && button->line_select <= FMC_LSK_L4)
        {
            fmc_data_set_hold_parameter(data, button->line_select - FMC_LSK_L1 + 1);
            return;
        }
        if (button->line_select == FMC_LSK_R1)
        {
            fmc_data_set_hold_parameter(data, 5);
            return;
        }
        return;
    }

    if (data->current_page == FMC_PAGE_STATUS && button->line_select == FMC_LSK_L6)
    {
        fmc_data_clear_scratchpad(data);
    }
}

static void action_text(FMC_Data *data, const FMC_Button *button)
{
    printf("FMC event: action_text %s\n", button != NULL && button->label != NULL ? button->label : "");
    if (data == NULL || button == NULL || button->input_char == '\0')
    {
        return;
    }

    fmc_data_append_char(data, button->input_char);
}

static void action_delete(FMC_Data *data, const FMC_Button *button)
{
    (void)button;
    printf("FMC event: action_delete\n");
    fmc_data_show_delete(data);
}

static void action_clear(FMC_Data *data, const FMC_Button *button)
{
    (void)button;
    printf("FMC event: action_clear\n");
    fmc_data_backspace(data);
}

static void action_exec(FMC_Data *data, const FMC_Button *button)
{
    (void)button;
    printf("FMC event: action_exec\n");
    if (data != NULL &&
        (data->current_page == FMC_PAGE_CLIMB ||
         data->current_page == FMC_PAGE_CRUISE ||
         data->current_page == FMC_PAGE_DESCENT))
    {
        fmc_data_activate_current_phase(data);
        setExec();
        return;
    }

    fmc_data_exec_route_selection(data);
}

static const char *xplane_command_for_button(const FMC_Button *button)
{
    if (button == NULL)
    {
        return NULL;
    }

    switch (button->id)
    {
    case FMC_BUTTON_INIT_REF:
        return "sim/FMS/init";
    case FMC_BUTTON_RTE:
        return "sim/FMS/fpln";
    case FMC_BUTTON_CLB:
        return "sim/FMS/clb";
    case FMC_BUTTON_CRZ:
        return "sim/FMS/crz";
    case FMC_BUTTON_DES:
        return "sim/FMS/des";
    case FMC_BUTTON_DIR_INTC:
        return "sim/FMS/dir_intc";
    case FMC_BUTTON_DEP_ARR:
        return "sim/FMS/dep_arr";
    case FMC_BUTTON_LEGS:
        return "sim/FMS/legs";
    case FMC_BUTTON_HOLD:
        return "sim/FMS/hold";
    case FMC_BUTTON_STATUS:
        return "sim/FMS/prog";
    case FMC_BUTTON_FIX:
        return "sim/FMS/fix";
    case FMC_BUTTON_NAV_RAD:
        return "sim/FMS/nav_rad";
    case FMC_BUTTON_PREV_PAGE:
        return "sim/FMS/prev";
    case FMC_BUTTON_NEXT_PAGE:
        return "sim/FMS/next";
    case FMC_BUTTON_CLR:
        return "sim/FMS/clear";
    case FMC_BUTTON_DEL:
        return "sim/FMS/delete";
    case FMC_BUTTON_LSK_L1:
        return "sim/FMS/ls_1l";
    case FMC_BUTTON_LSK_L2:
        return "sim/FMS/ls_2l";
    case FMC_BUTTON_LSK_L3:
        return "sim/FMS/ls_3l";
    case FMC_BUTTON_LSK_L4:
        return "sim/FMS/ls_4l";
    case FMC_BUTTON_LSK_L5:
        return "sim/FMS/ls_5l";
    case FMC_BUTTON_LSK_L6:
        return "sim/FMS/ls_6l";
    case FMC_BUTTON_LSK_R1:
        return "sim/FMS/ls_1r";
    case FMC_BUTTON_LSK_R2:
        return "sim/FMS/ls_2r";
    case FMC_BUTTON_LSK_R3:
        return "sim/FMS/ls_3r";
    case FMC_BUTTON_LSK_R4:
        return "sim/FMS/ls_4r";
    case FMC_BUTTON_LSK_R5:
        return "sim/FMS/ls_5r";
    case FMC_BUTTON_LSK_R6:
        return "sim/FMS/ls_6r";
    case FMC_BUTTON_EXEC:
    case FMC_BUTTON_TEXT:
    case FMC_BUTTON_NONE:
    default:
        return NULL;
    }
}

static int send_button_to_xplane(const FMC_Button *button)
{
    const char *command = NULL;

    if (button == NULL)
    {
        return 0;
    }

    if (button->id == FMC_BUTTON_TEXT)
    {
        return 0;
    }

    command = xplane_command_for_button(button);
    if (command == NULL)
    {
        return 0;
    }

    return fmc_xplane_send_command(command);
}

static int button_sync_deferred_until_data_success(const FMC_Data *data, const FMC_Button *button)
{
    if (data == NULL || button == NULL || data->current_page != FMC_PAGE_ROUTE)
    {
        return 0;
    }

    return button->line_select == FMC_LSK_L1 ||
           button->line_select == FMC_LSK_R1 ||
           button->line_select == FMC_LSK_R3;
}

static void action_prev_page(FMC_Data *data, const FMC_Button *button)
{
    (void)button;
    printf("FMC event: action_prev_page\n");
    if (data != NULL && data->current_page == FMC_PAGE_ROUTE)
    {
        fmc_data_route_prev_page(data);
    }
    else if (data != NULL && data->current_page == FMC_PAGE_DEP_ARR)
    {
        fmc_data_dep_arr_prev_page(data);
    }
}

static void action_next_page(FMC_Data *data, const FMC_Button *button)
{
    (void)button;
    printf("FMC event: action_next_page\n");
    if (data != NULL && data->current_page == FMC_PAGE_ROUTE)
    {
        fmc_data_route_next_page(data);
    }
    else if (data != NULL && data->current_page == FMC_PAGE_DEP_ARR)
    {
        fmc_data_dep_arr_next_page(data);
    }
}


static int point_in_rect(int x, int y, const SDL_Rect *rect)
{
    return rect != NULL &&
           x >= rect->x &&
           x < rect->x + rect->w &&
           y >= rect->y &&
           y < rect->y + rect->h;
}

static int point_in_circle(int x, int y, SDL_Point center, int radius)
{
    const int dx = x - center.x;
    const int dy = y - center.y;
    return radius > 0 && dx * dx + dy * dy <= radius * radius;
}

int fmc_key_button_count(void)
{
    ensure_buttons_loaded();
    return FMC_BUTTON_COUNT;
}

const FMC_Button *fmc_key_button_at(int index)
{
    ensure_buttons_loaded();
    if (index < 0 || index >= FMC_BUTTON_COUNT)
    {
        return NULL;
    }

    return &fmc_buttons[index];
}

int fmc_key_button_contains_base_point(const FMC_Button *button, int x, int y)
{
    if (button == NULL || button->id == FMC_BUTTON_NONE)
    {
        return 0;
    }

    if (button->shape == FMC_BUTTON_SHAPE_CIRCLE)
    {
        return point_in_circle(x, y, button->center, button->radius);
    }

    return point_in_rect(x, y, &button->rect);
}

int fmc_key_is_page_button(const FMC_Button *button)
{
    return button != NULL && button->action == action_page;
}

void fmc_key_activate_button(FMC_Data *data, const FMC_Button *button)
{
    if (button == NULL)
    {
        return;
    }

    if (button->id != FMC_BUTTON_EXEC &&
        !button_sync_deferred_until_data_success(data, button))
    {
        send_button_to_xplane(button);
    }

    if (button->action != NULL)
    {
        button->action(data, button);
    }
}
