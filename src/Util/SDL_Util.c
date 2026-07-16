#include "SDL_Util.h"
#include "xplaneConnect.h"
TTF_Font *font8 = NULL;
TTF_Font *font12 = NULL;
TTF_Font *font16 = NULL;
TTF_Font *font20 = NULL;
TTF_Font *font24 = NULL;
int initSDL(void)
{
    // 使用最近邻缩放，避免纹理缩放时产生模糊
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) == -1)
    {
        printf("SDL初始化失败: %s\n", SDL_GetError());
        return -1;
    }
    // 初始化TTF
    if (TTF_Init() == -1)
    {
        printf("SDL 字体初始化失败: %s\n", TTF_GetError());
        return -1;
    }
    else
    {
        font8 = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 8);
        font12 = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 12);
        font16 = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 16);
        font20 = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 20);
        font24 = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 24);
        if (font8 == NULL || font12 == NULL || font16 == NULL || font20 == NULL || font24 == NULL)
        {
            printf("SDL 字体初始化失败: %s\n", TTF_GetError());
            return -1;
        }
    }
    if (IMG_Init(IMG_INIT_PNG) == -1)
    {
        printf("SDL 图片初始化失败: %s\n", IMG_GetError());
        return -1;
    }
    return 0;
}
void renderText(SDL_Renderer *renderer, SDL_Color color, TTF_Font *font, int x, int y, const char *text)
{
    // 判断参数是否为NULL
    if (!font || !text)
        return;
    // 根据字体创建一个表面，然后再由表面创建一个纹理
    SDL_Surface *surface = TTF_RenderUTF8_Solid(font, text, color);
    if (!surface)
    {
        printf("TTF_RenderText_Solid error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture)
    {
        printf("SDL_CreateTextureFromSurface error: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }
    // 将纹理绘制到渲染器上
    SDL_Rect dest_rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dest_rect);
    // 释放资源
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}
int destroySDL(SDL_Renderer *renderer, SDL_Window *window)
{
    // 清理字体
    TTF_CloseFont(font8);
    TTF_CloseFont(font12);
    TTF_CloseFont(font16);
    TTF_CloseFont(font20);
    TTF_CloseFont(font24);
    // 退出SDL
    TTF_Quit();
    IMG_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
