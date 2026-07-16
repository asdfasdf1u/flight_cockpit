#include "cabin_api.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_complete_address(void)
{
    const char *json =
        "{\"status\":\"1\",\"regeocode\":{\"formatted_address\":\"四川省成都市武侯区桂溪街道天府大道\","
        "\"addressComponent\":{\"province\":\"四川省\",\"city\":\"成都市\",\"district\":\"武侯区\",\"adcode\":\"510107\"," 
        "\"township\":\"桂溪街道\",\"streetNumber\":{\"street\":\"天府大道\"}}}}";
    Cabin_Place place = {0};

    assert(cabin_api_parse_reverse_geocode_response(json, &place));
    place.status = CABIN_PLACE_VALID;
    assert(strcmp(place.province, "四川省") == 0);
    assert(strcmp(place.city, "成都市") == 0);
    assert(strcmp(place.district, "武侯区") == 0);
    assert(strcmp(place.adcode, "510107") == 0);
    assert(strcmp(place.township, "桂溪街道") == 0);
    assert(strcmp(place.street, "天府大道") == 0);
}

static void test_street_and_municipality_fallback(void)
{
    const char *json =
        "{\"status\":\"1\",\"regeocode\":{\"formatted_address\":\"北京市朝阳区建国路\","
        "\"addressComponent\":{\"province\":\"北京市\",\"city\":[],\"district\":\"朝阳区\",\"adcode\":\"110105\"," 
        "\"township\":\"\",\"streetNumber\":{\"street\":\"建国路\"}}}}";
    Cabin_Place place = {0};

    assert(cabin_api_parse_reverse_geocode_response(json, &place));
    place.status = CABIN_PLACE_VALID;
    assert(strcmp(place.city, "北京市") == 0);
    assert(strcmp(place.adcode, "110105") == 0);
    assert(place.township[0] == '\0');
    assert(strcmp(place.street, "建国路") == 0);
}

static void test_formatted_address_fallback(void)
{
    const char *json =
        "{\"status\":\"1\",\"regeocode\":{\"formatted_address\":\"青海省海西蒙古族藏族自治州格尔木市测试地点\","
        "\"addressComponent\":{\"province\":\"青海省\",\"city\":\"海西蒙古族藏族自治州\",\"district\":\"格尔木市\",\"adcode\":\"632801\"," 
        "\"township\":[],\"streetNumber\":[]}}}";
    Cabin_Place place = {0};

    assert(cabin_api_parse_reverse_geocode_response(json, &place));
    place.status = CABIN_PLACE_VALID;
    assert(place.township[0] == '\0');
    assert(place.street[0] == '\0');
    assert(strcmp(place.adcode, "632801") == 0);
    assert(strcmp(place.formatted_address, "青海省海西蒙古族藏族自治州格尔木市测试地点") == 0);
}

static void test_supported_city_adcodes(void)
{
    const char *json_cases[] = {
        "{\"status\":\"1\",\"regeocode\":{\"formatted_address\":\"陕西省西安市雁塔区\",\"addressComponent\":{\"province\":\"陕西省\",\"city\":\"西安市\",\"district\":\"雁塔区\",\"adcode\":\"610113\"}}}",
        "{\"status\":\"1\",\"regeocode\":{\"formatted_address\":\"山东省青岛市市南区\",\"addressComponent\":{\"province\":\"山东省\",\"city\":\"青岛市\",\"district\":\"市南区\",\"adcode\":\"370202\"}}}",
        "{\"status\":\"1\",\"regeocode\":{\"formatted_address\":\"四川省成都市武侯区\",\"addressComponent\":{\"province\":\"四川省\",\"city\":\"成都市\",\"district\":\"武侯区\",\"adcode\":\"510107\"}}}"};
    const char *expected_adcodes[] = {"610113", "370202", "510107"};

    for (int i = 0; i < 3; ++i)
    {
        Cabin_Place place = {0};
        assert(cabin_api_parse_reverse_geocode_response(json_cases[i], &place));
        assert(strcmp(place.adcode, expected_adcodes[i]) == 0);
    }
}

static void test_failed_status_clears_fields(void)
{
    const char *json = "{\"status\":\"0\",\"info\":\"INVALID_USER_KEY\"}";
    Cabin_Place place = {0};
    snprintf(place.township, sizeof(place.township), "%s", "旧值");
    snprintf(place.adcode, sizeof(place.adcode), "%s", "110105");

    assert(!cabin_api_parse_reverse_geocode_response(json, &place));
    assert(place.province[0] == '\0');
    assert(place.city[0] == '\0');
    assert(place.district[0] == '\0');
    assert(place.adcode[0] == '\0');
    assert(place.township[0] == '\0');
    assert(place.street[0] == '\0');
    assert(place.formatted_address[0] == '\0');

    cabin_api_set_key(NULL, 0);
    assert(!cabin_api_reverse_geocode(39.9042, 116.4074, &place));
}

static void test_address_lengths_are_bounded(void)
{
    const char *json =
        "{\"status\":\"1\",\"regeocode\":{\"formatted_address\":\""
        "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ\","
        "\"addressComponent\":{\"province\":\"测试省\",\"city\":\"测试市\",\"district\":\"测试区\"," 
        "\"adcode\":\"ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ\"," 
        "\"township\":\"ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ\","
        "\"streetNumber\":{\"street\":\"测试路\"}}}}";
    Cabin_Place place = {0};

    assert(cabin_api_parse_reverse_geocode_response(json, &place));
    assert(strlen(place.township) == sizeof(place.township) - 1);
    assert(strlen(place.adcode) == sizeof(place.adcode) - 1);
    assert(place.township[sizeof(place.township) - 1] == '\0');
    assert(strlen(place.formatted_address) == sizeof(place.formatted_address) - 1);
    assert(place.formatted_address[sizeof(place.formatted_address) - 1] == '\0');
}

static void test_static_map_cache_validation(void)
{
    static const unsigned char png_header[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    const char *invalid_path = "build_tmp/cabin_invalid_cache.png";
    FILE *file = fopen(invalid_path, "wb");
    assert(file != NULL);
    assert(fwrite(png_header, 1, sizeof(png_header), file) == sizeof(png_header));
    fclose(file);

    Cabin_Data data = {0};
    data.map_min_zoom = CABIN_MAP_MIN_ZOOM;
    data.map_max_zoom = CABIN_MAP_MAX_ZOOM;
    data.map_zoom = 5;
    data.map_center_lat = 35.3;
    data.map_center_lon = 110.4;
    snprintf(data.map_cache_path, sizeof(data.map_cache_path), "%s", invalid_path);
    cabin_api_set_key(NULL, 0);
    assert(!cabin_api_prepare_static_map(&data, NULL, 0));
    file = fopen(invalid_path, "rb");
    assert(file == NULL);

    snprintf(data.map_cache_path,
             sizeof(data.map_cache_path),
             "%s",
             "assets/cache/cabin_map_beijing_chengdu_z5_16x9.png");
    char loaded_path[256];
    assert(cabin_api_prepare_static_map(&data, loaded_path, sizeof(loaded_path)));
    assert(strcmp(loaded_path, data.map_cache_path) == 0);
    assert(strcmp(data.map_source, "CACHE") == 0);
}

int main(void)
{
    test_complete_address();
    test_street_and_municipality_fallback();
    test_formatted_address_fallback();
    test_supported_city_adcodes();
    test_failed_status_clears_fields();
    test_address_lengths_are_bounded();
    test_static_map_cache_validation();
    printf("Cabin reverse geocode tests passed.\n");
    return 0;
}
