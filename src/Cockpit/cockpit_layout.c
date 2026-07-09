#include "cockpit_layout.h"

const SDL_Rect COCKPIT_FMC_SCREEN_RECT = {104, 74, 435, 345};
const SDL_Rect COCKPIT_FMC_BUTTON_INIT_REF = {69, 477, 72, 51};
const SDL_Rect COCKPIT_FMC_BUTTON_RTE = {153, 477, 72, 51};
const SDL_Rect COCKPIT_FMC_BUTTON_DEP_ARR = {236, 536, 72, 51};
const SDL_Rect COCKPIT_FMC_BUTTON_LEGS = {153, 536, 72, 51};
const SDL_Rect COCKPIT_FMC_BUTTON_EXEC = {500, 536, 72, 51};
const SDL_Rect COCKPIT_FMC_BUTTON_DEL = {401, 923, 49, 49};
const SDL_Rect COCKPIT_FMC_BUTTON_CLR = {535, 923, 49, 49};

static SDL_Rect scale_rect(int x, int y, int w, int h, float sx, float sy)
{
    SDL_Rect rect;
    rect.x = (int)((float)x * sx);
    rect.y = (int)((float)y * sy);
    rect.w = (int)((float)w * sx);
    rect.h = (int)((float)h * sy);
    return rect;
}

const char *cockpit_layout_background_path(void)
{
    return "assets/main.png";
}

const char *cockpit_layout_fmc_background_path(void)
{
    return "assets/fmc.png";
}

Cockpit_Layout cockpit_layout_default(int world_width, int world_height)
{
    if (world_width <= 0)
    {
        world_width = 8026;
    }
    if (world_height <= 0)
    {
        world_height = 3136;
    }

    const float sx = (float)world_width / 8026.0f;
    const float sy = (float)world_height / 3136.0f;

    Cockpit_Layout layout;
    layout.world_width = world_width;
    layout.world_height = world_height;

    layout.capt_pfd_rect = scale_rect(1320, 960, 600, 700, sx, sy);
    layout.capt_nd_rect = scale_rect(2210, 955, 760, 700, sx, sy);
    layout.eicas1_rect = scale_rect(3705, 955, 680, 700, sx, sy);
    layout.fo_nd_rect = scale_rect(4865, 955, 760, 700, sx, sy);
    layout.fo_pfd_rect = scale_rect(5790, 960, 600, 700, sx, sy);

    layout.left_fmc_rect = scale_rect(2850, 1930, 510, 820, sx, sy);
    layout.eicas2_rect = scale_rect(3570, 1965, 740, 760, sx, sy);
    layout.right_fmc_rect = scale_rect(4535, 1930, 510, 820, sx, sy);

    return layout;
}

Cockpit_FmcSide cockpit_layout_hit_test_fmc(const Cockpit_Layout *layout, float world_x, float world_y)
{
    if (layout == 0)
    {
        return COCKPIT_FMC_NONE;
    }

    if (world_x >= (float)layout->left_fmc_rect.x &&
        world_x < (float)(layout->left_fmc_rect.x + layout->left_fmc_rect.w) &&
        world_y >= (float)layout->left_fmc_rect.y &&
        world_y < (float)(layout->left_fmc_rect.y + layout->left_fmc_rect.h))
    {
        return COCKPIT_FMC_LEFT;
    }

    if (world_x >= (float)layout->right_fmc_rect.x &&
        world_x < (float)(layout->right_fmc_rect.x + layout->right_fmc_rect.w) &&
        world_y >= (float)layout->right_fmc_rect.y &&
        world_y < (float)(layout->right_fmc_rect.y + layout->right_fmc_rect.h))
    {
        return COCKPIT_FMC_RIGHT;
    }

    return COCKPIT_FMC_NONE;
}

SDL_Rect cockpit_layout_fmc_source_to_dest_rect(SDL_Rect fmc_dest, const SDL_Rect *source_rect)
{
    SDL_Rect dest = {0, 0, 0, 0};
    if (source_rect == 0 || fmc_dest.w <= 0 || fmc_dest.h <= 0)
    {
        return dest;
    }

    dest.x = fmc_dest.x + source_rect->x * fmc_dest.w / COCKPIT_FMC_IMAGE_WIDTH;
    dest.y = fmc_dest.y + source_rect->y * fmc_dest.h / COCKPIT_FMC_IMAGE_HEIGHT;
    dest.w = source_rect->w * fmc_dest.w / COCKPIT_FMC_IMAGE_WIDTH;
    dest.h = source_rect->h * fmc_dest.h / COCKPIT_FMC_IMAGE_HEIGHT;
    if (dest.w < 1)
    {
        dest.w = 1;
    }
    if (dest.h < 1)
    {
        dest.h = 1;
    }
    return dest;
}
