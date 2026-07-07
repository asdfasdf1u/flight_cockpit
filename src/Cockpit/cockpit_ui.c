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

static void draw_display_texture(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, SDL_Texture *texture, const char *label)
{
    SDL_Rect bezel = {rect->x - 18, rect->y - 22, rect->w + 36, rect->h + 54};
    fill_rect(renderer, &bezel, COLOR_BEZEL);
    draw_rect(renderer, &bezel, COLOR_EDGE);

    fill_rect(renderer, rect, COLOR_BLACK);
    if (texture != NULL)
    {
        SDL_RenderCopy(renderer, texture, NULL, rect);
    }
    else
    {
        draw_centered_text(renderer, font, COLOR_DIM, rect, "%s", label);
    }

    draw_rect(renderer, rect, COLOR_DIM);
    draw_centered_text(renderer, font, COLOR_CYAN, &(SDL_Rect){bezel.x, bezel.y + bezel.h - 28, bezel.w, 22}, "%s", label);
}

static void draw_lower_eicas_placeholder(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect)
{
    SDL_Rect bezel = {rect->x - 18, rect->y - 22, rect->w + 36, rect->h + 54};
    fill_rect(renderer, &bezel, COLOR_BEZEL);
    draw_rect(renderer, &bezel, COLOR_EDGE);
    fill_rect(renderer, rect, COLOR_BLACK);
    draw_rect(renderer, rect, COLOR_DIM);

    draw_centered_text(renderer, font, COLOR_CYAN, &(SDL_Rect){rect->x, rect->y + 28, rect->w, 34}, "EICAS LOWER");
    for (int i = 0; i < 6; ++i)
    {
        const int y = rect->y + 115 + i * 72;
        draw_text(renderer, font, COLOR_DIM, rect->x + 85, y, "SYS %d", i + 1);
        draw_text(renderer, font, COLOR_GREEN, rect->x + rect->w - 250, y, "NORMAL");
    }
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
    SDL_Texture *eicas_texture,
    SDL_Texture *fo_nd_texture,
    SDL_Texture *fo_pfd_texture,
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

    draw_display_texture(renderer, font, &layout->capt_pfd_rect, capt_pfd_texture, "CAPT PFD");
    draw_display_texture(renderer, font, &layout->capt_nd_rect, capt_nd_texture, "CAPT ND");
    draw_display_texture(renderer, font, &layout->eicas_rect, eicas_texture, "EICAS");
    draw_display_texture(renderer, font, &layout->fo_nd_rect, fo_nd_texture, "FO ND");
    draw_display_texture(renderer, font, &layout->fo_pfd_rect, fo_pfd_texture, "FO PFD");
    draw_display_texture(renderer, font, &layout->left_fmc_rect, fmc_texture, "LEFT FMC");
    draw_lower_eicas_placeholder(renderer, font, &layout->lower_eicas_rect);
    draw_display_texture(renderer, font, &layout->right_fmc_rect, fmc_texture, "RIGHT FMC");

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

void cockpit_ui_render_fmc_zoom_overlay(
    SDL_Renderer *renderer,
    TTF_Font *font,
    SDL_Texture *fmc_texture,
    SDL_Texture *fmc_background_texture,
    SDL_Rect zoom_rect,
    Cockpit_FmcSide selected_fmc)
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

    if (fmc_background_texture != NULL)
    {
        SDL_RenderCopy(renderer, fmc_background_texture, NULL, &zoom_rect);
    }
    else if (fmc_texture != NULL)
    {
        SDL_RenderCopy(renderer, fmc_texture, NULL, &zoom_rect);
    }
    else
    {
        fill_rect(renderer, &zoom_rect, COLOR_BLACK);
        draw_centered_text(renderer, font, COLOR_DIM, &zoom_rect, "FMC");
    }

    if (fmc_background_texture != NULL && fmc_texture != NULL)
    {
        SDL_Rect screen = {
            zoom_rect.x + zoom_rect.w * 15 / 100,
            zoom_rect.y + zoom_rect.h * 7 / 100,
            zoom_rect.w * 70 / 100,
            zoom_rect.h * 43 / 100};
        SDL_RenderCopy(renderer, fmc_texture, NULL, &screen);
        draw_rect(renderer, &screen, COLOR_DIM);
    }

    draw_rect(renderer, &zoom_rect, COLOR_CYAN);
    draw_centered_text(renderer, font, COLOR_CYAN, &(SDL_Rect){frame.x, frame.y + frame.h - 35, frame.w, 28},
                       "%s FMC - ESC / Click outside to return",
                       selected_fmc == COCKPIT_FMC_RIGHT ? "RIGHT" : "LEFT");
}
