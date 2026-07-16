#include "cabin_api.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_weather_url_uses_adcode(void)
{
    char url[512];
    assert(cabin_api_build_weather_url(url,
                                       sizeof(url),
                                       "12345678901234567890123456789012",
                                       "370202"));
    assert(strstr(url, "city=370202") != NULL);
    assert(strstr(url, "青岛") == NULL);
    assert(!cabin_api_build_weather_url(url, sizeof(url), "key", ""));
}

static void test_city_weather(const char *response,
                              const char *requested_city,
                              const char *adcode,
                              const char *expected_city,
                              const char *expected_wind_power)
{
    Cabin_Data data;
    cabin_data_init(&data);
    assert(cabin_api_parse_weather_response(&data, response, requested_city, adcode));
    assert(strcmp(data.weather_city, expected_city) == 0);
    assert(strcmp(data.weather_adcode, adcode) == 0);
    assert(strcmp(data.weather_source, "API") == 0);
    assert(data.api_weather_loaded);
    assert(!data.api_weather_failed);
    assert(strcmp(data.wind_power, expected_wind_power) == 0);
    assert(isfinite(data.temperature));
    assert(isfinite(data.humidity));
}

int main(void)
{
    const char *qingdao =
        "{\"status\":\"1\",\"lives\":[{\"province\":\"山东\",\"city\":\"青岛市\",\"adcode\":\"370202\"," 
        "\"weather\":\"晴\",\"temperature\":\"26\",\"winddirection\":\"南\",\"windpower\":\"≤3\"," 
        "\"humidity\":\"61\",\"reporttime\":\"2026-07-16 12:00:00\"}]}";
    const char *xian =
        "{\"status\":\"1\",\"lives\":[{\"province\":\"陕西\",\"city\":\"西安市\",\"adcode\":\"610113\"," 
        "\"weather\":\"多云\",\"temperature\":\"31\",\"winddirection\":\"东北\",\"windpower\":\"4\"," 
        "\"humidity\":\"45\",\"reporttime\":\"2026-07-16 12:00:00\"}]}";

    test_weather_url_uses_adcode();
    test_city_weather(qingdao, "青岛市", "370202", "青岛市", "≤3");
    test_city_weather(xian, "西安市", "610113", "西安市", "4");
    printf("Cabin weather adcode tests passed.\n");
    return 0;
}
