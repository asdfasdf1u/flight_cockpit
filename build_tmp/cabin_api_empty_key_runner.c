#include <stdio.h>
#include <stdlib.h>

#include "../src/Cabin/cabin_api.h"
#include "../src/Cabin/cabin_data.h"

int main(void)
{
#ifdef _WIN32
    _putenv_s("AMAP_API_KEY", "ENVKEY_SHOULD_NOT_BE_USED");
    _putenv_s("GAODE_API_KEY", "ENVKEY_SHOULD_NOT_BE_USED");
#else
    setenv("AMAP_API_KEY", "ENVKEY_SHOULD_NOT_BE_USED", 1);
    setenv("GAODE_API_KEY", "ENVKEY_SHOULD_NOT_BE_USED", 1);
#endif

    Cabin_Data data;
    cabin_data_init(&data);
    cabin_api_set_key(NULL, 0);
    printf("has_key_after_empty=%d\n", cabin_api_has_key());
    cabin_api_update_weather_for_city(&data, "北京", "110000");
    printf("weather_source=%s weather=%s temp=%.1f\n",
           data.weather_source,
           data.weather,
           data.temperature);

    cabin_api_set_key("PASTEKEY123", 1);
    printf("has_key_after_input=%d\n", cabin_api_has_key());
    return 0;
}
