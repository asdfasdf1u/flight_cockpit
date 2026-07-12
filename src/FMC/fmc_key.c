#include "fmc_key.h"

#include <stdio.h>

static void action_page(FMC_Data *data, const FMC_Button *button);
static void action_lsk(FMC_Data *data, const FMC_Button *button);
static void action_text(FMC_Data *data, const FMC_Button *button);
static void action_delete(FMC_Data *data, const FMC_Button *button);
static void action_clear(FMC_Data *data, const FMC_Button *button);
static void action_exec(FMC_Data *data, const FMC_Button *button);
static void action_prev_page(FMC_Data *data, const FMC_Button *button);
static void action_next_page(FMC_Data *data, const FMC_Button *button);

#define RECT_BUTTON(button_id, x, y, w, h, text, ch, page_value, lsk_value, handler) \
    {button_id, FMC_BUTTON_SHAPE_RECT, {x, y, w, h}, {0, 0}, 0, text, ch, page_value, lsk_value, handler}

#define CIRCLE_BUTTON(button_id, x, y, radius_value, text, ch, handler) \
    {button_id, FMC_BUTTON_SHAPE_CIRCLE, {0, 0, 0, 0}, {x, y}, radius_value, text, ch, FMC_PAGE_INDEX, FMC_LSK_NONE, handler}

static const FMC_Button FMC_BUTTONS[] = {
    RECT_BUTTON(FMC_BUTTON_INIT_REF, 69, 477, 72, 51, "INIT REF", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, action_page),
    RECT_BUTTON(FMC_BUTTON_RTE, 153, 477, 72, 51, "RTE", '\0', FMC_PAGE_ROUTE, FMC_LSK_NONE, action_page),
    RECT_BUTTON(FMC_BUTTON_CLB, 236, 477, 72, 51, "CLB", '\0', FMC_PAGE_CLIMB, FMC_LSK_NONE, action_page),
    RECT_BUTTON(FMC_BUTTON_CRZ, 319, 477, 72, 51, "CRZ", '\0', FMC_PAGE_CRUISE, FMC_LSK_NONE, action_page),
    RECT_BUTTON(FMC_BUTTON_DES, 401, 477, 72, 51, "DES", '\0', FMC_PAGE_DESCENT, FMC_LSK_NONE, action_page),
    RECT_BUTTON(FMC_BUTTON_DEP_ARR, 236, 536, 72, 51, "DEP ARR", '\0', FMC_PAGE_DEP_ARR, FMC_LSK_NONE, action_page),
    RECT_BUTTON(FMC_BUTTON_LEGS, 153, 536, 72, 51, "LEGS", '\0', FMC_PAGE_LEGS, FMC_LSK_NONE, action_page),
    RECT_BUTTON(FMC_BUTTON_HOLD, 319, 536, 72, 51, "HOLD", '\0', FMC_PAGE_HOLD, FMC_LSK_NONE, action_page),
    RECT_BUTTON(FMC_BUTTON_STATUS, 401, 536, 72, 51, "PROG", '\0', FMC_PAGE_STATUS, FMC_LSK_NONE, action_page),
    RECT_BUTTON(FMC_BUTTON_EXEC, 500, 536, 72, 51, "EXEC", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, action_exec),
    RECT_BUTTON(FMC_BUTTON_PREV_PAGE, 69, 655, 72, 51, "PREV PAGE", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, action_prev_page),
    RECT_BUTTON(FMC_BUTTON_NEXT_PAGE, 153, 655, 72, 51, "NEXT PAGE", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, action_next_page),

    RECT_BUTTON(FMC_BUTTON_LSK_L1, 6, 117, 48, 36, "L1", '\0', FMC_PAGE_INDEX, FMC_LSK_L1, action_lsk),
    RECT_BUTTON(FMC_BUTTON_LSK_L2, 6, 166, 48, 36, "L2", '\0', FMC_PAGE_INDEX, FMC_LSK_L2, action_lsk),
    RECT_BUTTON(FMC_BUTTON_LSK_L3, 6, 215, 48, 36, "L3", '\0', FMC_PAGE_INDEX, FMC_LSK_L3, action_lsk),
    RECT_BUTTON(FMC_BUTTON_LSK_L4, 6, 264, 48, 36, "L4", '\0', FMC_PAGE_INDEX, FMC_LSK_L4, action_lsk),
    RECT_BUTTON(FMC_BUTTON_LSK_L5, 6, 313, 48, 36, "L5", '\0', FMC_PAGE_INDEX, FMC_LSK_L5, action_lsk),
    RECT_BUTTON(FMC_BUTTON_LSK_L6, 6, 362, 48, 36, "L6", '\0', FMC_PAGE_INDEX, FMC_LSK_L6, action_lsk),
    RECT_BUTTON(FMC_BUTTON_LSK_R1, 586, 117, 48, 36, "R1", '\0', FMC_PAGE_INDEX, FMC_LSK_R1, action_lsk),
    RECT_BUTTON(FMC_BUTTON_LSK_R2, 586, 166, 48, 36, "R2", '\0', FMC_PAGE_INDEX, FMC_LSK_R2, action_lsk),
    RECT_BUTTON(FMC_BUTTON_LSK_R3, 586, 215, 48, 36, "R3", '\0', FMC_PAGE_INDEX, FMC_LSK_R3, action_lsk),
    RECT_BUTTON(FMC_BUTTON_LSK_R4, 586, 264, 48, 36, "R4", '\0', FMC_PAGE_INDEX, FMC_LSK_R4, action_lsk),
    RECT_BUTTON(FMC_BUTTON_LSK_R5, 586, 313, 48, 36, "R5", '\0', FMC_PAGE_INDEX, FMC_LSK_R5, action_lsk),
    RECT_BUTTON(FMC_BUTTON_LSK_R6, 586, 362, 48, 36, "R6", '\0', FMC_PAGE_INDEX, FMC_LSK_R6, action_lsk),

    RECT_BUTTON(FMC_BUTTON_TEXT, 263, 614, 49, 49, "A", 'A', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 332, 614, 49, 49, "B", 'B', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 401, 614, 49, 49, "C", 'C', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 468, 614, 49, 49, "D", 'D', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 535, 614, 49, 49, "E", 'E', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 263, 675, 49, 49, "F", 'F', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 332, 675, 49, 49, "G", 'G', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 401, 675, 49, 49, "H", 'H', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 468, 675, 49, 49, "I", 'I', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 535, 675, 49, 49, "J", 'J', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 263, 736, 49, 49, "K", 'K', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 332, 736, 49, 49, "L", 'L', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 401, 736, 49, 49, "M", 'M', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 468, 736, 49, 49, "N", 'N', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 535, 736, 49, 49, "O", 'O', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 263, 799, 49, 49, "P", 'P', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 332, 799, 49, 49, "Q", 'Q', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 401, 799, 49, 49, "R", 'R', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 468, 799, 49, 49, "S", 'S', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 535, 799, 49, 49, "T", 'T', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 263, 860, 49, 49, "U", 'U', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 332, 860, 49, 49, "V", 'V', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 401, 860, 49, 49, "W", 'W', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 468, 860, 49, 49, "X", 'X', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 535, 860, 49, 49, "Y", 'Y', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_TEXT, 263, 923, 49, 49, "Z", 'Z', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_DEL, 401, 923, 49, 49, "DEL", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, action_delete),
    RECT_BUTTON(FMC_BUTTON_TEXT, 468, 923, 49, 49, "/", '/', FMC_PAGE_INDEX, FMC_LSK_NONE, action_text),
    RECT_BUTTON(FMC_BUTTON_CLR, 535, 923, 49, 49, "CLR", '\0', FMC_PAGE_INDEX, FMC_LSK_NONE, action_clear),

    CIRCLE_BUTTON(FMC_BUTTON_TEXT, 87, 763, 25, "1", '1', action_text),
    CIRCLE_BUTTON(FMC_BUTTON_TEXT, 150, 763, 25, "2", '2', action_text),
    CIRCLE_BUTTON(FMC_BUTTON_TEXT, 213, 763, 25, "3", '3', action_text),
    CIRCLE_BUTTON(FMC_BUTTON_TEXT, 87, 824, 25, "4", '4', action_text),
    CIRCLE_BUTTON(FMC_BUTTON_TEXT, 150, 824, 25, "5", '5', action_text),
    CIRCLE_BUTTON(FMC_BUTTON_TEXT, 213, 824, 25, "6", '6', action_text),
    CIRCLE_BUTTON(FMC_BUTTON_TEXT, 87, 887, 25, "7", '7', action_text),
    CIRCLE_BUTTON(FMC_BUTTON_TEXT, 150, 887, 25, "8", '8', action_text),
    CIRCLE_BUTTON(FMC_BUTTON_TEXT, 213, 887, 25, "9", '9', action_text),
    CIRCLE_BUTTON(FMC_BUTTON_TEXT, 87, 949, 25, ".", '.', action_text),
    CIRCLE_BUTTON(FMC_BUTTON_TEXT, 150, 949, 25, "0", '0', action_text),
    CIRCLE_BUTTON(FMC_BUTTON_TEXT, 213, 949, 25, "+/-", '-', action_text),
};

#define FMC_BUTTON_COUNT ((int)(sizeof(FMC_BUTTONS) / sizeof(FMC_BUTTONS[0])))


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

    if (data->current_page == FMC_PAGE_CRUISE ||
        data->current_page == FMC_PAGE_DESCENT)
    {
        if (button->line_select >= FMC_LSK_L1 && button->line_select <= FMC_LSK_L3)
        {
            fmc_data_set_phase_parameter(data, button->line_select - FMC_LSK_L1 + 1);
            return;
        }
    }

    if (data->current_page == FMC_PAGE_DEP_ARR)
    {
        if (button->line_select >= FMC_LSK_L1 && button->line_select <= FMC_LSK_L3)
        {
            fmc_data_set_dep_arr_parameter(data, 0, button->line_select - FMC_LSK_L1 + 1);
            return;
        }
        if (button->line_select >= FMC_LSK_R1 && button->line_select <= FMC_LSK_R3)
        {
            fmc_data_set_dep_arr_parameter(data, 1, button->line_select - FMC_LSK_R1 + 1);
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
        return;
    }

    fmc_data_exec_route_selection(data);
}

static void action_prev_page(FMC_Data *data, const FMC_Button *button)
{
    (void)button;
    printf("FMC event: action_prev_page\n");
    if (data != NULL && data->current_page == FMC_PAGE_ROUTE)
    {
        fmc_data_route_prev_page(data);
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
    return FMC_BUTTON_COUNT;
}

const FMC_Button *fmc_key_button_at(int index)
{
    if (index < 0 || index >= FMC_BUTTON_COUNT)
    {
        return NULL;
    }

    return &FMC_BUTTONS[index];
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
    if (button == NULL || button->action == NULL)
    {
        return;
    }

    button->action(data, button);
}
