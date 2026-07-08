#ifndef EICAS_UI_COMMON_H
#define EICAS_UI_COMMON_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef struct EICAS_Canvas
{
    float scale;
    int x;
    int y;
} EICAS_Canvas;

#define EICAS_BASE_SIZE 768.0f

extern const SDL_Color EICAS_COLOR_BG;
extern const SDL_Color EICAS_COLOR_DIAL;
extern const SDL_Color EICAS_COLOR_CYAN;
extern const SDL_Color EICAS_COLOR_GREEN;
extern const SDL_Color EICAS_COLOR_RED;
extern const SDL_Color EICAS_COLOR_WHITE;
extern const SDL_Color EICAS_COLOR_FRAME_DIM;

float eicas_ui_clamp_float(float value, float min_value, float max_value);
void eicas_ui_set_color(SDL_Renderer *renderer, SDL_Color color);
void eicas_ui_fill_rect(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float x, float y, float w, float h, SDL_Color color);
void eicas_ui_draw_rect(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float x, float y, float w, float h, SDL_Color color);
void eicas_ui_draw_line(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float x1, float y1, float x2, float y2, SDL_Color color);
void eicas_ui_draw_text(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, SDL_Color color, float x, float y, const char *format, ...);
void eicas_ui_draw_centered_text(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, SDL_Color color, float center_x, float y, const char *format, ...);
void eicas_ui_draw_value_box(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, float x, float y, float w, float h, const char *format, ...);
void eicas_ui_fill_lower_semicircle(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float cx, float cy, float radius, SDL_Color color);
void eicas_ui_draw_arc(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float cx, float cy, float radius, float start_deg, float end_deg, SDL_Color color);
void eicas_ui_draw_percent_gauge(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, float cx, float cy, float value);
void eicas_ui_draw_percent_gauge_with_needle(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, float cx, float cy, float value, SDL_Color needle_color);
void eicas_ui_draw_percent_gauge_end_line(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float cx, float cy, SDL_Color color);
void eicas_ui_draw_percent_gauge_end_y(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, float cx, float cy);
void eicas_ui_draw_egt_gauge(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float cx, float cy, float egt);
void eicas_ui_draw_egt_gauge_with_needle(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float cx, float cy, float egt, SDL_Color needle_color);
void eicas_ui_draw_vertical_scale(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float x, float top, float bottom, float value, float max_value, int ticks_left);
void eicas_ui_draw_vertical_scale_with_limit(SDL_Renderer *renderer, const EICAS_Canvas *canvas, float x, float top, float bottom, float value, float max_value, int ticks_left, float limit_ratio, int limit_line_count);
EICAS_Canvas eicas_ui_begin_frame(SDL_Renderer *renderer);

#endif
