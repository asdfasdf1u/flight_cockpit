#include "eicas_ui.h"

#include <stdarg.h>
#include <stdio.h>

static const SDL_Color COLOR_BG = {4, 8, 11, 255};
static const SDL_Color COLOR_PANEL = {13, 22, 28, 255};
static const SDL_Color COLOR_DIM = {105, 128, 128, 255};
static const SDL_Color COLOR_CYAN = {70, 210, 255, 255};
static const SDL_Color COLOR_GREEN = {90, 255, 135, 255};
static const SDL_Color COLOR_AMBER = {255, 190, 65, 255};
static const SDL_Color COLOR_RED = {255, 70, 70, 255};
static const SDL_Color COLOR_WHITE = {245, 250, 248, 255};

static float clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    if (value > max_value)
    {
        return max_value;
    }

    return value;
}

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

    char text[160];
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

    char text[160];
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

static void draw_panel(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const char *title)
{
    fill_rect(renderer, rect, COLOR_PANEL);
    draw_rect(renderer, rect, COLOR_CYAN);
    draw_text(renderer, font, COLOR_CYAN, rect->x + 14, rect->y + 10, "%s", title);
}

static SDL_Color value_color(float value, float caution_min, float warning_min, float warning_max)
{
    if (value < warning_min || value > warning_max)
    {
        return COLOR_RED;
    }

    if (value < caution_min)
    {
        return COLOR_AMBER;
    }

    return COLOR_GREEN;
}

static void draw_bar(SDL_Renderer *renderer, const SDL_Rect *rect, float value, float max_value, SDL_Color fill_color)
{
    const float ratio = clamp_float(value / max_value, 0.0f, 1.0f);
    SDL_Rect fill = {rect->x + 3, rect->y + 3, (int)((float)(rect->w - 6) * ratio), rect->h - 6};

    fill_rect(renderer, rect, (SDL_Color){5, 12, 16, 255});
    fill_rect(renderer, &fill, fill_color);
    draw_rect(renderer, rect, COLOR_WHITE);
}

static void draw_engine_metric(SDL_Renderer *renderer, TTF_Font *font, int x, int y, const char *label, float value, const char *unit, SDL_Color color)
{
    draw_text(renderer, font, COLOR_DIM, x, y, "%s", label);
    draw_text(renderer, font, color, x + 118, y, "%6.1f", value);
    draw_text(renderer, font, COLOR_DIM, x + 198, y, "%s", unit);
}

static void draw_engine_panel(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const char *title, const EICAS_EngineData *engine)
{
    draw_panel(renderer, font, rect, title);

    const int x = rect->x + 22;
    int y = rect->y + 54;

    draw_text(renderer, font, engine->running ? COLOR_GREEN : COLOR_RED, rect->x + rect->w - 112, rect->y + 10,
              "%s", engine->running ? "RUNNING" : "SHUTDOWN");

    draw_engine_metric(renderer, font, x, y, "N1", engine->n1, "%", value_color(engine->n1, 45.0f, 20.0f, 103.0f));
    draw_bar(renderer, &(SDL_Rect){x, y + 28, rect->w - 44, 18}, engine->n1, 110.0f, value_color(engine->n1, 45.0f, 20.0f, 103.0f));

    y += 62;
    draw_engine_metric(renderer, font, x, y, "N2", engine->n2, "%", value_color(engine->n2, 55.0f, 25.0f, 104.0f));
    draw_bar(renderer, &(SDL_Rect){x, y + 28, rect->w - 44, 18}, engine->n2, 110.0f, value_color(engine->n2, 55.0f, 25.0f, 104.0f));

    y += 62;
    draw_engine_metric(renderer, font, x, y, "EGT", engine->egt, "C", engine->egt > 820.0f ? COLOR_RED : COLOR_GREEN);
    draw_bar(renderer, &(SDL_Rect){x, y + 28, rect->w - 44, 18}, engine->egt, 950.0f, engine->egt > 820.0f ? COLOR_RED : COLOR_GREEN);

    y += 62;
    draw_engine_metric(renderer, font, x, y, "FUEL FLOW", engine->fuel_flow, "PPH", COLOR_GREEN);

    y += 34;
    draw_engine_metric(renderer, font, x, y, "OIL PRESS", engine->oil_pressure, "PSI", engine->oil_pressure < 35.0f ? COLOR_RED : COLOR_GREEN);

    y += 34;
    draw_engine_metric(renderer, font, x, y, "OIL TEMP", engine->oil_temp, "C", engine->oil_temp > 140.0f ? COLOR_AMBER : COLOR_GREEN);
}

static void draw_fuel_panel(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const EICAS_Data *data)
{
    draw_panel(renderer, font, rect, "FUEL");

    SDL_Color fuel_color = COLOR_GREEN;
    if (data->fuel_quantity < 20.0f)
    {
        fuel_color = COLOR_AMBER;
    }

    draw_text(renderer, font, fuel_color, rect->x + 28, rect->y + 48, "%5.1f%%", data->fuel_quantity);
    draw_bar(renderer, &(SDL_Rect){rect->x + 28, rect->y + 88, rect->w - 56, 34}, data->fuel_quantity, 100.0f, fuel_color);
    draw_text(renderer, font, COLOR_DIM, rect->x + 28, rect->y + 134, "TOTAL FUEL QUANTITY");
}

static void draw_config_panel(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const EICAS_Data *data)
{
    draw_panel(renderer, font, rect, "CONFIG");

    draw_text(renderer, font, COLOR_DIM, rect->x + 28, rect->y + 52, "GEAR");
    draw_text(renderer, font, data->gear_down ? COLOR_GREEN : COLOR_AMBER, rect->x + 160, rect->y + 52, "%s", data->gear_down ? "DOWN" : "UP");

    draw_text(renderer, font, COLOR_DIM, rect->x + 28, rect->y + 86, "FLAPS");
    draw_text(renderer, font, COLOR_GREEN, rect->x + 160, rect->y + 86, "%d", data->flaps_level);

    draw_text(renderer, font, COLOR_DIM, rect->x + 28, rect->y + 120, "PARK BRK");
    draw_text(renderer, font, data->parking_brake_on ? COLOR_AMBER : COLOR_GREEN, rect->x + 160, rect->y + 120,
              "%s", data->parking_brake_on ? "ON" : "OFF");
}

static void draw_system_panel(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const EICAS_Data *data)
{
    draw_panel(renderer, font, rect, "SYSTEM");

    draw_text(renderer, font, COLOR_DIM, rect->x + 28, rect->y + 52, "HYD PRESS");
    draw_text(renderer, font, data->hydraulic_pressure < 2500.0f ? COLOR_AMBER : COLOR_GREEN, rect->x + 170, rect->y + 52,
              "%5.0f PSI", data->hydraulic_pressure);

    draw_text(renderer, font, COLOR_DIM, rect->x + 28, rect->y + 88, "CABIN PRESS");
    draw_text(renderer, font, COLOR_GREEN, rect->x + 170, rect->y + 88, "%4.1f PSI", data->cabin_pressure);

    draw_text(renderer, font, COLOR_DIM, rect->x + 28, rect->y + 124, "BATTERY");
    draw_text(renderer, font, data->battery_voltage < 24.0f ? COLOR_AMBER : COLOR_GREEN, rect->x + 170, rect->y + 124,
              "%4.1f V", data->battery_voltage);
}

static SDL_Color warning_color(EICAS_WarningLevel level)
{
    if (level == EICAS_WARNING_WARNING)
    {
        return COLOR_RED;
    }

    if (level == EICAS_WARNING_CAUTION)
    {
        return COLOR_AMBER;
    }

    return COLOR_GREEN;
}

static const char *warning_level_text(EICAS_WarningLevel level)
{
    if (level == EICAS_WARNING_WARNING)
    {
        return "WARNING";
    }

    if (level == EICAS_WARNING_CAUTION)
    {
        return "CAUTION";
    }

    return "INFO";
}

static void draw_warning_panel(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect *rect, const EICAS_Data *data)
{
    draw_panel(renderer, font, rect, "CREW ALERTING");

    for (int i = 0; i < data->warning_count && i < EICAS_MAX_WARNINGS; ++i)
    {
        const EICAS_WarningItem *item = &data->warnings[i];
        if (!item->active)
        {
            continue;
        }

        const int y = rect->y + 48 + i * 34;
        const SDL_Color color = warning_color(item->level);
        draw_text(renderer, font, color, rect->x + 28, y, "%-8s", warning_level_text(item->level));
        draw_text(renderer, font, color, rect->x + 142, y, "%s", item->text);
    }
}

void eicas_ui_render(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Data *data)
{
    if (renderer == NULL || font == NULL || data == NULL)
    {
        return;
    }

    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    if (width <= 0 || height <= 0)
    {
        width = 1000;
        height = 700;
    }

    fill_rect(renderer, &(SDL_Rect){0, 0, width, height}, COLOR_BG);

    const SDL_Rect title_rect = {30, 20, 940, 54};
    const SDL_Rect eng1_rect = {30, 95, 455, 330};
    const SDL_Rect eng2_rect = {515, 95, 455, 330};
    const SDL_Rect fuel_rect = {30, 445, 300, 165};
    const SDL_Rect config_rect = {350, 445, 290, 165};
    const SDL_Rect system_rect = {660, 445, 310, 165};
    const SDL_Rect warning_rect = {30, 625, 940, 55};

    fill_rect(renderer, &title_rect, COLOR_PANEL);
    draw_rect(renderer, &title_rect, COLOR_CYAN);
    draw_centered_text(renderer, font, COLOR_CYAN, &title_rect, "EICAS - Engine Indication and Crew Alerting System");

    draw_engine_panel(renderer, font, &eng1_rect, "ENG 1", &data->engine_left);
    draw_engine_panel(renderer, font, &eng2_rect, "ENG 2", &data->engine_right);
    draw_fuel_panel(renderer, font, &fuel_rect, data);
    draw_config_panel(renderer, font, &config_rect, data);
    draw_system_panel(renderer, font, &system_rect, data);
    draw_warning_panel(renderer, font, &warning_rect, data);
}
