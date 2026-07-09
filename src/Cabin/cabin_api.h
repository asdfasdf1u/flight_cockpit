#ifndef CABIN_API_H
#define CABIN_API_H

#include "cabin_data.h"

int cabin_api_update_weather(Cabin_Data *data);
int cabin_api_update_weather_for_city(Cabin_Data *data, const char *city_name, const char *adcode);
int cabin_api_prepare_static_map(Cabin_Data *data, char *map_path, int map_path_size);

#endif
