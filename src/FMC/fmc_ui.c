#include "fmc_ui.h"

#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define FMC_BASE_WIDTH 638
#define FMC_BASE_HEIGHT 998

typedef struct FMC_Layout
{
    SDL_Rect viewport;
    float scale;
} FMC_Layout;

static const SDL_Color COLOR_BG = {5, 8, 10, 255};
static const SDL_Color COLOR_SCREEN = {0, 0, 0, 255};
static const SDL_Color COLOR_TEXT = {112, 255, 180, 255};
static const SDL_Color COLOR_DIM = {82, 166, 128, 255};
static const SDL_Color COLOR_CYAN = {64, 225, 255, 255};
static const SDL_Color COLOR_WHITE = {235, 244, 232, 255};
static const SDL_Color COLOR_AMBER = {255, 185, 95, 255};
static const SDL_Color COLOR_HOVER = {255, 255, 255, 72};

static const SDL_Rect FMC_SCREEN_RECT = {104, 74, 435, 345};
static const SDL_Rect FMC_SCRATCHPAD_RECT = {145, 388, 350, 28};

#define FMC_BOX_PLACEHOLDER "\xE2\x96\xA1\xE2\x96\xA1\xE2\x96\xA1\xE2\x96\xA1"

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

static void set_color(SDL_Renderer *renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
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

static FMC_Layout get_layout(SDL_Renderer *renderer)
{
    int width = FMC_BASE_WIDTH;
    int height = FMC_BASE_HEIGHT;
    SDL_GetRendererOutputSize(renderer, &width, &height);

    float scale_x = (float)width / (float)FMC_BASE_WIDTH;
    float scale_y = (float)height / (float)FMC_BASE_HEIGHT;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale <= 0.0f)
    {
        scale = 1.0f;
    }

    int viewport_w = (int)((float)FMC_BASE_WIDTH * scale + 0.5f);
    int viewport_h = (int)((float)FMC_BASE_HEIGHT * scale + 0.5f);

    FMC_Layout layout;
    layout.viewport.x = (width - viewport_w) / 2;
    layout.viewport.y = (height - viewport_h) / 2;
    layout.viewport.w = viewport_w;
    layout.viewport.h = viewport_h;
    layout.scale = scale;
    return layout;
}

static FMC_Layout get_screen_only_layout(const SDL_Rect *screen_rect)
{
    SDL_Rect target = FMC_SCREEN_RECT;
    if (screen_rect != NULL && screen_rect->w > 0 && screen_rect->h > 0)
    {
        target = *screen_rect;
    }

    float scale_x = (float)target.w / (float)FMC_SCREEN_RECT.w;
    float scale_y = (float)target.h / (float)FMC_SCREEN_RECT.h;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale <= 0.0f)
    {
        scale = 1.0f;
    }

    const int mapped_screen_w = (int)((float)FMC_SCREEN_RECT.w * scale + 0.5f);
    const int mapped_screen_h = (int)((float)FMC_SCREEN_RECT.h * scale + 0.5f);

    FMC_Layout layout;
    layout.viewport.x = target.x + (target.w - mapped_screen_w) / 2 - (int)((float)FMC_SCREEN_RECT.x * scale + 0.5f);
    layout.viewport.y = target.y + (target.h - mapped_screen_h) / 2 - (int)((float)FMC_SCREEN_RECT.y * scale + 0.5f);
    layout.viewport.w = (int)((float)FMC_BASE_WIDTH * scale + 0.5f);
    layout.viewport.h = (int)((float)FMC_BASE_HEIGHT * scale + 0.5f);
    layout.scale = scale;
    return layout;
}

static SDL_Rect scale_rect(const FMC_Layout *layout, const SDL_Rect *rect)
{
    SDL_Rect scaled = {
        layout->viewport.x + (int)((float)rect->x * layout->scale + 0.5f),
        layout->viewport.y + (int)((float)rect->y * layout->scale + 0.5f),
        (int)((float)rect->w * layout->scale + 0.5f),
        (int)((float)rect->h * layout->scale + 0.5f)};

    if (scaled.w < 1)
    {
        scaled.w = 1;
    }
    if (scaled.h < 1)
    {
        scaled.h = 1;
    }
    return scaled;
}

static int window_to_base(SDL_Renderer *renderer, int x, int y, int *base_x, int *base_y)
{
    FMC_Layout layout = get_layout(renderer);
    if (x < layout.viewport.x ||
        x >= layout.viewport.x + layout.viewport.w ||
        y < layout.viewport.y ||
        y >= layout.viewport.y + layout.viewport.h)
    {
        return 0;
    }

    if (base_x != NULL)
    {
        *base_x = (int)((float)(x - layout.viewport.x) / layout.scale);
    }
    if (base_y != NULL)
    {
        *base_y = (int)((float)(y - layout.viewport.y) / layout.scale);
    }
    return 1;
}

static void draw_scaled_rect(SDL_Renderer *renderer, const FMC_Layout *layout, const SDL_Rect *rect, SDL_Color color)
{
    SDL_Rect dest = scale_rect(layout, rect);
    set_color(renderer, color);
    SDL_RenderFillRect(renderer, &dest);
}

static Uint32 gfx_color(SDL_Color color);

static void draw_scaled_outline(SDL_Renderer *renderer, const FMC_Layout *layout, const SDL_Rect *rect, SDL_Color color)
{
    SDL_Rect dest = scale_rect(layout, rect);
    rectangleColor(renderer,
                   (Sint16)dest.x,
                   (Sint16)dest.y,
                   (Sint16)(dest.x + dest.w),
                   (Sint16)(dest.y + dest.h),
                   gfx_color(color));
}

static Uint32 gfx_color(SDL_Color color)
{
    return ((Uint32)color.r << 24) |
           ((Uint32)color.g << 16) |
           ((Uint32)color.b << 8) |
           (Uint32)color.a;
}

static void draw_scaled_circle_outline(SDL_Renderer *renderer, const FMC_Layout *layout, SDL_Point center, int radius, SDL_Color color)
{
    const int scaled_x = layout->viewport.x + (int)((float)center.x * layout->scale + 0.5f);
    const int scaled_y = layout->viewport.y + (int)((float)center.y * layout->scale + 0.5f);
    int scaled_radius = (int)((float)radius * layout->scale + 0.5f);
    if (scaled_radius < 1)
    {
        scaled_radius = 1;
    }

    set_color(renderer, color);
    int x = scaled_radius;
    int y = 0;
    int decision = 1 - x;
    while (y <= x)
    {
        SDL_RenderDrawPoint(renderer, scaled_x + x, scaled_y + y);
        SDL_RenderDrawPoint(renderer, scaled_x + y, scaled_y + x);
        SDL_RenderDrawPoint(renderer, scaled_x - y, scaled_y + x);
        SDL_RenderDrawPoint(renderer, scaled_x - x, scaled_y + y);
        SDL_RenderDrawPoint(renderer, scaled_x - x, scaled_y - y);
        SDL_RenderDrawPoint(renderer, scaled_x - y, scaled_y - x);
        SDL_RenderDrawPoint(renderer, scaled_x + y, scaled_y - x);
        SDL_RenderDrawPoint(renderer, scaled_x + x, scaled_y - y);

        ++y;
        if (decision <= 0)
        {
            decision += 2 * y + 1;
        }
        else
        {
            --x;
            decision += 2 * (y - x) + 1;
        }
    }
}

static void draw_button_feedback(SDL_Renderer *renderer, const FMC_Layout *layout, const FMC_Button *button, SDL_Color color)
{
    if (button == NULL || button->id == FMC_BUTTON_NONE)
    {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (button->shape == FMC_BUTTON_SHAPE_RECT)
    {
        draw_scaled_rect(renderer, layout, &button->rect, color);
        draw_scaled_outline(renderer, layout, &button->rect, COLOR_CYAN);
    }
    else
    {
        draw_scaled_circle_outline(renderer, layout, button->center, button->radius, COLOR_CYAN);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static void draw_text(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, SDL_Color color, int x, int y, const char *format, ...)
{
    if (renderer == NULL || font == NULL || layout == NULL || format == NULL)
    {
        return;
    }

    char text[180];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (surface == NULL)
    {
        return;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL)
    {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dest = {
        layout->viewport.x + (int)((float)x * layout->scale + 0.5f),
        layout->viewport.y + (int)((float)y * layout->scale + 0.5f),
        (int)((float)surface->w * layout->scale + 0.5f),
        (int)((float)surface->h * layout->scale + 0.5f)};
    if (dest.w < 1)
    {
        dest.w = 1;
    }
    if (dest.h < 1)
    {
        dest.h = 1;
    }

    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

static void draw_centered_text(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, SDL_Color color, const SDL_Rect *rect, const char *format, ...)
{
    if (renderer == NULL || font == NULL || layout == NULL || rect == NULL || format == NULL)
    {
        return;
    }

    char text[180];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    int text_w = 0;
    int text_h = 0;
    if (TTF_SizeUTF8(font, text, &text_w, &text_h) != 0)
    {
        return;
    }

    draw_text(renderer, font, layout, color, rect->x + (rect->w - text_w) / 2, rect->y + (rect->h - text_h) / 2, "%s", text);
}

static SDL_Texture *load_texture_from_candidates(SDL_Renderer *renderer)
{
    const char *paths[] = {
        "assets/fmc.png",
        "../assets/fmc.png",
        "../../assets/fmc.png",
        NULL};

    for (int i = 0; paths[i] != NULL; ++i)
    {
        SDL_Texture *texture = IMG_LoadTexture(renderer, paths[i]);
        if (texture != NULL)
        {
            return texture;
        }
    }

    return NULL;
}

static void draw_screen_base(SDL_Renderer *renderer, const FMC_Layout *layout)
{
    SDL_Rect screen = scale_rect(layout, &FMC_SCREEN_RECT);
    int radius = (int)(28.0f * layout->scale + 0.5f);
    if (radius < 4)
    {
        radius = 4;
    }
    roundedBoxColor(renderer,
                    (Sint16)screen.x,
                    (Sint16)screen.y,
                    (Sint16)(screen.x + screen.w),
                    (Sint16)(screen.y + screen.h),
                    (Sint16)radius,
                    gfx_color(COLOR_SCREEN));
}

static void action_page(FMC_Data *data, const FMC_Button *button)
{
    if (data == NULL || button == NULL)
    {
        return;
    }

    fmc_data_set_page(data, button->page);
}

static void action_lsk(FMC_Data *data, const FMC_Button *button)
{
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
            fmc_data_clear_scratchpad(data);
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
    if (data == NULL || button == NULL || button->input_char == '\0')
    {
        return;
    }

    fmc_data_append_char(data, button->input_char);
}

static void action_delete(FMC_Data *data, const FMC_Button *button)
{
    (void)button;
    fmc_data_backspace(data);
}

static void action_clear(FMC_Data *data, const FMC_Button *button)
{
    (void)button;
    fmc_data_clear_scratchpad(data);
}

static void action_exec(FMC_Data *data, const FMC_Button *button)
{
    (void)button;
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
    if (data != NULL && data->current_page == FMC_PAGE_ROUTE)
    {
        fmc_data_route_prev_page(data);
    }
}

static void action_next_page(FMC_Data *data, const FMC_Button *button)
{
    (void)button;
    if (data != NULL && data->current_page == FMC_PAGE_ROUTE)
    {
        fmc_data_route_next_page(data);
    }
}

static void draw_lsk_marks(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout)
{
    (void)renderer;
    (void)font;
    (void)layout;
}

static void draw_right_text(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, SDL_Color color, int right_x, int y, const char *format, ...)
{
    if (renderer == NULL || font == NULL || layout == NULL || format == NULL)
    {
        return;
    }

    char text[180];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    int text_w = 0;
    int text_h = 0;
    if (TTF_SizeUTF8(font, text, &text_w, &text_h) != 0)
    {
        return;
    }

    draw_text(renderer, font, layout, color, right_x - text_w, y, "%s", text);
}

static void draw_fmc_header(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const char *title, const char *subtitle, const char *page)
{
    draw_centered_text(renderer, font, layout, COLOR_TEXT, &(SDL_Rect){104, 88, 435, 22}, "%s", title);
    if (subtitle != NULL && subtitle[0] != '\0')
    {
        draw_centered_text(renderer, font, layout, COLOR_TEXT, &(SDL_Rect){104, 106, 435, 20}, "%s", subtitle);
    }
    if (page != NULL && page[0] != '\0')
    {
        draw_text(renderer, font, layout, COLOR_TEXT, 474, 92, "%s", page);
    }
}

static void draw_fmc_dashed_separator(SDL_Renderer *renderer, const FMC_Layout *layout)
{
    for (int x = 128; x <= 512; x += 11)
    {
        draw_scaled_rect(renderer, layout, &(SDL_Rect){x, 356, 6, 3}, COLOR_CYAN);
    }
}

static void draw_fmc_softkeys(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const char *left, const char *right)
{
    draw_fmc_dashed_separator(renderer, layout);
    if (left != NULL && left[0] != '\0')
    {
        draw_text(renderer, font, layout, COLOR_TEXT, 126, 368, "%s", left);
    }
    if (right != NULL && right[0] != '\0')
    {
        draw_right_text(renderer, font, layout, COLOR_TEXT, 514, 368, "%s", right);
    }
}

static void draw_list_option(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, int x, int y, const char *value, const char *selected_value)
{
    if (selected_value != NULL && selected_value[0] != '\0' && strcmp(value, selected_value) == 0)
    {
        draw_text(renderer, font, layout, COLOR_WHITE, x, y, "%s <SEL>", value);
    }
    else
    {
        draw_text(renderer, font, layout, COLOR_WHITE, x, y, "%s", value);
    }
}

static void draw_home_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    draw_fmc_header(renderer, font, layout, "DEP/ARR INDEX", "ACT FPLN", "");
    draw_text(renderer, font, layout, COLOR_WHITE, 128, 132, "<DEP");
    draw_text(renderer, font, layout, COLOR_WHITE, 298, 132, "%s", data->origin[0] ? data->origin : FMC_BOX_PLACEHOLDER);
    draw_right_text(renderer, font, layout, COLOR_WHITE, 518, 132, "ARR>");

    draw_text(renderer, font, layout, COLOR_WHITE, 312, 180, "%s", data->destination[0] ? data->destination : FMC_BOX_PLACEHOLDER);
    draw_right_text(renderer, font, layout, COLOR_WHITE, 518, 180, "ARR>");
}

static void draw_route_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    const int page_count = fmc_data_route_page_count(data);
    int page_index = data->configured_route_page;
    char page_text[16];
    const int route_rows = FMC_RTE_PAGE_SIZE;

    if (page_index < 0)
    {
        page_index = 0;
    }
    if (page_index >= page_count)
    {
        page_index = page_count - 1;
    }

    snprintf(page_text, sizeof(page_text), "RTE %d/%d", page_index + 1, page_count);
    draw_fmc_header(renderer, font, layout, "ACT FPLN", "", page_text);

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 116, "ORIGIN");
    draw_text(renderer, font, layout, COLOR_WHITE, 126, 138, "%s", data->origin[0] ? data->origin : FMC_BOX_PLACEHOLDER);
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 116, "DEST");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 138, "%s", data->destination[0] ? data->destination : FMC_BOX_PLACEHOLDER);

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 162, "FLT NO");
    draw_text(renderer, font, layout, COLOR_WHITE, 126, 184, "%s", data->flight_no[0] ? data->flight_no : "----");
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 162, "CRZ ALT");
    if (data->cruise_altitude > 0)
    {
        draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 184, "FL%03d", data->cruise_altitude / 100);
    }
    else
    {
        draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 184, "----");
    }

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 214, "VIA");
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 214, "TO");

    const int start_index = page_index * FMC_RTE_PAGE_SIZE;
    const int row_y = 236;
    const int row_spacing = 24;
    for (int row = 0; row < route_rows; ++row)
    {
        const int route_index = start_index + row;
        const char *ident = "----";

        if (route_index < data->route_count && data->route_points[route_index][0] != '\0')
        {
            ident = data->route_points[route_index];
        }

        draw_text(renderer, font, layout, COLOR_DIM, 126, row_y + row * row_spacing, "%02d DIRECT", route_index + 1);
        draw_right_text(renderer, font, layout, COLOR_WHITE, 510, row_y + row * row_spacing, "%s", ident);
    }

    draw_fmc_softkeys(renderer, font, layout, "<ROUTE MENU", "VNAV>");
}

static void draw_dep_arr_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    draw_fmc_header(renderer, font, layout, "DEP/ARR", "ACT FPLN", "1/1");

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 116, "DEP AIRPORT");
    draw_text(renderer, font, layout, COLOR_WHITE, 126, 138, "%s", data->origin[0] ? data->origin : FMC_BOX_PLACEHOLDER);
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 116, "ARR AIRPORT");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 138, "%s", data->destination[0] ? data->destination : FMC_BOX_PLACEHOLDER);

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 162, "RUNWAY");
    draw_text(renderer, font, layout, COLOR_WHITE, 126, 184, "%s", data->departure_runway[0] ? data->departure_runway : "----");
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 162, "RUNWAY");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 184, "%s", data->arrival_runway[0] ? data->arrival_runway : "----");

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 214, "SID");
    draw_text(renderer, font, layout, COLOR_WHITE, 126, 236, "%s", data->departure_procedure[0] ? data->departure_procedure : "----");
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 214, "STAR");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 236, "%s", data->arrival_procedure[0] ? data->arrival_procedure : "----");

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 266, "DEP TRANS");
    draw_text(renderer, font, layout, COLOR_WHITE, 126, 288, "%s", data->departure_transition[0] ? data->departure_transition : "----");
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 266, "APPROACH");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 288, "%s", data->arrival_transition[0] ? data->arrival_transition : "----");

    draw_fmc_softkeys(renderer, font, layout, "", "");
}

static void draw_perf_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    draw_fmc_header(renderer, font, layout, "PERF INIT", "ACT FPLN", "1/1");

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 116, "CRZ ALT");
    if (data->cruise_altitude > 0)
    {
        draw_text(renderer, font, layout, COLOR_WHITE, 126, 138, "FL%03d", data->cruise_altitude / 100);
    }
    else
    {
        draw_text(renderer, font, layout, COLOR_WHITE, 126, 138, "----");
    }

    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 116, "TGT SPEED");
    if (data->target_speed > 0)
    {
        draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 138, "%dKT", data->target_speed);
    }
    else
    {
        draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 138, "----");
    }

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 178, "COST INDEX");
    draw_text(renderer, font, layout, COLOR_WHITE, 126, 200, "%.0f", data->cost_index);

    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 178, "ROUTE");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 200, "%s-%s",
                    data->origin[0] ? data->origin : FMC_BOX_PLACEHOLDER,
                    data->destination[0] ? data->destination : FMC_BOX_PLACEHOLDER);

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 240, "RESERVES");
    draw_text(renderer, font, layout, COLOR_WHITE, 126, 262, "----");
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 240, "STEP SIZE");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 262, "----");

    draw_fmc_softkeys(renderer, font, layout, "<INDEX", "CLB>");
}

static void draw_climb_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    draw_fmc_header(renderer, font, layout, "ACT VNAV CLIMB", "", "1/3");

    draw_text(renderer, font, layout, COLOR_TEXT, 132, 124, "TGT SPEED");
    draw_text(renderer, font, layout, COLOR_WHITE, 132, 150, "%s", data->climb_target_speed_text[0] ? data->climb_target_speed_text : "----");
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 124, "TRANS ALT");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 150, "%s", data->climb_transition_alt_text[0] ? data->climb_transition_alt_text : "----");

    draw_text(renderer, font, layout, COLOR_TEXT, 132, 190, "SPD/ALT LIMIT");
    draw_text(renderer, font, layout, COLOR_WHITE, 132, 216, "%s", data->climb_spd_alt_limit_text[0] ? data->climb_spd_alt_limit_text : "----");
    draw_text(renderer, font, layout, COLOR_DIM, 132, 260, "--/-----");

    draw_fmc_softkeys(renderer, font, layout, "<RTE", "CRZ>");
}

static void draw_cruise_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    draw_fmc_header(renderer, font, layout, "VNAV CRUISE", "", "2/3");

    draw_text(renderer, font, layout, COLOR_TEXT, 132, 124, "TGT SPEED");
    draw_text(renderer, font, layout, COLOR_WHITE, 132, 150, "%dKT", data->cruise_speed);

    draw_text(renderer, font, layout, COLOR_TEXT, 132, 190, "CRZ ALT");
    draw_text(renderer, font, layout, COLOR_WHITE, 132, 216, "FL%03d", data->cruise_altitude / 100);

    draw_text(renderer, font, layout, COLOR_TEXT, 132, 256, "COST INDEX");
    draw_text(renderer, font, layout, COLOR_WHITE, 132, 282, "%.0f", data->cost_index);

    draw_fmc_softkeys(renderer, font, layout, "<CLB", "DES>");
}

static void draw_descent_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    draw_fmc_header(renderer, font, layout, "VNAV DESCENT", "", "3/3");

    draw_text(renderer, font, layout, COLOR_TEXT, 132, 124, "TGT SPEED");
    draw_text(renderer, font, layout, COLOR_WHITE, 132, 150, "%dKT", data->descent_speed);

    draw_text(renderer, font, layout, COLOR_TEXT, 132, 190, "TRANS LVL");
    draw_text(renderer, font, layout, COLOR_WHITE, 132, 216, "%d", data->descent_transition_level);

    draw_text(renderer, font, layout, COLOR_TEXT, 132, 256, "DES V/S");
    draw_text(renderer, font, layout, COLOR_WHITE, 132, 282, "%dFPM", data->descent_vertical_speed);

    draw_fmc_softkeys(renderer, font, layout, "<CRZ", "");
}

static void draw_legs_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    draw_text(renderer, font, layout, COLOR_TEXT, 132, 92, "ACT LEGS");
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 124, "SEQUENCE");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 150, "%s", data->legs_sequence[0] ? data->legs_sequence : "AUTO/INHIBIT");
    draw_fmc_softkeys(renderer, font, layout, "", "");
}

static void draw_hold_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    draw_fmc_header(renderer, font, layout, "HOLD", "ACT FPLN", "1/1");

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 116, "HOLD FIX");
    draw_text(renderer, font, layout, COLOR_WHITE, 126, 138, "%s", data->hold_fix[0] ? data->hold_fix : "----");
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 116, "SPD/ALT");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 138, "%s", data->hold_speed_altitude[0] ? data->hold_speed_altitude : "----");

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 178, "INBD CRS");
    draw_text(renderer, font, layout, COLOR_WHITE, 126, 200, "%s", data->hold_inbound_course[0] ? data->hold_inbound_course : "----");

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 240, "TURN DIR");
    draw_text(renderer, font, layout, COLOR_WHITE, 126, 262, "%s", data->hold_turn_direction[0] ? data->hold_turn_direction : "----");

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 302, "LEG TIME");
    draw_text(renderer, font, layout, COLOR_WHITE, 126, 324, "%s", data->hold_leg_time[0] ? data->hold_leg_time : "----");

    draw_fmc_softkeys(renderer, font, layout, "", "");
}

static void draw_status_bar(SDL_Renderer *renderer, const FMC_Layout *layout, int x, int y, int width, int percent)
{
    if (percent < 0)
    {
        percent = 0;
    }
    if (percent > 100)
    {
        percent = 100;
    }

    const SDL_Rect outline = {x, y, width, 12};
    const SDL_Rect fill = {x + 2, y + 2, (width - 4) * percent / 100, 8};
    draw_scaled_outline(renderer, layout, &outline, COLOR_DIM);
    draw_scaled_rect(renderer, layout, &fill, percent >= 80 ? COLOR_TEXT : COLOR_AMBER);
}

static void draw_status_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    (void)data;
    draw_centered_text(renderer, font, layout, COLOR_TEXT, &(SDL_Rect){104, 92, 435, 24}, "PROGRESS");
    draw_fmc_softkeys(renderer, font, layout, "", "");
}

typedef void (*FMC_PageRenderFn)(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data);

static FMC_PageRenderFn page_renderer(FMC_Page page)
{
    static const FMC_PageRenderFn renderers[FMC_PAGE_COUNT] = {
        draw_home_page,
        draw_route_page,
        draw_dep_arr_page,
        draw_perf_page,
        draw_climb_page,
        draw_cruise_page,
        draw_descent_page,
        draw_legs_page,
        draw_hold_page,
        draw_status_page};

    if (page < FMC_PAGE_HOME || page >= FMC_PAGE_COUNT || renderers[page] == NULL)
    {
        return draw_home_page;
    }

    return renderers[page];
}

static void draw_page_content(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    draw_lsk_marks(renderer, font, layout);
    page_renderer(data->current_page)(renderer, font, layout, data);
}

static void draw_scratchpad(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    if (data->scratchpad_len > 0)
    {
        draw_text(renderer, font, layout, COLOR_WHITE, FMC_SCRATCHPAD_RECT.x + 20, FMC_SCRATCHPAD_RECT.y, "%s", data->scratchpad);
    }
    else if (data->message[0] != '\0')
    {
        draw_text(renderer, font, layout, COLOR_AMBER, FMC_SCRATCHPAD_RECT.x + 20, FMC_SCRATCHPAD_RECT.y, "%s", data->message);
    }
    else if (data->current_page == FMC_PAGE_LEGS ||
             data->current_page == FMC_PAGE_HOLD ||
             data->current_page == FMC_PAGE_STATUS)
    {
        draw_text(renderer, font, layout, COLOR_WHITE, FMC_SCRATCHPAD_RECT.x + 20, FMC_SCRATCHPAD_RECT.y, "DELETE");
    }
}

static void draw_active_button(SDL_Renderer *renderer, const FMC_Layout *layout, FMC_Page current_page)
{
    for (int i = 0; i < FMC_BUTTON_COUNT; ++i)
    {
        if (FMC_BUTTONS[i].action == action_page && FMC_BUTTONS[i].page == current_page)
        {
            draw_scaled_outline(renderer, layout, &FMC_BUTTONS[i].rect, COLOR_CYAN);
            draw_scaled_outline(renderer, layout, &(SDL_Rect){
                                              FMC_BUTTONS[i].rect.x + 2,
                                              FMC_BUTTONS[i].rect.y + 2,
                                              FMC_BUTTONS[i].rect.w - 4,
                                              FMC_BUTTONS[i].rect.h - 4},
                                COLOR_CYAN);
            return;
        }
    }
}

static void draw_hover_button(SDL_Renderer *renderer, const FMC_Layout *layout, const FMC_UI_State *state)
{
    if (state == NULL || state->hovered_button_index < 0 || state->hovered_button_index >= FMC_BUTTON_COUNT)
    {
        return;
    }

    draw_button_feedback(renderer, layout, &FMC_BUTTONS[state->hovered_button_index], COLOR_HOVER);
}

int fmc_ui_assets_load(SDL_Renderer *renderer, FMC_UI_Assets *assets)
{
    if (renderer == NULL || assets == NULL)
    {
        return 0;
    }

    memset(assets, 0, sizeof(*assets));
    assets->panel_texture = load_texture_from_candidates(renderer);
    if (assets->panel_texture == NULL)
    {
        printf("IMG_LoadTexture fmc.png failed: %s\n", IMG_GetError());
        return 0;
    }

    return 1;
}

void fmc_ui_assets_destroy(FMC_UI_Assets *assets)
{
    if (assets == NULL)
    {
        return;
    }

    if (assets->panel_texture != NULL)
    {
        SDL_DestroyTexture(assets->panel_texture);
        assets->panel_texture = NULL;
    }
}

void fmc_ui_render(SDL_Renderer *renderer, TTF_Font *font, const FMC_UI_Assets *assets, const FMC_UI_State *state, const FMC_Data *data)
{
    if (renderer == NULL || font == NULL || data == NULL)
    {
        return;
    }

    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    set_color(renderer, COLOR_BG);
    SDL_RenderFillRect(renderer, &(SDL_Rect){0, 0, width, height});

    FMC_Layout layout = get_layout(renderer);
    if (assets != NULL && assets->panel_texture != NULL)
    {
        SDL_RenderCopy(renderer, assets->panel_texture, NULL, &layout.viewport);
    }
    else
    {
        draw_scaled_rect(renderer, &layout, &(SDL_Rect){0, 0, FMC_BASE_WIDTH, FMC_BASE_HEIGHT}, (SDL_Color){23, 30, 32, 255});
    }

    draw_screen_base(renderer, &layout);
    draw_page_content(renderer, font, &layout, data);
    draw_scratchpad(renderer, font, &layout, data);
    draw_active_button(renderer, &layout, data->current_page);
    draw_hover_button(renderer, &layout, state);
}

void fmc_ui_render_screen_only(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data, const SDL_Rect *screen_rect)
{
    if (renderer == NULL || font == NULL || data == NULL || screen_rect == NULL)
    {
        return;
    }

    FMC_Layout layout = get_screen_only_layout(screen_rect);
    SDL_Rect clip = *screen_rect;
    SDL_RenderSetClipRect(renderer, &clip);
    draw_page_content(renderer, font, &layout, data);
    draw_scratchpad(renderer, font, &layout, data);
    SDL_RenderSetClipRect(renderer, NULL);
}

static int button_contains_base_point(const FMC_Button *button, int x, int y)
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

static int hit_test_button_index(SDL_Renderer *renderer, int x, int y)
{
    int base_x = 0;
    int base_y = 0;
    if (!window_to_base(renderer, x, y, &base_x, &base_y))
    {
        return -1;
    }

    for (int i = 0; i < FMC_BUTTON_COUNT; ++i)
    {
        if (button_contains_base_point(&FMC_BUTTONS[i], base_x, base_y))
        {
            return i;
        }
    }

    return -1;
}

static int hit_test_button_index_base(int base_x, int base_y)
{
    for (int i = 0; i < FMC_BUTTON_COUNT; ++i)
    {
        if (button_contains_base_point(&FMC_BUTTONS[i], base_x, base_y))
        {
            return i;
        }
    }

    return -1;
}

void fmc_ui_state_init(FMC_UI_State *state)
{
    if (state == NULL)
    {
        return;
    }

    state->hovered_button = FMC_BUTTON_NONE;
    state->hovered_button_index = -1;
}

void fmc_ui_update_hover(SDL_Renderer *renderer, FMC_UI_State *state, int x, int y)
{
    if (state == NULL)
    {
        return;
    }

    const int index = hit_test_button_index(renderer, x, y);
    state->hovered_button_index = index;
    state->hovered_button = index >= 0 ? FMC_BUTTONS[index].id : FMC_BUTTON_NONE;
}

void fmc_ui_update_hover_base(FMC_UI_State *state, int base_x, int base_y)
{
    if (state == NULL)
    {
        return;
    }

    const int index = hit_test_button_index_base(base_x, base_y);
    state->hovered_button_index = index;
    state->hovered_button = index >= 0 ? FMC_BUTTONS[index].id : FMC_BUTTON_NONE;
}

int fmc_ui_handle_mouse_button(SDL_Renderer *renderer, FMC_UI_State *state, FMC_Data *data, int x, int y)
{
    const int index = hit_test_button_index(renderer, x, y);
    if (state != NULL)
    {
        state->hovered_button_index = index;
        state->hovered_button = index >= 0 ? FMC_BUTTONS[index].id : FMC_BUTTON_NONE;
    }

    if (index < 0 || FMC_BUTTONS[index].action == NULL)
    {
        return 0;
    }

    FMC_BUTTONS[index].action(data, &FMC_BUTTONS[index]);
    return 1;
}

int fmc_ui_handle_mouse_button_base(FMC_UI_State *state, FMC_Data *data, int base_x, int base_y)
{
    const int index = hit_test_button_index_base(base_x, base_y);
    if (state != NULL)
    {
        state->hovered_button_index = index;
        state->hovered_button = index >= 0 ? FMC_BUTTONS[index].id : FMC_BUTTON_NONE;
    }

    if (index < 0 || FMC_BUTTONS[index].action == NULL)
    {
        return 0;
    }

    FMC_BUTTONS[index].action(data, &FMC_BUTTONS[index]);
    return 1;
}

const FMC_Button *fmc_ui_hit_test_button(SDL_Renderer *renderer, int x, int y)
{
    const int index = hit_test_button_index(renderer, x, y);
    return index >= 0 ? &FMC_BUTTONS[index] : NULL;
}
