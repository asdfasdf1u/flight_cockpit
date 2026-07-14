#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "../src/Cabin/cabin_data.h"
#include "../src/Cabin/cabin_ui.h"

static Uint32 push_paste_event(Uint32 interval, void *param)
{
    (void)interval;
    (void)param;

    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.type = SDL_KEYDOWN;
    event.key.state = SDL_PRESSED;
    event.key.keysym.sym = SDLK_v;
    event.key.keysym.mod = KMOD_CTRL;
    SDL_PushEvent(&event);
    return 0;
}

static Uint32 push_return_event(Uint32 interval, void *param)
{
    (void)interval;
    (void)param;

    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.type = SDL_KEYDOWN;
    event.key.state = SDL_PRESSED;
    event.key.keysym.sym = SDLK_RETURN;
    SDL_PushEvent(&event);
    return 0;
}

static TTF_Font *open_font(int size)
{
    TTF_Font *font = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", size);
    if (font != NULL)
    {
        return font;
    }
    font = TTF_OpenFont("C:/Windows/Fonts/msyh.ttc", size);
    if (font != NULL)
    {
        return font;
    }
    return TTF_OpenFont("C:/Windows/Fonts/arial.ttf", size);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0 || TTF_Init() != 0)
    {
        printf("init_failed\n");
        return 2;
    }

    SDL_Window *window = SDL_CreateWindow("cabin-dialog-test",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          960,
                                          540,
                                          SDL_WINDOW_HIDDEN);
    SDL_Renderer *renderer = window != NULL ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : NULL;
    TTF_Font *title_font = open_font(24);
    TTF_Font *font = open_font(20);
    TTF_Font *small_font = open_font(17);
    if (window == NULL || renderer == NULL || title_font == NULL || font == NULL || small_font == NULL)
    {
        printf("setup_failed\n");
        return 2;
    }

    Cabin_Data data;
    cabin_data_init(&data);
    SDL_SetClipboardText("PASTEKEY123");
    Cabin_Assets assets = {0};
    assets.title_font = title_font;
    assets.font = font;
    assets.small_font = small_font;

    Cabin_ApiKeyDialogResult result;
    SDL_AddTimer(120, push_paste_event, NULL);
    SDL_AddTimer(260, push_return_event, NULL);
    const int rc = cabin_ui_run_apikey_dialog(window,
                                              renderer,
                                              &assets,
                                              &data,
                                              title_font,
                                              font,
                                              small_font,
                                              &result);
    printf("rc=%d confirmed=%d remember=%d key=%s key_len=%d\n",
           rc,
           result.confirmed,
           result.remember,
           result.api_key,
           (int)SDL_strlen(result.api_key));

    TTF_CloseFont(small_font);
    TTF_CloseFont(font);
    TTF_CloseFont(title_font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
