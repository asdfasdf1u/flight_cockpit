#ifndef FMC_KEY_H
#define FMC_KEY_H

#include <SDL2/SDL.h>

#include "fmc_ui_adapter.h"

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

typedef enum FMC_Key
{
    FMC_KEY_NONE = 0,
    FMC_KEY_INIT_REF,
    FMC_KEY_RTE,
    FMC_KEY_CLB,
    FMC_KEY_CRZ,
    FMC_KEY_DES,
    FMC_KEY_DIR_INTC,
    FMC_KEY_DEP_ARR,
    FMC_KEY_LEGS,
    FMC_KEY_HOLD,
    FMC_KEY_STATUS,
    FMC_KEY_EXEC,
    FMC_KEY_FIX,
    FMC_KEY_NAV_RAD,
    FMC_KEY_PREV_PAGE,
    FMC_KEY_NEXT_PAGE,
    FMC_KEY_CLR,
    FMC_KEY_DEL,
    FMC_KEY_SCREEN_L1,
    FMC_KEY_SCREEN_L2,
    FMC_KEY_SCREEN_L3,
    FMC_KEY_SCREEN_L4,
    FMC_KEY_SCREEN_L5,
    FMC_KEY_SCREEN_L6,
    FMC_KEY_SCREEN_R1,
    FMC_KEY_SCREEN_R2,
    FMC_KEY_SCREEN_R3,
    FMC_KEY_SCREEN_R4,
    FMC_KEY_SCREEN_R5,
    FMC_KEY_SCREEN_R6,
    FMC_KEY_TEXT,

    FMC_BUTTON_NONE = FMC_KEY_NONE,
    FMC_BUTTON_INIT_REF = FMC_KEY_INIT_REF,
    FMC_BUTTON_RTE = FMC_KEY_RTE,
    FMC_BUTTON_CLB = FMC_KEY_CLB,
    FMC_BUTTON_CRZ = FMC_KEY_CRZ,
    FMC_BUTTON_DES = FMC_KEY_DES,
    FMC_BUTTON_DIR_INTC = FMC_KEY_DIR_INTC,
    FMC_BUTTON_DEP_ARR = FMC_KEY_DEP_ARR,
    FMC_BUTTON_LEGS = FMC_KEY_LEGS,
    FMC_BUTTON_HOLD = FMC_KEY_HOLD,
    FMC_BUTTON_STATUS = FMC_KEY_STATUS,
    FMC_BUTTON_EXEC = FMC_KEY_EXEC,
    FMC_BUTTON_FIX = FMC_KEY_FIX,
    FMC_BUTTON_NAV_RAD = FMC_KEY_NAV_RAD,
    FMC_BUTTON_PREV_PAGE = FMC_KEY_PREV_PAGE,
    FMC_BUTTON_NEXT_PAGE = FMC_KEY_NEXT_PAGE,
    FMC_BUTTON_CLR = FMC_KEY_CLR,
    FMC_BUTTON_DEL = FMC_KEY_DEL,
    FMC_BUTTON_LSK_L1 = FMC_KEY_SCREEN_L1,
    FMC_BUTTON_LSK_L2 = FMC_KEY_SCREEN_L2,
    FMC_BUTTON_LSK_L3 = FMC_KEY_SCREEN_L3,
    FMC_BUTTON_LSK_L4 = FMC_KEY_SCREEN_L4,
    FMC_BUTTON_LSK_L5 = FMC_KEY_SCREEN_L5,
    FMC_BUTTON_LSK_L6 = FMC_KEY_SCREEN_L6,
    FMC_BUTTON_LSK_R1 = FMC_KEY_SCREEN_R1,
    FMC_BUTTON_LSK_R2 = FMC_KEY_SCREEN_R2,
    FMC_BUTTON_LSK_R3 = FMC_KEY_SCREEN_R3,
    FMC_BUTTON_LSK_R4 = FMC_KEY_SCREEN_R4,
    FMC_BUTTON_LSK_R5 = FMC_KEY_SCREEN_R5,
    FMC_BUTTON_LSK_R6 = FMC_KEY_SCREEN_R6,
    FMC_BUTTON_TEXT = FMC_KEY_TEXT
} FMC_Key;

typedef FMC_Key FMC_ButtonId;

typedef enum FMC_ButtonShape
{
    FMC_SHAPE_RECT,
    FMC_SHAPE_CIRCLE,

    FMC_BUTTON_SHAPE_RECT = FMC_SHAPE_RECT,
    FMC_BUTTON_SHAPE_CIRCLE = FMC_SHAPE_CIRCLE
} FMC_ButtonShape;

typedef struct FMC_Button FMC_Button;
typedef int (*FMC_ButtonHandler)(void);
typedef void (*FMC_ButtonAction)(FMC_Data *data, const FMC_Button *button);

struct FMC_Button
{
    FMC_Key key;
    FMC_ButtonShape shape;
    SDL_Rect rect;
    const char *label;
    FMC_ButtonHandler on_click;

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
