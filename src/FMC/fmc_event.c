#include "fmc_event.h"

#include "fmc_display.h"

static int hit_test_button_index(SDL_Renderer *renderer, int x, int y)
{
    int base_x = 0;
    int base_y = 0;
    if (!fmc_display_window_to_base(renderer, x, y, &base_x, &base_y))
    {
        return -1;
    }

    const int count = fmc_key_button_count();
    for (int i = 0; i < count; ++i)
    {
        const FMC_Button *button = fmc_key_button_at(i);
        if (fmc_key_button_contains_base_point(button, base_x, base_y))
        {
            return i;
        }
    }

    return -1;
}

static int hit_test_button_index_base(int base_x, int base_y)
{
    const int count = fmc_key_button_count();
    for (int i = 0; i < count; ++i)
    {
        const FMC_Button *button = fmc_key_button_at(i);
        if (fmc_key_button_contains_base_point(button, base_x, base_y))
        {
            return i;
        }
    }

    return -1;
}

void fmc_event_state_init(FMC_Event_State *state)
{
    if (state == NULL)
    {
        return;
    }

    state->hovered_button = FMC_BUTTON_NONE;
    state->hovered_button_index = -1;
}

void fmc_event_update_hover(SDL_Renderer *renderer, FMC_Event_State *state, int x, int y)
{
    if (state == NULL)
    {
        return;
    }

    const int index = hit_test_button_index(renderer, x, y);
    state->hovered_button_index = index;
    state->hovered_button = index >= 0 ? fmc_key_button_at(index)->id : FMC_BUTTON_NONE;
}

void fmc_event_update_hover_base(FMC_Event_State *state, int base_x, int base_y)
{
    if (state == NULL)
    {
        return;
    }

    const int index = hit_test_button_index_base(base_x, base_y);
    state->hovered_button_index = index;
    state->hovered_button = index >= 0 ? fmc_key_button_at(index)->id : FMC_BUTTON_NONE;
}

int fmc_event_handle_mouse_button(SDL_Renderer *renderer, FMC_Event_State *state, FMC_Data *data, int x, int y)
{
    const int index = hit_test_button_index(renderer, x, y);
    if (state != NULL)
    {
        state->hovered_button_index = index;
        state->hovered_button = index >= 0 ? fmc_key_button_at(index)->id : FMC_BUTTON_NONE;
    }

    if (index < 0)
    {
        return 0;
    }

    const FMC_Button *button = fmc_key_button_at(index);
    if (button == NULL || button->action == NULL)
    {
        return 0;
    }

    fmc_key_activate_button(data, button);
    return 1;
}

int fmc_event_handle_mouse_button_base(FMC_Event_State *state, FMC_Data *data, int base_x, int base_y)
{
    const int index = hit_test_button_index_base(base_x, base_y);
    if (state != NULL)
    {
        state->hovered_button_index = index;
        state->hovered_button = index >= 0 ? fmc_key_button_at(index)->id : FMC_BUTTON_NONE;
    }

    if (index < 0)
    {
        return 0;
    }

    const FMC_Button *button = fmc_key_button_at(index);
    if (button == NULL || button->action == NULL)
    {
        return 0;
    }

    fmc_key_activate_button(data, button);
    return 1;
}

const FMC_Button *fmc_event_hit_test_button(SDL_Renderer *renderer, int x, int y)
{
    const int index = hit_test_button_index(renderer, x, y);
    return index >= 0 ? fmc_key_button_at(index) : NULL;
}
