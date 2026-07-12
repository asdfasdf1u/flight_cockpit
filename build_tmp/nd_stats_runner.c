#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "../src/ND/nd_data.h"
#include "../src/ND/nd_ui.h"

static TTF_Font *open_font(void)
{
    TTF_Font *font = TTF_OpenFont("assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", 18);
    if (font != NULL) return font;
    font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 18);
    if (font != NULL) return font;
    return TTF_OpenFont("C:/Windows/Fonts/simhei.ttf", 18);
}

static void render_and_print(SDL_Renderer *renderer, TTF_Font *font, ND_Data *data, const char *label)
{
    ND_UIRenderStats stats;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    nd_ui_render(renderer, font, data);
    SDL_RenderPresent(renderer);
    nd_ui_get_last_render_stats(&stats);
    printf("%s WPT=%d ARPT=%d STA=%d labels=%d compact_hidden=%d distance=%d bearing=%d bounds=%d symbol_overlap=%d\n",
           label,
           stats.displayed_wpt, stats.displayed_arpt, stats.displayed_sta,
           stats.labels_drawn, stats.labels_hidden_compact,
           stats.filtered_distance, stats.filtered_bearing, stats.filtered_bounds,
           stats.filtered_symbol_overlap);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0 || TTF_Init() != 0) return 2;
    SDL_Window *window = SDL_CreateWindow("nd-stats", 0, 0, 752, 752, SDL_WINDOW_HIDDEN);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : NULL;
    TTF_Font *font = open_font();
    if (!window || !renderer || !font) return 2;
    ND_Data data;
    nd_data_init(&data);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    nd_ui_render(renderer, font, &data);
    SDL_RenderPresent(renderer);
    ND_UIRenderStats stats;
    nd_ui_get_last_render_stats(&stats);
    SDL_Surface *compact_surface = SDL_CreateRGBSurfaceWithFormat(0, 752, 752, 32, SDL_PIXELFORMAT_ARGB8888);
    if (compact_surface != NULL)
    {
        SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                             compact_surface->pixels, compact_surface->pitch);
        SDL_SaveBMP(compact_surface, "build_tmp/nd_compact.bmp");
        SDL_FreeSurface(compact_surface);
    }
    printf("compact WPT=%d ARPT=%d STA=%d labels=%d compact_hidden=%d distance=%d bearing=%d bounds=%d symbol_overlap=%d\n",
           stats.displayed_wpt, stats.displayed_arpt, stats.displayed_sta,
           stats.labels_drawn, stats.labels_hidden_compact,
           stats.filtered_distance, stats.filtered_bearing, stats.filtered_bounds,
           stats.filtered_symbol_overlap);
    nd_data_toggle_map_labels_visible(&data);
    SDL_RenderClear(renderer);
    nd_ui_render(renderer, font, &data);
    SDL_RenderPresent(renderer);
    nd_ui_get_last_render_stats(&stats);
    SDL_Surface *labels_surface = SDL_CreateRGBSurfaceWithFormat(0, 752, 752, 32, SDL_PIXELFORMAT_ARGB8888);
    if (labels_surface != NULL)
    {
        SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                             labels_surface->pixels, labels_surface->pitch);
        SDL_SaveBMP(labels_surface, "build_tmp/nd_labels.bmp");
        SDL_FreeSurface(labels_surface);
    }
    printf("labels WPT=%d ARPT=%d STA=%d labels=%d compact_hidden=%d distance=%d bearing=%d bounds=%d symbol_overlap=%d\n",
           stats.displayed_wpt, stats.displayed_arpt, stats.displayed_sta,
           stats.labels_drawn, stats.labels_hidden_compact,
           stats.filtered_distance, stats.filtered_bearing, stats.filtered_bounds,
           stats.filtered_symbol_overlap);
    nd_data_toggle_map_layer_visible(&data, ND_MAP_LAYER_WPT);
    render_and_print(renderer, font, &data, "labels no_wpt");
    nd_data_toggle_map_layer_visible(&data, ND_MAP_LAYER_WPT);
    nd_data_toggle_map_layer_visible(&data, ND_MAP_LAYER_ARPT);
    render_and_print(renderer, font, &data, "labels no_arpt");
    nd_data_toggle_map_layer_visible(&data, ND_MAP_LAYER_ARPT);
    nd_data_toggle_map_layer_visible(&data, ND_MAP_LAYER_STA);
    render_and_print(renderer, font, &data, "labels no_sta");
    nd_data_toggle_map_layer_visible(&data, ND_MAP_LAYER_WPT);
    nd_data_toggle_map_layer_visible(&data, ND_MAP_LAYER_ARPT);
    render_and_print(renderer, font, &data, "labels only_none");
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
