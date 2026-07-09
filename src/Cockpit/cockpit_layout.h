#ifndef COCKPIT_LAYOUT_H
#define COCKPIT_LAYOUT_H

#include <SDL2/SDL.h>

typedef enum Cockpit_FmcSide
{
    COCKPIT_FMC_NONE,
    COCKPIT_FMC_LEFT,
    COCKPIT_FMC_RIGHT
} Cockpit_FmcSide;

#define COCKPIT_FMC_IMAGE_WIDTH 638
#define COCKPIT_FMC_IMAGE_HEIGHT 998

extern const SDL_Rect COCKPIT_FMC_SCREEN_RECT;
extern const SDL_Rect COCKPIT_FMC_BUTTON_RTE;
extern const SDL_Rect COCKPIT_FMC_BUTTON_LEGS;
extern const SDL_Rect COCKPIT_FMC_BUTTON_DEP_ARR;
extern const SDL_Rect COCKPIT_FMC_BUTTON_INIT_REF;
extern const SDL_Rect COCKPIT_FMC_BUTTON_EXEC;
extern const SDL_Rect COCKPIT_FMC_BUTTON_CLR;
extern const SDL_Rect COCKPIT_FMC_BUTTON_DEL;

typedef struct Cockpit_Layout
{
    int world_width;
    int world_height;

    SDL_Rect capt_pfd_rect;
    SDL_Rect capt_nd_rect;
    SDL_Rect eicas1_rect;
    SDL_Rect fo_nd_rect;
    SDL_Rect fo_pfd_rect;
    SDL_Rect left_fmc_rect;
    SDL_Rect eicas2_rect;
    SDL_Rect right_fmc_rect;
} Cockpit_Layout;

const char *cockpit_layout_background_path(void);
const char *cockpit_layout_fmc_background_path(void);
Cockpit_Layout cockpit_layout_default(int world_width, int world_height);
Cockpit_FmcSide cockpit_layout_hit_test_fmc(const Cockpit_Layout *layout, float world_x, float world_y);
SDL_Rect cockpit_layout_fmc_source_to_dest_rect(SDL_Rect fmc_dest, const SDL_Rect *source_rect);

#endif
