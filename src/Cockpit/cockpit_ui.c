#include "cockpit_ui.h"

#include <stdarg.h>
#include <stdio.h>

static const SDL_Color COLOR_BLACK = {0, 0, 0, 255};
static const SDL_Color COLOR_PANEL = {10, 14, 16, 255};
static const SDL_Color COLOR_BEZEL = {22, 29, 33, 255};
static const SDL_Color COLOR_EDGE = {84, 96, 98, 255};
static const SDL_Color COLOR_DIM = {112, 132, 132, 255};
static const SDL_Color COLOR_CYAN = {70, 210, 255, 255};
static const SDL_Color COLOR_GREEN = {90, 255, 135, 255};
static const SDL_Color COLOR_AMBER = {255, 190, 65, 255};
static const SDL_Color COLOR_MAGENTA = {255, 80, 220, 255};

static void set_color(SDL_Renderer *renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static void fill_rect(SDL_Renderer *renderer, const SDL_Rect *rect, SDL_Color color)
{
    set_color(renderer, color);
    SDL_RenderFillRect(renderer, rect);
}

static void draw_rect(SDL_Renderer *renderer, const SDL_Rect *rect, SDL_Color color)
{
    set_color(renderer, color);
    SDL_RenderDrawRect(renderer, rect);
}

static void draw_text(SDL_Renderer *renderer, TTF_Font *font, SDL_Color color, int x, int y, const char *format, ...)
{
    if (renderer == NULL || font == NULL || format == NULL)
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

    SDL_Rect dest = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

static void draw_centered_text(SDL_Renderer *renderer, TTF_Font *font, SDL_Color color, const SDL_Rect *rect, const char *format, ...)
{
    if (renderer == NULL || font == NULL || rect == NULL || format == NULL)
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

    draw_text(renderer, font, color, rect->x + (rect->w - text_w) / 2, rect->y + (rect->h - text_h) / 2, "%s", text);
}

static void draw_display_texture(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, SDL_Texture *texture, const char *label, int preserve_aspect)
{
    SDL_Rect bezel = {rect->x - 18, rect->y - 22, rect->w + 36, rect->h + 54};
    fill_rect(renderer, &bezel, COLOR_BEZEL);
    draw_rect(renderer, &bezel, COLOR_EDGE);

    fill_rect(renderer, rect, COLOR_BLACK);
    if (texture != NULL)
    {
        SDL_Rect texture_rect = *rect;
        int texture_width = 0;
        int texture_height = 0;

        if (preserve_aspect &&
            SDL_QueryTexture(texture, NULL, NULL, &texture_width, &texture_height) == 0 &&
            texture_width > 0 && texture_height > 0)
        {
            float scale = (float)rect->w / (float)texture_width;
            const float vertical_scale = (float)rect->h / (float)texture_height;
            if (scale > vertical_scale)
            {
                scale = vertical_scale;
            }

            texture_rect.w = (int)((float)texture_width * scale);
            texture_rect.h = (int)((float)texture_height * scale);
            texture_rect.x = rect->x + (rect->w - texture_rect.w) / 2;
            texture_rect.y = rect->y + (rect->h - texture_rect.h) / 2;
        }

        SDL_RenderCopy(renderer, texture, NULL, &texture_rect);
    }
    else
    {
        draw_centered_text(renderer, font, COLOR_DIM, rect, "%s", label);
    }

    draw_rect(renderer, rect, COLOR_DIM);
    draw_centered_text(renderer, font, COLOR_CYAN, &(SDL_Rect){bezel.x, bezel.y + bezel.h - 28, bezel.w, 22}, "%s", label);
}

static void draw_fmc_hotspot_hint(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const char *label)
{
    draw_rect(renderer, rect, COLOR_AMBER);
    draw_centered_text(renderer, font, COLOR_AMBER, &(SDL_Rect){rect->x, rect->y + rect->h - 44, rect->w, 28}, "%s - CLICK TO ZOOM", label);
}

static void draw_fallback_background(SDL_Renderer *renderer, TTF_Font *font, const Cockpit_Layout *layout)
{
    fill_rect(renderer, &(SDL_Rect){0, 0, layout->world_width, layout->world_height}, (SDL_Color){5, 7, 9, 255});
    fill_rect(renderer, &(SDL_Rect){120, 80, layout->world_width - 240, 780}, (SDL_Color){13, 18, 21, 255});
    fill_rect(renderer, &(SDL_Rect){220, 900, layout->world_width - 440, 920}, (SDL_Color){10, 14, 16, 255});
    fill_rect(renderer, &(SDL_Rect){360, 1840, layout->world_width - 720, 960}, (SDL_Color){7, 10, 12, 255});
    draw_centered_text(renderer, font, COLOR_DIM, &(SDL_Rect){0, 120, layout->world_width, 80}, "COCKPIT BACKGROUND FALLBACK");
}

void cockpit_ui_render_scene(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const Cockpit_Layout *layout,
    SDL_Texture *background_texture,
    SDL_Texture *capt_pfd_texture,
    SDL_Texture *capt_nd_texture,
    SDL_Texture *eicas1_texture,
    SDL_Texture *fo_nd_texture,
    SDL_Texture *fo_pfd_texture,
    SDL_Texture *eicas2_texture,
    SDL_Texture *fmc_texture)
{
    if (renderer == NULL || font == NULL || layout == NULL)
    {
        return;
    }

    if (background_texture != NULL)
    {
        SDL_RenderCopy(renderer, background_texture, NULL, &(SDL_Rect){0, 0, layout->world_width, layout->world_height});
    }
    else
    {
        draw_fallback_background(renderer, font, layout);
    }

    draw_display_texture(renderer, font, &layout->capt_pfd_rect, capt_pfd_texture, "CAPT PFD", 0);
    draw_display_texture(renderer, font, &layout->capt_nd_rect, capt_nd_texture, "CAPT ND", 1);
    draw_display_texture(renderer, font, &layout->eicas1_rect, eicas1_texture, "EICAS1", 0);
    draw_display_texture(renderer, font, &layout->fo_nd_rect, fo_nd_texture, "FO ND", 1);
    draw_display_texture(renderer, font, &layout->fo_pfd_rect, fo_pfd_texture, "FO PFD", 0);
    draw_display_texture(renderer, font, &layout->left_fmc_rect, fmc_texture, "LEFT FMC", 0);
    draw_display_texture(renderer, font, &layout->eicas2_rect, eicas2_texture, "EICAS2", 0);
    draw_display_texture(renderer, font, &layout->right_fmc_rect, fmc_texture, "RIGHT FMC", 0);

    draw_fmc_hotspot_hint(renderer, font, &layout->left_fmc_rect, "LEFT FMC");
    draw_fmc_hotspot_hint(renderer, font, &layout->right_fmc_rect, "RIGHT FMC");
}

SDL_Rect cockpit_ui_fmc_zoom_rect(int window_width, int window_height)
{
    int height = window_height - 80;
    if (height > 840)
    {
        height = 840;
    }
    if (height < 650)
    {
        height = window_height - 40;
    }

    int width = (int)((float)height * 0.70f);
    if (width > 650)
    {
        width = 650;
    }
    if (width < 460)
    {
        width = 460;
    }

    SDL_Rect rect;
    rect.w = width;
    rect.h = height;
    rect.x = (window_width - rect.w) / 2;
    rect.y = (window_height - rect.h) / 2;
    return rect;
}

SDL_Rect cockpit_ui_module_zoom_rect(int window_width, int window_height, int texture_width, int texture_height)
{
    if (texture_width <= 0)
    {
        texture_width = 1000;
    }
    if (texture_height <= 0)
    {
        texture_height = 700;
    }

    const float max_w = (float)window_width * 0.88f;
    const float max_h = (float)window_height * 0.84f;
    float scale = max_w / (float)texture_width;
    const float scale_y = max_h / (float)texture_height;
    if (scale > scale_y)
    {
        scale = scale_y;
    }
    if (scale > 1.0f)
    {
        scale = 1.0f;
    }

    SDL_Rect rect;
    rect.w = (int)((float)texture_width * scale);
    rect.h = (int)((float)texture_height * scale);
    rect.x = (window_width - rect.w) / 2;
    rect.y = (window_height - rect.h) / 2;
    return rect;
}

static void draw_fmc_debug_rect(SDL_Renderer *renderer, SDL_Rect fmc_rect, const SDL_Rect *source_rect, SDL_Color color)
{
    SDL_Rect dest = cockpit_layout_fmc_source_to_dest_rect(fmc_rect, source_rect);
    draw_rect(renderer, &dest, color);
}

static void draw_fmc_debug_overlay(SDL_Renderer *renderer, SDL_Rect fmc_rect)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    draw_rect(renderer, &fmc_rect, COLOR_CYAN);
    draw_fmc_debug_rect(renderer, fmc_rect, &COCKPIT_FMC_SCREEN_RECT, COLOR_GREEN);
    draw_fmc_debug_rect(renderer, fmc_rect, &COCKPIT_FMC_BUTTON_INIT_REF, COLOR_AMBER);
    draw_fmc_debug_rect(renderer, fmc_rect, &COCKPIT_FMC_BUTTON_RTE, COLOR_AMBER);
    draw_fmc_debug_rect(renderer, fmc_rect, &COCKPIT_FMC_BUTTON_LEGS, COLOR_AMBER);
    draw_fmc_debug_rect(renderer, fmc_rect, &COCKPIT_FMC_BUTTON_DEP_ARR, COLOR_AMBER);
    draw_fmc_debug_rect(renderer, fmc_rect, &COCKPIT_FMC_BUTTON_EXEC, COLOR_MAGENTA);
    draw_fmc_debug_rect(renderer, fmc_rect, &COCKPIT_FMC_BUTTON_CLR, COLOR_MAGENTA);
    draw_fmc_debug_rect(renderer, fmc_rect, &COCKPIT_FMC_BUTTON_DEL, COLOR_MAGENTA);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void cockpit_ui_render_module_zoom_overlay(
    SDL_Renderer *renderer,
    TTF_Font *font,
    SDL_Texture *module_texture,
    SDL_Rect zoom_rect,
    const char *label)
{
    if (renderer == NULL || font == NULL)
    {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    fill_rect(renderer, &(SDL_Rect){0, 0, 20000, 20000}, (SDL_Color){0, 0, 0, 178});
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    SDL_Rect frame = {zoom_rect.x - 18, zoom_rect.y - 18, zoom_rect.w + 36, zoom_rect.h + 56};
    fill_rect(renderer, &frame, COLOR_PANEL);
    draw_rect(renderer, &frame, COLOR_EDGE);

    fill_rect(renderer, &zoom_rect, COLOR_BLACK);
    if (module_texture != NULL)
    {
        SDL_RenderCopy(renderer, module_texture, NULL, &zoom_rect);
    }
    else
    {
        draw_centered_text(renderer, font, COLOR_DIM, &zoom_rect, "%s", label != NULL ? label : "MODULE");
    }

    draw_rect(renderer, &zoom_rect, COLOR_CYAN);
    draw_centered_text(renderer, font, COLOR_CYAN, &(SDL_Rect){frame.x, frame.y + frame.h - 35, frame.w, 28},
                       "%s - ESC / Click outside to return",
                       label != NULL ? label : "MODULE");
}

void cockpit_ui_render_fmc_zoom_overlay(
    SDL_Renderer *renderer,
    TTF_Font *font,
    SDL_Texture *fmc_texture,
    SDL_Texture *fmc_background_texture,
    SDL_Rect zoom_rect,
    Cockpit_FmcSide selected_fmc,
    int show_debug)
{
    if (renderer == NULL || font == NULL)
    {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    fill_rect(renderer, &(SDL_Rect){0, 0, 20000, 20000}, (SDL_Color){0, 0, 0, 178});
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    SDL_Rect frame = {zoom_rect.x - 18, zoom_rect.y - 18, zoom_rect.w + 36, zoom_rect.h + 56};
    fill_rect(renderer, &frame, COLOR_PANEL);
    draw_rect(renderer, &frame, COLOR_EDGE);

    if (fmc_texture != NULL)
    {
        SDL_RenderCopy(renderer, fmc_texture, NULL, &zoom_rect);
    }
    else if (fmc_background_texture != NULL)
    {
        SDL_RenderCopy(renderer, fmc_background_texture, NULL, &zoom_rect);
    }
    else
    {
        fill_rect(renderer, &zoom_rect, COLOR_BLACK);
        draw_centered_text(renderer, font, COLOR_DIM, &zoom_rect, "FMC");
    }

    if (show_debug)
    {
        draw_fmc_debug_overlay(renderer, zoom_rect);
    }

    draw_rect(renderer, &zoom_rect, COLOR_CYAN);
    draw_centered_text(renderer, font, COLOR_CYAN, &(SDL_Rect){frame.x, frame.y + frame.h - 35, frame.w, 28},
                       "%s FMC - ESC / Click outside to return",
                       selected_fmc == COCKPIT_FMC_RIGHT ? "RIGHT" : "LEFT");
}
