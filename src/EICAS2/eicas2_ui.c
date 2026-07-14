#include "eicas2_ui.h"

static void draw_vib_reference_ticks(SDL_Renderer *renderer, const EICAS_Canvas *canvas)
{
    eicas_ui_draw_line(renderer, canvas, 209.0f, 610.0f, 215.0f, 610.0f, EICAS_COLOR_WHITE);
    eicas_ui_draw_line(renderer, canvas, 333.0f, 610.0f, 327.0f, 610.0f, EICAS_COLOR_WHITE);
}

static void draw_eicas2_page(SDL_Renderer *renderer, TTF_Font *font, const EICAS_Canvas *canvas, const AircraftSystems_Data *data)
{
    const AircraftSystems_EngineData *left = &data->engine_left;
    const AircraftSystems_EngineData *right = &data->engine_right;
    const float left_fuel_flow = left->eicas2_fuel_flow_display_valid ? left->eicas2_fuel_flow_display : left->fuel_flow / 350.0f;
    const float right_fuel_flow = right->eicas2_fuel_flow_display_valid ? right->eicas2_fuel_flow_display : right->fuel_flow / 350.0f;

    eicas_ui_draw_percent_gauge_with_needle(renderer, font, canvas, 165.0f, 89.0f, left->n2, EICAS_COLOR_WHITE);
    eicas_ui_draw_percent_gauge_with_needle(renderer, font, canvas, 389.0f, 89.0f, right->n2, EICAS_COLOR_WHITE);
    eicas_ui_draw_percent_gauge_end_line(renderer, canvas, 165.0f, 89.0f, EICAS_COLOR_RED);
    eicas_ui_draw_percent_gauge_end_line(renderer, canvas, 389.0f, 89.0f, EICAS_COLOR_RED);
    eicas_ui_draw_value_box(renderer, font, canvas, 194.0f, 66.0f, 78.0f, 23.0f, "%.1f", left->n2);
    eicas_ui_draw_value_box(renderer, font, canvas, 418.0f, 66.0f, 78.0f, 23.0f, "%.1f", right->n2);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, 268.0f, 156.0f, "N2");

    eicas_ui_draw_value_box(renderer, font, canvas, 140.0f, 220.0f, 48.0f, 25.0f, "%.1f", left_fuel_flow);
    eicas_ui_draw_value_box(renderer, font, canvas, 363.0f, 220.0f, 48.0f, 25.0f, "%.1f", right_fuel_flow);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, 268.0f, 231.0f, "FF");

    eicas_ui_draw_vertical_scale_with_limit(renderer, canvas, 209.0f, 290.0f, 345.0f, left->oil_pressure, 100.0f, 0, 0.22f, 2);
    eicas_ui_draw_vertical_scale_with_limit(renderer, canvas, 333.0f, 290.0f, 345.0f, right->oil_pressure, 100.0f, 1, 0.22f, 2);
    eicas_ui_draw_value_box(renderer, font, canvas, 119.0f, 321.0f, 74.0f, 25.0f, "%.2f", left->oil_pressure);
    eicas_ui_draw_value_box(renderer, font, canvas, 357.0f, 321.0f, 74.0f, 25.0f, "%.2f", right->oil_pressure);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, 268.0f, 306.0f, "OIL");
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, 268.0f, 327.0f, "PRESS");

    eicas_ui_draw_vertical_scale_with_limit(renderer, canvas, 209.0f, 416.0f, 473.0f, left->oil_temp, 180.0f, 0, 0.96f, 1);
    eicas_ui_draw_vertical_scale_with_limit(renderer, canvas, 333.0f, 416.0f, 473.0f, right->oil_temp, 180.0f, 1, 0.96f, 1);
    eicas_ui_draw_value_box(renderer, font, canvas, 119.0f, 448.0f, 74.0f, 25.0f, "%.1f", left->oil_temp);
    eicas_ui_draw_value_box(renderer, font, canvas, 357.0f, 448.0f, 74.0f, 25.0f, "%.1f", right->oil_temp);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, 268.0f, 441.0f, "OIL");
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, 268.0f, 462.0f, "TEMP");

    eicas_ui_draw_value_box(renderer, font, canvas, 119.0f, 543.0f, 74.0f, 25.0f, "%.1f", left->oil_quantity);
    eicas_ui_draw_value_box(renderer, font, canvas, 357.0f, 543.0f, 74.0f, 25.0f, "%.1f", right->oil_quantity);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, 268.0f, 554.0f, "OIL QTY");

    eicas_ui_draw_vertical_scale_with_limit(renderer, canvas, 209.0f, 596.0f, 657.0f, left->vibration, 5.0f, 0, 0.0f, 0);
    eicas_ui_draw_vertical_scale_with_limit(renderer, canvas, 333.0f, 596.0f, 657.0f, right->vibration, 5.0f, 1, 0.0f, 0);
    draw_vib_reference_ticks(renderer, canvas);
    eicas_ui_draw_value_box(renderer, font, canvas, 119.0f, 626.0f, 74.0f, 25.0f, "%.1f", left->vibration);
    eicas_ui_draw_value_box(renderer, font, canvas, 357.0f, 626.0f, 74.0f, 25.0f, "%.1f", right->vibration);
    eicas_ui_draw_centered_text(renderer, font, canvas, EICAS_COLOR_CYAN, 268.0f, 619.0f, "VIB");
}

void eicas2_ui_render(SDL_Renderer *renderer, TTF_Font *font, const AircraftSystems_Data *data)
{
    if (renderer == NULL || font == NULL || data == NULL)
    {
        return;
    }

    EICAS_Canvas canvas = eicas_ui_begin_frame(renderer);
    draw_eicas2_page(renderer, font, &canvas, data);
}
