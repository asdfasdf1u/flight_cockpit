#ifndef CABIN_API_H
#define CABIN_API_H

#include <stddef.h>

#include "cabin_data.h"

void cabin_api_set_key(const char *key, int remember);
int cabin_api_has_key(void);
const char *cabin_api_get_key_source(void);

int cabin_api_update_weather(Cabin_Data *data);
int cabin_api_update_weather_for_city(Cabin_Data *data, const char *city_name, const char *adcode);
int cabin_api_build_weather_url(char *url, size_t url_size, const char *api_key, const char *adcode);
int cabin_api_parse_weather_response(Cabin_Data *data,
                                     const char *response,
                                     const char *requested_city,
                                     const char *requested_adcode);
int cabin_api_prepare_static_map(Cabin_Data *data, char *map_path, int map_path_size);
int cabin_api_parse_reverse_geocode_response(const char *response, Cabin_Place *place);
int cabin_api_reverse_geocode(double latitude,
                              double longitude,
                              Cabin_Place *place);

#endif
