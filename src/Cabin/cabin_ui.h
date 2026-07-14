#ifndef CABIN_UI_H
#define CABIN_UI_H

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

void cabin_ui_handle_event(SDL_Window *window, const SDL_Event *event);
void cabin_ui_render(SDL_Renderer *renderer, const Cabin_Assets *assets, const Cabin_Data *data);

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
