#include "eicas1_ui.h"

#include <math.h>

static const SDL_Color EICAS_COLOR_AMBER = {255, 198, 64, 255};

static SDL_Color warning_color(AircraftSystems_WarningLevel level)
{
    switch (level)
    {
    case AIRCRAFT_SYSTEMS_WARNING_WARNING:
        return EICAS_COLOR_RED;
    case AIRCRAFT_SYSTEMS_WARNING_CAUTION:
        return EICAS_COLOR_AMBER;
    case AIRCRAFT_SYSTEMS_WARNING_INFO:
    default:
        return EICAS_COLOR_WHITE;
    }
}

static const char *warning_level_text(AircraftSystems_WarningLevel level)
{
    switch (level)
    {
    case AIRCRAFT_SYSTEMS_WARNING_WARNING:
        return "WARNING";
    case AIRCRAFT_SYSTEMS_WARNING_CAUTION:
        return "CAUTION";
    case AIRCRAFT_SYSTEMS_WARNING_INFO:
    default:
        return "INFO";
    }
}

static void draw_engine_status_boxes(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas)
{
    const float w = 131.0f;
    const float h = 75.0f;
    const float y = 76.0f;
    const float left_x = 439.0f;
    const float right_x = 589.0f;

    eicas_ui_draw_rect(renderer, canvas, left_x, y, w, h, EICAS_COLOR_FRAME_DIM);
    eicas_ui_draw_rect(renderer, canvas, right_x, y, w, h, EICAS_COLOR_FRAME_DIM);
    eicas_ui_draw_line(renderer, canvas, left_x, y + 25.0f, left_x + w, y + 25.0f, EICAS_COLOR_FRAME_DIM);
    eicas_ui_draw_line(renderer, canvas, left_x, y + 50.0f, left_x + w, y + 50.0f, EICAS_COLOR_FRAME_DIM);
    eicas_ui_draw_line(renderer, canvas, right_x, y + 25.0f, right_x + w, y + 25.0f, EICAS_COLOR_FRAME_DIM);
    eicas_ui_draw_line(renderer, canvas, right_x, y + 50.0f, right_x + w, y + 50.0f, EICAS_COLOR_FRAME_DIM);

    eicas_ui_fill_rect(renderer, canvas, left_x + 43.0f, y - 24.0f, 48.0f, 16.0f, EICAS_COLOR_BG);
    eicas_ui_fill_rect(renderer, canvas, right_x + 43.0f, y - 24.0f, 48.0f, 16.0f, EICAS_COLOR_BG);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, left_x + w * 0.5f, y - 26.0f, "ENG1");
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, right_x + w * 0.5f, y - 26.0f, "ENG2");
}

static void draw_warning_summary(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, const AircraftSystems_Data *data)
{
    const float x = 449.0f;
    const float y = 184.0f;
    const float row_h = 30.0f;
    const int max_rows = 6;

    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, x + 130.0f, y, "WARNINGS");

    int shown = 0;
    for (int i = 0; i < data->warning_count && shown < max_rows; ++i)
    {
        const AircraftSystems_WarningItem *item = &data->warnings[i];
        if (!item->active)
        {
            continue;
        }

        const float row_y = y + 28.0f + row_h * (float)shown;
        const SDL_Color color = warning_color(item->level);
        eicas_ui_draw_text(renderer, font, canvas, color, x, row_y, "%s", warning_level_text(item->level));
        eicas_ui_draw_text(renderer, font, canvas, color, x + 93.0f, row_y, "%s", item->text);
        ++shown;
    }

    if (shown == 0)
    {
        eicas_ui_draw_text(renderer, font, canvas, EICAS_COLOR_WHITE, x, y + 28.0f, "INFO");
        eicas_ui_draw_text(renderer, font, canvas, EICAS_COLOR_WHITE, x + 93.0f, y + 28.0f, "NORMAL");
    }
}

static void draw_fuel_quantity_panel(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, const AircraftSystems_Data *data)
{
    const float x = 449.0f;
    const float y = 621.0f;
    const float w = 260.0f;
    const float h = 72.0f;
    const float left_qty = data->fuel_tank_quantities_valid ? data->fuel_left_quantity : data->fuel_quantity * 48.7f;
    const float center_qty = data->fuel_tank_quantities_valid ? data->fuel_center_quantity : data->fuel_quantity * 60.9f;
    const float right_qty = data->fuel_tank_quantities_valid ? data->fuel_right_quantity : data->fuel_quantity * 48.7f;
    const float total_qty = data->fuel_tank_quantities_valid ? data->fuel_total_quantity : left_qty + center_qty + right_qty;

    eicas_ui_draw_rect(renderer, canvas, x, y, w, h, EICAS_COLOR_CYAN);
    eicas_ui_fill_rect(renderer, canvas, x + 31.0f, y - 8.0f, 195.0f, 16.0f, EICAS_COLOR_BG);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, x + w * 0.5f, y - 11.0f, "FUEL QTY-LBSX1000");
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_WHITE, x + 48.0f, y + 22.0f, "%.2f", left_qty);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_WHITE, x + 130.0f, y + 22.0f, "%.2f", center_qty);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_WHITE, x + 212.0f, y + 22.0f, "%.2f", right_qty);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, x + 66.0f, y + 84.0f, "TOTAL");
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_WHITE, x + 137.0f, y + 84.0f, "%.1f", total_qty);
}

static void draw_eicas1_page(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, const AircraftSystems_Data *data)
{
    const AircraftSystems_EngineData *left = &data->engine_left;
    const AircraftSystems_EngineData *right = &data->engine_right;
    const float left_fuel_flow = left->eicas1_fuel_flow_display_valid ? left->eicas1_fuel_flow_display : left->fuel_flow / 500.0f;
    const float right_fuel_flow = right->eicas1_fuel_flow_display_valid ? right->eicas1_fuel_flow_display : right->fuel_flow / 500.0f;

    eicas_ui_draw_text(renderer, font, canvas, EICAS_COLOR_CYAN, 54.0f, 46.0f, "TAT");
    eicas_ui_draw_text(renderer, font, canvas, EICAS_COLOR_WHITE, 151.0f, 46.0f, "%.1f C", data->total_air_temperature);

    eicas_ui_draw_percent_gauge_with_needle(renderer, font, canvas, 91.0f, 147.0f, left->n1, EICAS_COLOR_WHITE);
    eicas_ui_draw_percent_gauge_with_needle(renderer, font, canvas, 315.0f, 147.0f, right->n1, EICAS_COLOR_WHITE);
    eicas_ui_draw_percent_gauge_end_y(renderer, font, canvas, 91.0f, 147.0f);
    eicas_ui_draw_percent_gauge_end_y(renderer, font, canvas, 315.0f, 147.0f);
    eicas_ui_draw_value_box(renderer, font, canvas, 130.0f, 130.0f, 60.0f, 22.0f, "%.0f", left->n1);
    eicas_ui_draw_value_box(renderer, font, canvas, 353.0f, 130.0f, 60.0f, 22.0f, "%.0f", right->n1);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, 204.0f, 238.0f, "N1");

    eicas_ui_draw_egt_gauge_with_needle(renderer, canvas, 91.0f, 322.0f, left->egt, EICAS_COLOR_WHITE);
    eicas_ui_draw_egt_gauge_with_needle(renderer, canvas, 315.0f, 322.0f, right->egt, EICAS_COLOR_WHITE);
    eicas_ui_draw_value_box(renderer, font, canvas, 130.0f, 294.0f, 60.0f, 22.0f, "%.0f", left->egt);
    eicas_ui_draw_value_box(renderer, font, canvas, 353.0f, 294.0f, 60.0f, 22.0f, "%.0f", right->egt);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, 204.0f, 401.0f, "EGT");

    eicas_ui_draw_value_box(renderer, font, canvas, 60.0f, 501.0f, 60.0f, 26.0f, "%.2f", left_fuel_flow);
    eicas_ui_draw_value_box(renderer, font, canvas, 283.0f, 501.0f, 60.0f, 26.0f, "%.2f", right_fuel_flow);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, 204.0f, 532.0f, "FF");

    draw_engine_status_boxes(renderer, font, canvas);
    draw_warning_summary(renderer, font, canvas, data);
    draw_fuel_quantity_panel(renderer, font, canvas, data);
}

void eicas1_ui_render(SDL_Renderer *renderer, TTF_Font *font, const AircraftSystems_Data *data)
{
    if (renderer == NULL || font == NULL || data == NULL)
    {
        return;
    }

    EICAS_Canvas canvas = eicas_ui_begin_frame(renderer);
    draw_eicas1_page(renderer, font, &canvas, data);
}
