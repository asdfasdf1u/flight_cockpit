#ifndef FMC_KEY_H
#define FMC_KEY_H

#include <SDL2/SDL.h>

#include "fmc_ui_adapter.h"
//行选键枚举
typedef enum FMC_LineSelectKey
{
    FMC_LSK_NONE = 0,
    FMC_LSK_L1,
    FMC_LSK_L2,
    FMC_LSK_L3,
    FMC_LSK_L4,
    FMC_LSK_L5,
    FMC_LSK_L6,
    FMC_LSK_R1,
    FMC_LSK_R2,
    FMC_LSK_R3,
    FMC_LSK_R4,
    FMC_LSK_R5,
    FMC_LSK_R6
} FMC_LineSelectKey;
//功能按键枚举
typedef enum FMC_ButtonId
{
    FMC_BUTTON_NONE = 0,
    FMC_BUTTON_INIT_REF,
    FMC_BUTTON_RTE,
    FMC_BUTTON_CLB,
    FMC_BUTTON_CRZ,
    FMC_BUTTON_DES,
    FMC_BUTTON_DIR_INTC,
    FMC_BUTTON_DEP_ARR,
    FMC_BUTTON_LEGS,
    FMC_BUTTON_HOLD,
    FMC_BUTTON_STATUS,
    FMC_BUTTON_EXEC,
    FMC_BUTTON_FIX,
    FMC_BUTTON_NAV_RAD,
    FMC_BUTTON_PREV_PAGE,
    FMC_BUTTON_NEXT_PAGE,
    FMC_BUTTON_CLR,
    FMC_BUTTON_DEL,
    FMC_BUTTON_LSK_L1,
    FMC_BUTTON_LSK_L2,
    FMC_BUTTON_LSK_L3,
    FMC_BUTTON_LSK_L4,
    FMC_BUTTON_LSK_L5,
    FMC_BUTTON_LSK_L6,
    FMC_BUTTON_LSK_R1,
    FMC_BUTTON_LSK_R2,
    FMC_BUTTON_LSK_R3,
    FMC_BUTTON_LSK_R4,
    FMC_BUTTON_LSK_R5,
    FMC_BUTTON_LSK_R6,
    FMC_BUTTON_TEXT
} FMC_ButtonId;
//按键形状枚举
typedef enum FMC_ButtonShape
{
    FMC_BUTTON_SHAPE_RECT,
    FMC_BUTTON_SHAPE_CIRCLE
} FMC_ButtonShape;

typedef struct FMC_Button FMC_Button;
typedef void (*FMC_ButtonAction)(FMC_Data *data, const FMC_Button *button);

struct FMC_Button
{
    FMC_ButtonId id;
    FMC_ButtonShape shape;
    SDL_Rect rect;
    SDL_Point center;
    int radius;
    const char *label;
    char input_char;
    FMC_Page page;
    FMC_LineSelectKey line_select;
    FMC_ButtonAction action;
};

int fmc_key_button_count(void);
int fmc_buttons_load(void);
const FMC_Button *fmc_key_button_at(int index);
int fmc_key_button_contains_base_point(const FMC_Button *button, int x, int y);
int fmc_key_is_page_button(const FMC_Button *button);
void fmc_key_activate_button(FMC_Data *data, const FMC_Button *button);

#endif
