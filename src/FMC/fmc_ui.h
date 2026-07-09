#ifndef FMC_UI_H
#define FMC_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "fmc_data.h"

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

typedef enum FMC_ButtonId
{
    FMC_BUTTON_NONE = 0,
    FMC_BUTTON_INIT_REF,
    FMC_BUTTON_RTE,
    FMC_BUTTON_CLB,
    FMC_BUTTON_CRZ,
    FMC_BUTTON_DES,
    FMC_BUTTON_DEP_ARR,
    FMC_BUTTON_LEGS,
    FMC_BUTTON_HOLD,
    FMC_BUTTON_STATUS,
    FMC_BUTTON_EXEC,
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

typedef struct FMC_UI_State
{
    FMC_ButtonId hovered_button;
    int hovered_button_index;
} FMC_UI_State;

typedef struct FMC_UI_Assets
{
    SDL_Texture *panel_texture;
} FMC_UI_Assets;

int fmc_ui_assets_load(SDL_Renderer *renderer, FMC_UI_Assets *assets);
void fmc_ui_assets_destroy(FMC_UI_Assets *assets);

void fmc_ui_state_init(FMC_UI_State *state);
void fmc_ui_update_hover(SDL_Renderer *renderer, FMC_UI_State *state, int x, int y);
int fmc_ui_handle_mouse_button(SDL_Renderer *renderer, FMC_UI_State *state, FMC_Data *data, int x, int y);
void fmc_ui_update_hover_base(FMC_UI_State *state, int base_x, int base_y);
int fmc_ui_handle_mouse_button_base(FMC_UI_State *state, FMC_Data *data, int base_x, int base_y);

void fmc_ui_render(SDL_Renderer *renderer, TTF_Font *font, const FMC_UI_Assets *assets, const FMC_UI_State *state, const FMC_Data *data);
void fmc_ui_render_screen_only(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data, const SDL_Rect *screen_rect);
const FMC_Button *fmc_ui_hit_test_button(SDL_Renderer *renderer, int x, int y);

#endif
