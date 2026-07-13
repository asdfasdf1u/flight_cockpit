#ifndef ND_UI_H
#define ND_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "nd_data.h"

typedef struct ND_UIRenderStats
{
    int displayed_wpt;
    int displayed_arpt;
    int displayed_sta;
    int labels_drawn;
    int labels_hidden_compact;
    int filtered_invalid;
    int filtered_category;
    int filtered_layer;
    int filtered_distance;
    int filtered_bearing;
    int filtered_bounds;
    int filtered_symbol_overlap;
    int labels_hidden_conflict;
    int route_points_drawn;
    int route_segments_drawn;
} ND_UIRenderStats;

void nd_ui_render(SDL_Renderer *renderer, TTF_Font *font, const ND_Data *data);
void nd_ui_get_last_render_stats(ND_UIRenderStats *stats);

#endif
