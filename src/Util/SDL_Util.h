#ifndef SDL_UTIL_H
#define SDL_UTIL_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h> // SDL_gfx 头文件
#include <stdio.h>

/**
 * @brief 初始化SDL2系统
 * 
 * @return int 成功返回0，失败返回负值
 */
int initSDL(void);
//定义多种大小的绘制字体
extern TTF_Font *font8; 
extern TTF_Font *font12;
extern TTF_Font *font16;
extern TTF_Font *font20;
extern TTF_Font *font24;
/**
 * @brief 绘制文本
 * @param renderer 渲染器
 * @param color 文本颜色
 * @param font 字体
 * @param x x坐标
 * @param y y坐标
 * @param text 绘制的文本
 */
void renderText(SDL_Renderer *renderer, SDL_Color color, TTF_Font *font, int x, int y, const char *text);
/**
 * @brief 销毁SDL2系统
 * @return int 成功返回0，失败返回负值
 */
int destroySDL(SDL_Renderer *renderer, SDL_Window *window);
#endif
