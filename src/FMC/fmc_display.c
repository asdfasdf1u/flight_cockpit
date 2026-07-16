// FMC display module
#include "fmc_display.h"

#include "fmc_key.h"

#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern int rte_index;
extern int dep_arr_index;
extern int dep_arr_type;
extern char show_ariport[20];

#define FMC_BASE_WIDTH 638
#define FMC_BASE_HEIGHT 998

typedef struct FMC_Layout
{
    SDL_Rect viewport;
    float scale;
} FMC_Layout;

static const SDL_Color COLOR_BG = {5, 8, 10, 255};
static const SDL_Color COLOR_SCREEN = {0, 0, 0, 255};
static const SDL_Color COLOR_TEXT = {64, 225, 255, 255};
static const SDL_Color COLOR_DIM = {48, 150, 180, 255};
static const SDL_Color COLOR_CYAN = {64, 225, 255, 255};
static const SDL_Color COLOR_WHITE = {235, 244, 232, 255};
static const SDL_Color COLOR_AMBER = {255, 185, 95, 255};
static const SDL_Color COLOR_EXEC = {40, 150, 255, 235};

static const SDL_Rect FMC_SCREEN_RECT = {104, 74, 435, 345};
static const SDL_Rect FMC_SCRATCHPAD_RECT = {145, 388, 350, 28};

static void set_color(SDL_Renderer *renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
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

int fmc_display_window_to_base(SDL_Renderer *renderer, int x, int y, int *base_x, int *base_y)
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
                   (Sint16)(dest.x + dest.w - 1),
                   (Sint16)(dest.y + dest.h - 1),
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

static SDL_Rect expand_rect(SDL_Rect rect, int amount)
{
    rect.x -= amount;
    rect.y -= amount;
    rect.w += amount * 2;
    rect.h += amount * 2;
    return rect;
}

static void draw_button_highlight(SDL_Renderer *renderer, const FMC_Layout *layout, const FMC_Button *button, SDL_Color color)
{
    if (button == NULL || button->id == FMC_BUTTON_NONE)
    {
        return;
    }

    if (button->shape == FMC_BUTTON_SHAPE_RECT)
    {
        for (int i = 0; i < 3; ++i)
        {
            SDL_Rect outline = expand_rect(button->rect, i);
            draw_scaled_outline(renderer, layout, &outline, color);
        }
        return;
    }

    for (int i = 0; i < 3; ++i)
    {
        draw_scaled_circle_outline(renderer, layout, (SDL_Point){button->rect.x, button->rect.y}, button->rect.w + i, color);
    }
}

static void draw_hover_outline(SDL_Renderer *renderer, const FMC_Layout *layout, const FMC_Button *button)
{
    if (button == NULL || button->id == FMC_BUTTON_NONE)
    {
        return;
    }

    const SDL_Color hover_outline = {255, 255, 255, 235};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (button->shape == FMC_BUTTON_SHAPE_RECT)
    {
        for (int i = 0; i < 3; ++i)
        {
            SDL_Rect outline = expand_rect(button->rect, i);
            draw_scaled_outline(renderer, layout, &outline, hover_outline);
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        return;
    }

    if (button->shape == FMC_BUTTON_SHAPE_CIRCLE)
    {
        const int center_x = layout->viewport.x + (int)((float)button->center.x * layout->scale + 0.5f);
        const int center_y = layout->viewport.y + (int)((float)button->center.y * layout->scale + 0.5f);
        int radius = (int)((float)button->radius * layout->scale + 0.5f);
        if (radius < 1)
        {
            radius = 1;
        }

        for (int i = 0; i < 3; ++i)
        {
            circleRGBA(renderer,
                       (Sint16)center_x,
                       (Sint16)center_y,
                       (Sint16)(radius + i),
                       hover_outline.r,
                       hover_outline.g,
                       hover_outline.b,
                       hover_outline.a);
        }
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

static void draw_title(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, int lc, const char *title, int type)
{
    const int y = type == 0 ? 88 : 106;

    if (title == NULL || title[0] == '\0')
    {
        return;
    }

    if (lc == 0)
    {
        draw_text(renderer, font, layout, COLOR_TEXT, 126, y, "%s", title);
        return;
    }

    draw_centered_text(renderer, font, layout, COLOR_TEXT, &(SDL_Rect){104, y, 435, 22}, "%s", title);
}

static void draw_screen_lr_text(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, int lr, int index, const char *text1, const char *text2)
{
    const int base_y = 112 + index * 48;
    const int left_x = 126;
    const int right_x = 510;
    const int center_x = 104;
    const int center_w = 435;

    if (text1 == NULL || text1[0] == '\0')
    {
        return;
    }

    if (text2 == NULL)
    {
        if (lr == 1)
        {
            draw_right_text(renderer, font, layout, COLOR_WHITE, right_x, base_y + 20, "%s", text1);
        }
        else if (lr == 2)
        {
            draw_centered_text(renderer, font, layout, COLOR_WHITE, &(SDL_Rect){center_x, base_y + 20, center_w, 22}, "%s", text1);
        }
        else
        {
            draw_text(renderer, font, layout, COLOR_WHITE, left_x, base_y + 20, "%s", text1);
        }
        return;
    }

    if (lr == 1)
    {
        draw_right_text(renderer, font, layout, COLOR_TEXT, right_x, base_y, "%s", text1);
        draw_right_text(renderer, font, layout, COLOR_WHITE, right_x, base_y + 21, "%s", text2);
    }
    else if (lr == 2)
    {
        draw_centered_text(renderer, font, layout, COLOR_TEXT, &(SDL_Rect){center_x, base_y, center_w, 20}, "%s", text1);
        draw_centered_text(renderer, font, layout, COLOR_WHITE, &(SDL_Rect){center_x, base_y + 21, center_w, 22}, "%s", text2);
    }
    else
    {
        draw_text(renderer, font, layout, COLOR_TEXT, left_x, base_y, "%s", text1);
        draw_text(renderer, font, layout, COLOR_WHITE, left_x, base_y + 21, "%s", text2);
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

static const char *airport_display_text(const char *airport)
{
    return (airport != NULL && airport[0] != '\0') ? airport : "";
}

static const char *route_field_display_text(const char *text)
{
    return (text != NULL && text[0] != '\0') ? text : "----";
}

static void format_route_display_text(const FMC_Data *data, char *dest, int dest_size)
{
    const char *origin = airport_display_text(data != NULL ? data->origin : NULL);
    const char *destination = airport_display_text(data != NULL ? data->destination : NULL);

    if (dest == NULL || dest_size <= 0)
    {
        return;
    }

    if (origin[0] == '\0' && destination[0] == '\0')
    {
        dest[0] = '\0';
        return;
    }

    snprintf(dest, (size_t)dest_size, "%s-%s", origin, destination);
}

static void draw_home_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    (void)data;
    draw_title(renderer, font, layout, 1, "INDEX", 0);
    draw_screen_lr_text(renderer, font, layout, 0, 0, "<STATUS", NULL);
    draw_screen_lr_text(renderer, font, layout, 1, 0, "ROUTE MENU>", NULL);
    draw_screen_lr_text(renderer, font, layout, 1, 1, "DATABASE>", NULL);
    draw_screen_lr_text(renderer, font, layout, 1, 4, "ARR DATA>", NULL);
}

static void draw_route_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    const int page_count = fmc_data_route_page_count(data);
    int page_index = rte_index;
    char page_text[16];

    if (page_index < 1)
    {
        page_index = 1;
    }
    if (page_index > page_count)
    {
        page_index = page_count;
    }

    snprintf(page_text, sizeof(page_text), "RTE %d/%d", page_index, page_count);
    draw_fmc_header(renderer,
                    font,
                    layout,
                    data != NULL && fmc_data_route_has_uncommitted_changes(data) ? "MOD FPLN" : "ACT FPLN",
                    "",
                    page_text);

    if (page_index == 1)
    {
        draw_screen_lr_text(renderer, font, layout, 0, 0, "ORIGIN", route_field_display_text(origin));
        draw_screen_lr_text(renderer, font, layout, 1, 0, "DESTINATION", route_field_display_text(dest));
        draw_screen_lr_text(renderer, font, layout, 0, 1, "CO ROUTE", route_field_display_text(co_route));
        draw_screen_lr_text(renderer, font, layout, 1, 2, "FLT NO", route_field_display_text(flt_no));
        if (via_to_list_count > 0)
        {
            draw_screen_lr_text(renderer, font, layout, 1, 3, "RTE LIST>", NULL);
        }

        draw_text(renderer, font, layout, COLOR_TEXT, 126, 296, via_to_list_count > 0 ? "<VIA" : "VIA");
        draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 296, "TO");
        if (via_to_list_count > 0)
        {
            draw_text(renderer, font, layout, COLOR_WHITE, 126, 317, "%s", via_to_list[0].VIA);
            draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 317, "%s", via_to_list[0].TO);
        }
        else
        {
            draw_text(renderer, font, layout, COLOR_WHITE, 126, 317, "DIRECT");
            draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 317, "----");
        }
    }
    else
    {
        const int start_index = (page_index - 2) * FMC_RTE_PAGE_SIZE;
        const int end_index = start_index + 4;
        const int row_y = 132;
        const int row_spacing = 48;

        draw_text(renderer, font, layout, COLOR_TEXT, 126, 112, "VIA");
        draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 112, "TO");

        for (int route_index = start_index, row = 0; route_index <= end_index && route_index < via_to_list_count; ++route_index, ++row)
        {
            draw_text(renderer, font, layout, COLOR_WHITE, 126, row_y + row * row_spacing, "%s", via_to_list[route_index].VIA);
            draw_right_text(renderer, font, layout, COLOR_WHITE, 510, row_y + row * row_spacing, "%s", via_to_list[route_index].TO);
        }
    }

    draw_fmc_softkeys(renderer, font, layout, "<ROUTE MENU", "VNAV>");
}

static void draw_dep_arr_choice(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, int right_side, int row, const char *label, const char *value)
{
    const int y = 112 + row * 48;
    if (right_side)
    {
        if (label != NULL && label[0] != '\0')
        {
            draw_right_text(renderer, font, layout, COLOR_TEXT, 510, y, "%s", label);
        }
        draw_right_text(renderer, font, layout, COLOR_WHITE, 510, y + 20, "%s", value != NULL && value[0] != '\0' ? value : "----");
    }
    else
    {
        if (label != NULL && label[0] != '\0')
        {
            draw_text(renderer, font, layout, COLOR_TEXT, 126, y, "%s", label);
        }
        draw_text(renderer, font, layout, COLOR_WHITE, 126, y + 20, "%s", value != NULL && value[0] != '\0' ? value : "----");
    }
}

static void format_dep_arr_selected(char *dest, int dest_size, const char *value)
{
    if (dest == NULL || dest_size <= 0)
    {
        return;
    }

    snprintf(dest, (size_t)dest_size, "%s <SEL>", value != NULL ? value : "");
}

static int dep_arr_page_count(int row_count)
{
    if (row_count <= 0)
    {
        return 1;
    }

    return (row_count + 4) / 5;
}

static int dep_arr_visible_row_count(const SelectDepArr *sda)
{
    int left_rows = proc_count;
    int right_rows = runway_count;

    if (sda != NULL && sda->select_proc[0] != '\0')
    {
        left_rows = 1 + proc_trans_count;
    }
    if (sda != NULL && sda->select_runway[0] != '\0')
    {
        right_rows = 1 + runway_trans_count;
    }

    return left_rows > right_rows ? left_rows : right_rows;
}

static void draw_dep_arr_index_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    draw_fmc_header(renderer, font, layout, "DEP/ARR INDEX", "ACT FPLN", "");
    if (data->origin[0] != '\0')
    {
        draw_dep_arr_choice(renderer, font, layout, 0, 0, "<DEP", data->origin);
        draw_dep_arr_choice(renderer, font, layout, 1, 0, "ARR>", data->origin);
    }
    if (data->destination[0] != '\0')
    {
        draw_dep_arr_choice(renderer, font, layout, 1, 1, "ARR>", data->destination);
    }
    draw_fmc_softkeys(renderer, font, layout, "", "");
}

static void draw_dep_arr_selection_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    (void)data;
    char title[32];
    char page_text[16];
    int total_count = dep_arr_page_count(dep_arr_visible_row_count(&select_dep_arr[dep_arr_type]));
    int page_index = dep_arr_index;
    if (page_index < 1)
    {
        page_index = 1;
    }
    if (page_index > total_count)
    {
        page_index = total_count;
    }
    int start_index = (page_index - 1) * 5;
    int end_index = start_index + 4;
    SelectDepArr sda = select_dep_arr[dep_arr_type];

    snprintf(title, sizeof(title), "%s %s", show_ariport, dep_arr_type == 0 ? "DEPART" : "ARRIVAL");
    snprintf(page_text, sizeof(page_text), "%d/%d", page_index, total_count);
    draw_fmc_header(renderer, font, layout, title, data != NULL && data->origin_exec_pending ? "MODEL FPLN" : "ACT FPLN", page_text);

    if (strlen(sda.select_proc) > 0)
    {
        char proc_text[32];
        format_dep_arr_selected(proc_text, sizeof(proc_text), sda.select_proc);
        draw_dep_arr_choice(renderer, font, layout, 0, 0, dep_arr_type == 0 ? "SID" : "STARS", proc_text);
        if (strlen(sda.select_proc_trans) > 0)
        {
            char proc_trans_text[32];
            format_dep_arr_selected(proc_trans_text, sizeof(proc_trans_text), sda.select_proc_trans);
            draw_dep_arr_choice(renderer, font, layout, 0, 1, "TRANS", proc_trans_text);
        }
        else
        {
            for (int i = 0, row = 1; i < 5 && i < proc_trans_count; ++i, ++row)
            {
                draw_dep_arr_choice(renderer, font, layout, 0, row, i == 0 ? "TRANS" : "", proc_trans[i]);
            }
        }
    }
    else
    {
        for (int i = start_index, row = 0; i <= end_index && i < proc_count; ++i, ++row)
        {
            draw_dep_arr_choice(renderer, font, layout, 0, row, i == start_index ? (dep_arr_type == 0 ? "SID" : "STARS") : "", proc[i]);
        }
    }

    if (strlen(sda.select_runway) > 0)
    {
        char runway_text[32];
        format_dep_arr_selected(runway_text, sizeof(runway_text), sda.select_runway);
        draw_dep_arr_choice(renderer, font, layout, 1, 0, dep_arr_type == 0 ? "RWYS" : "APPR", runway_text);
        if (strlen(sda.select_runway_trans) > 0)
        {
            char runway_trans_text[32];
            format_dep_arr_selected(runway_trans_text, sizeof(runway_trans_text), sda.select_runway_trans);
            draw_dep_arr_choice(renderer, font, layout, 1, 1, "TRANS", runway_trans_text);
        }
        else
        {
            for (int i = 0, row = 1; i < 5 && i < runway_trans_count; ++i, ++row)
            {
                draw_dep_arr_choice(renderer, font, layout, 1, row, i == 0 ? "TRANS" : "", runway_trans[i]);
            }
        }
    }
    else
    {
        for (int i = start_index, row = 0; i <= end_index && i < runway_count; ++i, ++row)
        {
            draw_dep_arr_choice(renderer, font, layout, 1, row, i == start_index ? (dep_arr_type == 0 ? "RWYS" : "APPR") : "", runway[i]);
        }
    }

    draw_fmc_softkeys(renderer, font, layout, "<INDEX", "");
}

static void draw_dep_arr_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    if (show_ariport[0] == '\0')
    {
        draw_dep_arr_index_page(renderer, font, layout, data);
    }
    else
    {
        draw_dep_arr_selection_page(renderer, font, layout, data);
    }
}

static void draw_perf_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    char route_text[2 * FMC_TEXT_LEN + 2];

    format_route_display_text(data, route_text, sizeof(route_text));
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
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 200, "%s", route_text);

    draw_text(renderer, font, layout, COLOR_TEXT, 126, 240, "RESERVES");
    draw_text(renderer, font, layout, COLOR_WHITE, 126, 262, "----");
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 240, "STEP SIZE");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 262, "----");

    draw_fmc_softkeys(renderer, font, layout, "<INDEX", "CLB>");
}

static void draw_climb_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    (void)data;
    char tgt_speed_text[24];
    char trans_alt_text[24];
    char limit_text[24];
    snprintf(tgt_speed_text, sizeof(tgt_speed_text), "%d/.%02d", tgt_speed1.speed1, tgt_speed1.speed2);
    snprintf(trans_alt_text, sizeof(trans_alt_text), "%d", trans_alt);
    snprintf(limit_text, sizeof(limit_text), "%d/%d", spd_alt_limit1.spd_limit, spd_alt_limit1.alt_limit);

    draw_fmc_header(renderer, font, layout, "ACT VNAV CLIMB", "", "1/3");
    draw_screen_lr_text(renderer, font, layout, 0, 0, "TGT SPEED", tgt_speed_text);
    draw_screen_lr_text(renderer, font, layout, 1, 0, "TRANS ALT", trans_alt_text);
    draw_screen_lr_text(renderer, font, layout, 0, 1, "SPD/ALT LIMIT", limit_text);
    draw_text(renderer, font, layout, COLOR_DIM, 132, 260, "--/-----");

    draw_fmc_softkeys(renderer, font, layout, "<RTE", "CRZ>");
}

static void draw_cruise_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    (void)data;
    char tgt_speed_text[24];
    char crz_alt_text[24];
    snprintf(tgt_speed_text, sizeof(tgt_speed_text), "%d/.%02d", tgt_speed2.speed1, tgt_speed2.speed2);
    if (crz_alt > 0)
    {
        snprintf(crz_alt_text, sizeof(crz_alt_text), "FL%03d", crz_alt / 100);
    }
    else
    {
        snprintf(crz_alt_text, sizeof(crz_alt_text), "----");
    }

    draw_fmc_header(renderer, font, layout, "VNAV CRUISE", "", "2/3");
    draw_screen_lr_text(renderer, font, layout, 0, 0, "TGT SPEED", tgt_speed_text);
    draw_screen_lr_text(renderer, font, layout, 1, 0, "CRZ ALT", crz_alt_text);
    draw_text(renderer, font, layout, COLOR_DIM, 132, 216, "--/-----");

    draw_fmc_softkeys(renderer, font, layout, "<CLB", "DES>");
}

static void draw_descent_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    (void)data;
    char tgt_speed_text[24];
    char trans_fl_text[24];
    char limit_text[24];
    char vpa_text[24];
    snprintf(tgt_speed_text, sizeof(tgt_speed_text), "%d/.%02d", tgt_speed3.speed1, tgt_speed3.speed2);
    snprintf(trans_fl_text, sizeof(trans_fl_text), "FL%03d", trans_fl);
    snprintf(limit_text, sizeof(limit_text), "%d/%d", spd_alt_limit1.spd_limit, spd_alt_limit1.alt_limit);
    snprintf(vpa_text, sizeof(vpa_text), "%.1f", vpa);

    draw_fmc_header(renderer, font, layout, "VNAV DESCENT", "", "3/3");
    draw_screen_lr_text(renderer, font, layout, 0, 0, "TGT SPEED", tgt_speed_text);
    draw_screen_lr_text(renderer, font, layout, 1, 0, "TRANS FL", trans_fl_text);
    draw_screen_lr_text(renderer, font, layout, 0, 1, "SPD/ALT LIMIT", limit_text);
    draw_text(renderer, font, layout, COLOR_DIM, 132, 260, "--/-----");
    draw_screen_lr_text(renderer, font, layout, 1, 3, "VPA", vpa_text);

    draw_fmc_softkeys(renderer, font, layout, "<CRZ", "");
}

static void draw_legs_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    draw_fmc_header(renderer,
                    font,
                    layout,
                    data != NULL && fmc_data_route_has_uncommitted_changes(data) ? "MOD LEGS" : "ACT LEGS",
                    "ROUTE SEGMENTS",
                    "1/1");
    draw_text(renderer, font, layout, COLOR_TEXT, 126, 112, "LEG");
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 112, "TO");

    if (via_to_list_count <= 0)
    {
        draw_centered_text(renderer, font, layout, COLOR_DIM, &(SDL_Rect){126, 190, 384, 24}, "NO ROUTE");
        draw_fmc_softkeys(renderer, font, layout, "<RTE", "");
        return;
    }

    const int max_rows = 6;
    const int row_y = 132;
    const int row_spacing = 36;
    int rows = via_to_list_count < max_rows ? via_to_list_count : max_rows;
    for (int i = 0; i < rows; ++i)
    {
        draw_text(renderer, font, layout, COLOR_DIM, 126, row_y + i * row_spacing, "%02d %s", i + 1, via_to_list[i].VIA);
        draw_right_text(renderer, font, layout, COLOR_WHITE, 510, row_y + i * row_spacing, "%s", via_to_list[i].TO);
    }

    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 364, "MOVE FIX/AFTER");
    draw_right_text(renderer,
                    font,
                    layout,
                    COLOR_DIM,
                    510,
                    384,
                    "%s",
                    data != NULL && data->legs_sequence[0] != '\0' ? data->legs_sequence : "----");

    draw_fmc_softkeys(renderer, font, layout, "<RTE", "");
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

static void draw_status_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    (void)data;
    draw_title(renderer, font, layout, 1, "STATUS", 0);
    draw_screen_lr_text(renderer, font, layout, 0, 0, "NAV DATA", "WORLD (XPLANE)");
    draw_screen_lr_text(renderer, font, layout, 0, 1, "ACTIVE DATABASE", "01FEB18 01MAR18");
    draw_screen_lr_text(renderer, font, layout, 0, 2, "SEC DATABASE", "NOT AVAIL");
    draw_screen_lr_text(renderer, font, layout, 0, 3, "UTC", "18:35");
    draw_screen_lr_text(renderer, font, layout, 1, 3, "DATE", "22MAR25");
    draw_screen_lr_text(renderer, font, layout, 0, 4, "PROGRAM", "X-PLANE 11.55r2");
    draw_fmc_softkeys(renderer, font, layout, "<INDEX", "DATABASE>");
}

static void draw_prog_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    char route_text[2 * FMC_TEXT_LEN + 2] = {0};
    char next_text[FMC_TEXT_LEN] = "----";
    char gs_text[24];
    char alt_text[24];
    char fuel_text[24];

    if (data != NULL)
    {
        format_route_display_text(data, route_text, sizeof(route_text));
        if (data->route_count > 0)
        {
            int index = data->active_waypoint_index;
            if (index < 0)
            {
                index = 0;
            }
            if (index >= data->route_count)
            {
                index = data->route_count - 1;
            }
            snprintf(next_text, sizeof(next_text), "%s", data->route_points[index][0] != '\0' ? data->route_points[index] : "----");
        }
    }

    snprintf(gs_text, sizeof(gs_text), "%.0f KT", data != NULL && data->current_ground_speed > 0.0f ? data->current_ground_speed : 0.0f);
    snprintf(alt_text, sizeof(alt_text), "%.0f FT", data != NULL && data->current_altitude_ft > 0.0f ? data->current_altitude_ft : 0.0f);
    snprintf(fuel_text, sizeof(fuel_text), "%.0f KG", data != NULL && data->current_fuel_kg > 0.0f ? data->current_fuel_kg : 0.0f);

    draw_fmc_header(renderer, font, layout, "PROGRESS", data != NULL && data->route_mod_pending ? "MOD FPLN" : "ACT FPLN", "1/1");
    draw_screen_lr_text(renderer, font, layout, 0, 0, "ROUTE", route_text[0] != '\0' ? route_text : "----");
    draw_screen_lr_text(renderer, font, layout, 1, 0, "NEXT WPT", next_text);
    draw_screen_lr_text(renderer, font, layout, 0, 1, "GROUND SPD", gs_text);
    draw_screen_lr_text(renderer, font, layout, 1, 1, "ALTITUDE", alt_text);
    draw_screen_lr_text(renderer, font, layout, 0, 2, "FUEL", fuel_text);
    draw_screen_lr_text(renderer, font, layout, 1, 2, "ETA", "--:--");
    draw_fmc_softkeys(renderer, font, layout, "<INDEX", "RTE>");
}

static void draw_dir_intc_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    (void)data;
    draw_fmc_header(renderer, font, layout, "DIR INTC", "ACT FPLN", "1/1");
    draw_screen_lr_text(renderer, font, layout, 0, 0, "DIRECT TO", "----");
    draw_screen_lr_text(renderer, font, layout, 1, 0, "INTC CRS", "----");
    draw_screen_lr_text(renderer, font, layout, 0, 1, "WAYPOINT", "----");
    draw_screen_lr_text(renderer, font, layout, 1, 1, "DISTANCE", "----");
    draw_fmc_softkeys(renderer, font, layout, "<INDEX", "");
}

static void draw_fix_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    (void)data;
    draw_fmc_header(renderer, font, layout, "FIX INFO", "ACT FPLN", "1/1");
    draw_screen_lr_text(renderer, font, layout, 0, 0, "FIX", "----");
    draw_screen_lr_text(renderer, font, layout, 1, 0, "ABEAM", "----");
    draw_screen_lr_text(renderer, font, layout, 0, 1, "RADIAL", "----");
    draw_screen_lr_text(renderer, font, layout, 1, 1, "DIST", "----");
    draw_fmc_softkeys(renderer, font, layout, "<INDEX", "");
}

static void draw_nav_rad_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    (void)data;
    draw_fmc_header(renderer, font, layout, "NAV RADIO", "ACT FPLN", "1/1");
    draw_screen_lr_text(renderer, font, layout, 0, 0, "VOR L", "----");
    draw_screen_lr_text(renderer, font, layout, 1, 0, "VOR R", "----");
    draw_screen_lr_text(renderer, font, layout, 0, 1, "ILS", "----");
    draw_screen_lr_text(renderer, font, layout, 1, 1, "CRS", "----");
    draw_fmc_softkeys(renderer, font, layout, "<INDEX", "");
}

typedef void (*FMC_PageRenderFn)(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data);

static FMC_PageRenderFn page_renderer(FMC_Page page)
{
    static const FMC_PageRenderFn renderers[FMC_PAGE_COUNT] = {
        [FMC_PAGE_HOME] = draw_home_page,
        [FMC_PAGE_ROUTE] = draw_route_page,
        [FMC_PAGE_DEP_ARR] = draw_dep_arr_page,
        [FMC_PAGE_PERF] = draw_perf_page,
        [FMC_PAGE_CLIMB] = draw_climb_page,
        [FMC_PAGE_CRUISE] = draw_cruise_page,
        [FMC_PAGE_DESCENT] = draw_descent_page,
        [FMC_PAGE_LEGS] = draw_legs_page,
        [FMC_PAGE_HOLD] = draw_hold_page,
        [FMC_PAGE_PROG] = draw_prog_page,
        [FMC_PAGE_STATUS] = draw_status_page,
        [FMC_PAGE_DIR_INTC] = draw_dir_intc_page,
        [FMC_PAGE_FIX] = draw_fix_page,
        [FMC_PAGE_NAV_RAD] = draw_nav_rad_page};

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
    const int count = fmc_key_button_count();
    for (int i = 0; i < count; ++i)
    {
        const FMC_Button *button = fmc_key_button_at(i);
        if (fmc_key_is_page_button(button) && button->page == current_page)
        {
            draw_button_highlight(renderer, layout, button, COLOR_CYAN);
            return;
        }
    }
}

static void draw_exec_light(SDL_Renderer *renderer, const FMC_Layout *layout, const FMC_Data *data)
{
    if (data == NULL || !data->origin_exec_pending)
    {
        return;
    }

    const int count = fmc_key_button_count();
    for (int i = 0; i < count; ++i)
    {
        const FMC_Button *button = fmc_key_button_at(i);
        if (button != NULL && button->id == FMC_BUTTON_EXEC)
        {
            const SDL_Rect exec_light = {
                button->rect.x + 8,
                button->rect.y - 34,
                button->rect.w - 16,
                10};

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            draw_scaled_rect(renderer, layout, &exec_light, COLOR_EXEC);
            draw_scaled_outline(renderer, layout, &exec_light, COLOR_EXEC);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            return;
        }
    }
}

static void draw_hover_button(SDL_Renderer *renderer, const FMC_Layout *layout, const FMC_Event_State *state)
{
    if (state == NULL || state->hovered_button_index < 0 || state->hovered_button_index >= fmc_key_button_count())
    {
        return;
    }

    const FMC_Button *button = fmc_key_button_at(state->hovered_button_index);
    draw_hover_outline(renderer, layout, button);
}

int fmc_display_assets_load(SDL_Renderer *renderer, FMC_Display_Assets *assets)
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

void fmc_display_assets_destroy(FMC_Display_Assets *assets)
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

void fmc_display_render(SDL_Renderer *renderer, TTF_Font *font, const FMC_Display_Assets *assets, const FMC_Event_State *state, const FMC_Data *data)
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
    draw_exec_light(renderer, &layout, data);
    draw_hover_button(renderer, &layout, state);
}

void fmc_display_render_screen_only(SDL_Renderer *renderer, TTF_Font *font, const FMC_Data *data, const SDL_Rect *screen_rect)
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

void fmc_display_render_exec_light_only(SDL_Renderer *renderer, const FMC_Data *data)
{
    if (renderer == NULL || data == NULL)
    {
        return;
    }

    FMC_Layout layout = get_layout(renderer);
    draw_exec_light(renderer, &layout, data);
}

void fmc_display_render_hover_only(SDL_Renderer *renderer, const FMC_Event_State *state)
{
    if (renderer == NULL || state == NULL)
    {
        return;
    }

    FMC_Layout layout = get_layout(renderer);
    draw_hover_button(renderer, &layout, state);
}
