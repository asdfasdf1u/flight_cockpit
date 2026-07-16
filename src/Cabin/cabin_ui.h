#ifndef CABIN_UI_H
#define CABIN_UI_H

#include <stddef.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "cabin_data.h"

typedef struct Cabin_Assets
{
    SDL_Texture *map_texture;
    SDL_Texture *plane_texture;
    SDL_Texture *fullscreen_texture;
    SDL_Texture *add_texture;
    SDL_Texture *sub_texture;
    TTF_Font *title_font;
    TTF_Font *emergency_font;
    TTF_Font *font;
    TTF_Font *small_font;
} Cabin_Assets;

void cabin_ui_handle_event(SDL_Window *window, const SDL_Event *event, Cabin_Data *data);
void cabin_ui_complete_map_refresh(int success);
void cabin_ui_render(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data);
int cabin_ui_clip_line_to_rect(const SDL_Rect *rect, double *x0, double *y0, double *x1, double *y1);
void cabin_ui_format_wind_power(const char *wind_power, char *text, size_t text_size);

typedef struct Cabin_ApiKeyDialogResult
{
    int confirmed;
    int remember;
    char api_key[256];
} Cabin_ApiKeyDialogResult;

int cabin_ui_run_apikey_dialog(SDL_Window *window,
                               SDL_Renderer *renderer,
                               const Cabin_Assets *assets,
                               const Cabin_Data *data,
                               TTF_Font *title_font,
                               TTF_Font *font,
                               TTF_Font *small_font,
                               Cabin_ApiKeyDialogResult *result);

#endif
