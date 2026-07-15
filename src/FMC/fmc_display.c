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
    if (button == NULL || button->key == FMC_BUTTON_NONE)
    {
        return;
    }

    if (button->shape == FMC_SHAPE_RECT)
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
    if (button == NULL || button->key == FMC_BUTTON_NONE)
    {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    if (button->shape == FMC_SHAPE_RECT)
    {
        SDL_Rect dest = scale_rect(layout, &button->rect);
        SDL_RenderDrawRect(renderer, &dest);
        return;
    }

    if (button->shape == FMC_SHAPE_CIRCLE)
    {
        const int center_x = layout->viewport.x + (int)((float)button->rect.x * layout->scale + 0.5f);
        const int center_y = layout->viewport.y + (int)((float)button->rect.y * layout->scale + 0.5f);
        int radius = (int)((float)button->rect.w * layout->scale + 0.5f);
        if (radius < 1)
        {
            radius = 1;
        }

        circleRGBA(renderer, center_x, center_y, radius, 255, 255, 255, 255);
        circleRGBA(renderer, center_x, center_y, radius + 1, 255, 255, 255, 255);
    }
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
    draw_fmc_header(renderer, font, layout, "DEP/ARR INDEX", "ACT FPLN", "");
    draw_text(renderer, font, layout, COLOR_WHITE, 128, 132, "<DEP");
    draw_text(renderer, font, layout, COLOR_WHITE, 298, 132, "%s", airport_display_text(data->origin));
    draw_right_text(renderer, font, layout, COLOR_WHITE, 518, 132, "ARR>");

    draw_text(renderer, font, layout, COLOR_WHITE, 312, 180, "%s", airport_display_text(data->destination));
    draw_right_text(renderer, font, layout, COLOR_WHITE, 518, 180, "ARR>");
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
        draw_text(renderer, font, layout, COLOR_TEXT, 126, 112, "ORIGIN");
        draw_text(renderer, font, layout, COLOR_WHITE, 126, 133, "%s", airport_display_text(data->origin));
        draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 112, "DESTINATION");
        draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 133, "%s", airport_display_text(data->destination));

        draw_text(renderer, font, layout, COLOR_TEXT, 126, 161, "CO ROUTE");
        draw_text(renderer, font, layout, COLOR_WHITE, 126, 182, "%s", data->company_route[0] ? data->company_route : "----");
        draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 210, "FLT NO");
        draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 231, "%s", data->flight_no[0] ? data->flight_no : "----");
        if (via_to_list_count > 0)
        {
            draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 259, "RTE LIST>");
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
    int total_count = (runway_count > proc_count ? (runway_count + 1) / 5 : (proc_count + 1) / 5) + 1;
    int start_index = (dep_arr_index - 1) * 5;
    int end_index = start_index + 4;
    SelectDepArr sda = select_dep_arr[dep_arr_type];

    snprintf(title, sizeof(title), "%s %s", show_ariport, dep_arr_type == 0 ? "DEPART" : "ARRIVAL");
    snprintf(page_text, sizeof(page_text), "%d/%d", dep_arr_index, total_count);
    draw_fmc_header(renderer, font, layout, title, data != NULL && data->origin_exec_pending ? "MODEL FPLN" : "ACT FPLN", page_text);

    if (strlen(sda.select_proc) > 0)
    {
        char proc_text[32];
        snprintf(proc_text, sizeof(proc_text), "%s <%s>", sda.select_proc, sda.select_flag == 0 ? "SEL" : "ACT");
        draw_dep_arr_choice(renderer, font, layout, 0, 0, dep_arr_type == 0 ? "SID" : "STARS", proc_text);
        if (strlen(sda.select_proc_trans) > 0)
        {
            char proc_trans_text[32];
            snprintf(proc_trans_text, sizeof(proc_trans_text), "%s <%s>", sda.select_proc_trans, sda.select_flag == 0 ? "SEL" : "ACT");
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
        snprintf(runway_text, sizeof(runway_text), "%s <%s>", sda.select_runway, sda.select_flag == 0 ? "SEL" : "ACT");
        draw_dep_arr_choice(renderer, font, layout, 1, 0, dep_arr_type == 0 ? "RWYS" : "APPR", runway_text);
        if (strlen(sda.select_runway_trans) > 0)
        {
            char runway_trans_text[32];
            snprintf(runway_trans_text, sizeof(runway_trans_text), "%s <%s>", sda.select_runway_trans, sda.select_flag == 0 ? "SEL" : "ACT");
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
    char limit_text[24];
    snprintf(tgt_speed_text, sizeof(tgt_speed_text), "%d/.%02d", tgt_speed1.speed1, tgt_speed1.speed2);
    snprintf(limit_text, sizeof(limit_text), "%d/%d", spd_alt_limit1.spd_limit, spd_alt_limit1.alt_limit);

    draw_fmc_header(renderer, font, layout, "ACT VNAV CLIMB", "", "1/3");

    draw_text(renderer, font, layout, COLOR_TEXT, 132, 124, "TGT SPEED");
    draw_text(renderer, font, layout, COLOR_WHITE, 132, 150, "%s", tgt_speed_text);
    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 124, "TRANS ALT");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 150, "%d", trans_alt);

    draw_text(renderer, font, layout, COLOR_TEXT, 132, 190, "SPD/ALT LIMIT");
    draw_text(renderer, font, layout, COLOR_WHITE, 132, 216, "%s", limit_text);
    draw_text(renderer, font, layout, COLOR_DIM, 132, 260, "--/-----");

    draw_fmc_softkeys(renderer, font, layout, "<RTE", "CRZ>");
}

static void draw_cruise_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    (void)data;
    char tgt_speed_text[24];
    snprintf(tgt_speed_text, sizeof(tgt_speed_text), "%d/.%02d", tgt_speed2.speed1, tgt_speed2.speed2);

    draw_fmc_header(renderer, font, layout, "VNAV CRUISE", "", "2/3");

    draw_text(renderer, font, layout, COLOR_TEXT, 132, 124, "TGT SPEED");
    draw_text(renderer, font, layout, COLOR_WHITE, 132, 150, "%s", tgt_speed_text);

    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 124, "CRZ ALT");
    if (crz_alt > 0)
    {
        draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 150, "FL%03d", crz_alt / 100);
    }
    else
    {
        draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 150, "----");
    }

    draw_text(renderer, font, layout, COLOR_DIM, 132, 216, "--/-----");

    draw_fmc_softkeys(renderer, font, layout, "<CLB", "DES>");
}

static void draw_descent_page(SDL_Renderer *renderer, TTF_Font *font, const FMC_Layout *layout, const FMC_Data *data)
{
    (void)data;
    char tgt_speed_text[24];
    char limit_text[24];
    snprintf(tgt_speed_text, sizeof(tgt_speed_text), "%d/.%02d", tgt_speed3.speed1, tgt_speed3.speed2);
    snprintf(limit_text, sizeof(limit_text), "%d/%d", spd_alt_limit1.spd_limit, spd_alt_limit1.alt_limit);

    draw_fmc_header(renderer, font, layout, "VNAV DESCENT", "", "3/3");

    draw_text(renderer, font, layout, COLOR_TEXT, 132, 124, "TGT SPEED");
    draw_text(renderer, font, layout, COLOR_WHITE, 132, 150, "%s", tgt_speed_text);

    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 124, "TRANS FL");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 150, "FL%03d", trans_fl);

    draw_text(renderer, font, layout, COLOR_TEXT, 132, 190, "SPD/ALT LIMIT");
    draw_text(renderer, font, layout, COLOR_WHITE, 132, 216, "%s", limit_text);
    draw_text(renderer, font, layout, COLOR_DIM, 132, 260, "--/-----");

    draw_right_text(renderer, font, layout, COLOR_TEXT, 510, 256, "VPA");
    draw_right_text(renderer, font, layout, COLOR_WHITE, 510, 282, "%.1f", vpa);

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
        if (button != NULL && button->key == FMC_BUTTON_EXEC)
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
