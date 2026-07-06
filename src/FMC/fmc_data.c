#include "fmc_data.h"

#include <stdio.h>

static void set_text(char *dest, int dest_size, const char *src)
{
    if (dest == NULL || dest_size <= 0 || src == NULL)
    {
        return;
    }

    snprintf(dest, (size_t)dest_size, "%s", src);
}

void fmc_data_init(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->current_page = FMC_PAGE_INDEX;

    set_text(data->origin, sizeof(data->origin), "ZBAA");
    set_text(data->destination, sizeof(data->destination), "ZSPD");
    set_text(data->flight_no, sizeof(data->flight_no), "CA001");

    data->route_count = 5;
    set_text(data->route_points[0], sizeof(data->route_points[0]), "ZBAA");
    set_text(data->route_points[1], sizeof(data->route_points[1]), "WPT01");
    set_text(data->route_points[2], sizeof(data->route_points[2]), "WPT02");
    set_text(data->route_points[3], sizeof(data->route_points[3]), "WPT03");
    set_text(data->route_points[4], sizeof(data->route_points[4]), "ZSPD");

    for (int i = data->route_count; i < FMC_MAX_ROUTE_POINTS; ++i)
    {
        data->route_points[i][0] = '\0';
    }

    data->cruise_altitude = 35000;
    data->target_speed = 280;
    data->cost_index = 45.0f;

    set_text(data->departure_runway, sizeof(data->departure_runway), "36R");
    set_text(data->arrival_runway, sizeof(data->arrival_runway), "34L");

    data->scratchpad[0] = '\0';
    data->scratchpad_len = 0;
}

void fmc_data_update_mock(FMC_Data *data, float delta_time)
{
    (void)delta_time;

    if (data == NULL)
    {
        return;
    }
}

void fmc_data_set_page(FMC_Data *data, FMC_Page page)
{
    if (data == NULL)
    {
        return;
    }

    if (page < FMC_PAGE_INDEX || page > FMC_PAGE_LEGS)
    {
        return;
    }

    data->current_page = page;
}

void fmc_data_append_char(FMC_Data *data, char c)
{
    if (data == NULL)
    {
        return;
    }

    if (c < 32 || c > 126)
    {
        return;
    }

    if (data->scratchpad_len >= FMC_TEXT_LEN - 1)
    {
        return;
    }

    data->scratchpad[data->scratchpad_len++] = c;
    data->scratchpad[data->scratchpad_len] = '\0';
}

void fmc_data_backspace(FMC_Data *data)
{
    if (data == NULL || data->scratchpad_len <= 0)
    {
        return;
    }

    data->scratchpad_len--;
    data->scratchpad[data->scratchpad_len] = '\0';
}

void fmc_data_clear_scratchpad(FMC_Data *data)
{
    if (data == NULL)
    {
        return;
    }

    data->scratchpad[0] = '\0';
    data->scratchpad_len = 0;
}
