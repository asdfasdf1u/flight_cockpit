#include "cabin_api.h"

#include <ctype.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define cabin_popen _popen
#define cabin_pclose _pclose
#else
#define cabin_popen popen
#define cabin_pclose pclose
#endif

#define CABIN_AMAP_WEATHER_URL "https://restapi.amap.com/v3/weather/weatherInfo"
#define CABIN_AMAP_STATIC_MAP_URL "https://restapi.amap.com/v3/staticmap"
#define CABIN_AMAP_DEFAULT_CITY "110000"
#define CABIN_AMAP_DEFAULT_CITY_NAME "北京"
#define CABIN_AMAP_STATIC_SIZE "1024*576"
#define CABIN_MAP_CACHE_DIR "assets/cache"
#define CABIN_MAP_DEFAULT_CACHE_PATH "assets/cache/cabin_map_beijing_chengdu.png"
#define CABIN_STATIC_MAP_MIN_BYTES 20000
#define CABIN_AMAP_KEY_PLACEHOLDER ""
#define CABIN_HTTP_RESPONSE_MAX 65536
#define CABIN_WEATHER_CACHE_MAX 6
#define CABIN_API_KEY_MAX 256

typedef struct Cabin_Weather_Cache
{
    int valid;
    char city[CABIN_TEXT_LEN];
    char adcode[CABIN_TEXT_LEN];
    char weather[CABIN_TEXT_LEN];
    float temperature;
    float humidity;
    char wind_direction[CABIN_TEXT_LEN];
    char wind_power[CABIN_TEXT_LEN];
    char report_time[CABIN_TEXT_LEN];
} Cabin_Weather_Cache;

static Cabin_Weather_Cache g_weather_cache[CABIN_WEATHER_CACHE_MAX];
static char g_user_api_key[CABIN_API_KEY_MAX] = "";
static int g_user_api_key_remember = 0;

void cabin_api_set_key(const char *key, int remember)
{
    g_user_api_key[0] = '\0';
    g_user_api_key_remember = remember ? 1 : 0;
    memset(g_weather_cache, 0, sizeof(g_weather_cache));

    if (key == NULL || key[0] == '\0')
    {
        return;
    }

    snprintf(g_user_api_key, sizeof(g_user_api_key), "%s", key);
}

int cabin_api_has_key(void)
{
    return g_user_api_key[0] != '\0';
}

const char *cabin_api_get_key_source(void)
{
    if (g_user_api_key[0] != '\0')
    {
        return "dialog input";
    }

    return "dialog empty";
}

static void copy_text(char *dest, size_t dest_size, const char *src)
{
    if (dest == NULL || dest_size == 0)
    {
        return;
    }

    snprintf(dest, dest_size, "%s", src != NULL ? src : "");
}

static const char *get_api_key(const char **source)
{
    if (g_user_api_key[0] != '\0')
    {
        if (source != NULL)
        {
            *source = "dialog input";
        }
        return g_user_api_key;
    }

    if (source != NULL)
    {
        *source = "dialog empty";
    }
    return CABIN_AMAP_KEY_PLACEHOLDER;
}

static int is_placeholder_api_key(const char *key)
{
    if (key == NULL || key[0] == '\0')
    {
        return 1;
    }

    return strcmp(key, "YOUR_AMAP_KEY") == 0 ||
           strcmp(key, "YOUR_AMAP_WEB_SERVICE_KEY") == 0 ||
           strcmp(key, "PLEASE_SET_AMAP_KEY") == 0 ||
           strstr(key, "PLACEHOLDER") != NULL ||
           strstr(key, "TODO") != NULL ||
           strstr(key, "KEY_HERE") != NULL;
}

static int is_safe_api_key(const char *key)
{
    if (is_placeholder_api_key(key))
    {
        return 0;
    }

    for (int i = 0; key[i] != '\0'; ++i)
    {
        const unsigned char ch = (unsigned char)key[i];
        if (!isalnum(ch) && ch != '-' && ch != '_')
        {
            return 0;
        }
    }

    return 1;
}

static void apply_weather_values(
    Cabin_Data *data,
    const char *city,
    const char *adcode,
    const char *weather,
    float temperature,
    float humidity,
    const char *wind_direction,
    const char *wind_power,
    const char *report_time,
    const char *source)
{
    if (data == NULL)
    {
        return;
    }

    copy_text(data->weather_city, sizeof(data->weather_city), city);
    copy_text(data->weather_adcode, sizeof(data->weather_adcode), adcode);
    copy_text(data->weather, sizeof(data->weather), weather);
    data->temperature = temperature;
    data->humidity = humidity;
    copy_text(data->wind_direction, sizeof(data->wind_direction), wind_direction);
    copy_text(data->wind_power, sizeof(data->wind_power), wind_power);
    copy_text(data->weather_report_time, sizeof(data->weather_report_time), report_time);
    copy_text(data->weather_source, sizeof(data->weather_source), source);
}

static Cabin_Weather_Cache *find_weather_cache(const char *adcode)
{
    if (adcode == NULL || adcode[0] == '\0')
    {
        return NULL;
    }

    for (int i = 0; i < CABIN_WEATHER_CACHE_MAX; ++i)
    {
        if (g_weather_cache[i].valid && strcmp(g_weather_cache[i].adcode, adcode) == 0)
        {
            return &g_weather_cache[i];
        }
    }

    return NULL;
}

static Cabin_Weather_Cache *alloc_weather_cache(void)
{
    for (int i = 0; i < CABIN_WEATHER_CACHE_MAX; ++i)
    {
        if (!g_weather_cache[i].valid)
        {
            return &g_weather_cache[i];
        }
    }

    return &g_weather_cache[0];
}

static void apply_cached_weather(Cabin_Data *data, const Cabin_Weather_Cache *cache)
{
    if (data == NULL || cache == NULL || !cache->valid)
    {
        return;
    }

    apply_weather_values(data,
                         cache->city,
                         cache->adcode,
                         cache->weather,
                         cache->temperature,
                         cache->humidity,
                         cache->wind_direction,
                         cache->wind_power,
                         cache->report_time,
                         "API");
    copy_text(data->api_error_message, sizeof(data->api_error_message), "");
    data->api_weather_loaded = 1;
    data->api_weather_failed = 0;
}

static void set_mock_weather_for_city(Cabin_Data *data, const char *city, const char *adcode, const char *message)
{
    const char *safe_city = city != NULL && city[0] != '\0' ? city : "飞行途中";
    const char *safe_adcode = adcode != NULL ? adcode : "";

    if (strcmp(safe_city, "成都") == 0 || strcmp(safe_city, "成都市") == 0 || strcmp(safe_city, "Chengdu") == 0)
    {
        apply_weather_values(data, "成都", safe_adcode, "多云", 24.0f, 62.0f, "东南", "2级", "--", "MOCK");
    }
    else if (strcmp(safe_city, "北京") == 0 || strcmp(safe_city, "北京市") == 0 || strcmp(safe_city, "Beijing") == 0)
    {
        apply_weather_values(data, "北京", safe_adcode, "晴", 18.0f, 57.0f, "西南", "3级", "--", "MOCK");
    }
    else
    {
        apply_weather_values(data, safe_city, safe_adcode, "巡航", -45.0f, 20.0f, "西", "微风", "--", "MOCK");
    }

    data->api_weather_loaded = 0;
    data->api_weather_failed = 1;
    copy_text(data->api_error_message, sizeof(data->api_error_message), message);
}

static void set_map_error(Cabin_Data *data, const char *message)
{
    if (data == NULL)
    {
        return;
    }

    data->api_map_loaded = 0;
    data->api_map_failed = 1;
    copy_text(data->map_source, sizeof(data->map_source), "FALLBACK");
    copy_text(data->api_map_error_message, sizeof(data->api_map_error_message), message);
}

static int http_get_with_curl(const char *url, char *response, size_t response_size)
{
    if (url == NULL || response == NULL || response_size == 0)
    {
        return 0;
    }

    response[0] = '\0';

    char command[1200];
    snprintf(command, sizeof(command), "curl -L -s --connect-timeout 5 --max-time 8 \"%s\"", url);

    FILE *pipe = cabin_popen(command, "r");
    if (pipe == NULL)
    {
        return 0;
    }

    size_t used = 0;
    while (!feof(pipe) && used + 1 < response_size)
    {
        const size_t read_count = fread(response + used, 1, response_size - used - 1, pipe);
        if (read_count == 0)
        {
            break;
        }
        used += read_count;
    }
    response[used] = '\0';

    const int close_code = cabin_pclose(pipe);
    if (close_code != 0)
    {
        printf("Cabin API: curl exited with code=%d.\n", close_code);
    }

    return used > 0 && close_code == 0;
}

static int download_file_with_curl(const char *url, const char *path)
{
    if (url == NULL || path == NULL)
    {
        return 0;
    }

    char command[1400];
    snprintf(command, sizeof(command), "curl -L -s --connect-timeout 5 --max-time 12 -o \"%s\" \"%s\"", path, url);
    return system(command) == 0;
}

static int file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }

    fclose(file);
    return 1;
}

static int directory_exists(const char *path)
{
    struct _stat info;
    return path != NULL && _stat(path, &info) == 0 && (info.st_mode & _S_IFDIR) != 0;
}

static int ensure_cache_directory(void)
{
    if (_mkdir(CABIN_MAP_CACHE_DIR) == 0)
    {
        return 1;
    }

    return directory_exists(CABIN_MAP_CACHE_DIR);
}

static int file_has_png_header(const char *path)
{
    static const unsigned char png_header[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    unsigned char header[8];
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }

    const size_t read_count = fread(header, 1, sizeof(header), file);
    fclose(file);

    return read_count == sizeof(header) && memcmp(header, png_header, sizeof(header)) == 0;
}

static long file_size_bytes(const char *path)
{
    struct _stat info;
    if (path == NULL || _stat(path, &info) != 0)
    {
        return -1;
    }
    return (long)info.st_size;
}

static int static_map_file_usable(const char *path)
{
    const long size = file_size_bytes(path);
    if (!file_has_png_header(path))
    {
        return 0;
    }
    if (size >= 0 && size < CABIN_STATIC_MAP_MIN_BYTES)
    {
        printf("Cabin Map: %s is only %ld bytes; treat as blank/out-of-coverage static map.\n",
               path,
               size);
        return 0;
    }
    return 1;
}

static const char *find_json_string_value(const char *json, const char *key)
{
    if (json == NULL || key == NULL)
    {
        return NULL;
    }

    char pattern[96];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *cursor = strstr(json, pattern);
    if (cursor == NULL)
    {
        return NULL;
    }

    cursor += strlen(pattern);
    cursor = strchr(cursor, ':');
    if (cursor == NULL)
    {
        return NULL;
    }
    ++cursor;

    while (*cursor != '\0' && isspace((unsigned char)*cursor))
    {
        ++cursor;
    }

    if (*cursor != '"')
    {
        return NULL;
    }

    return cursor + 1;
}

static int json_get_string(const char *json, const char *key, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
    {
        return 0;
    }
    out[0] = '\0';

    const char *value = find_json_string_value(json, key);
    if (value == NULL)
    {
        return 0;
    }

    size_t write_index = 0;
    for (const char *cursor = value; *cursor != '\0' && write_index + 1 < out_size; ++cursor)
    {
        if (*cursor == '"' && (cursor == value || *(cursor - 1) != '\\'))
        {
            out[write_index] = '\0';
            return 1;
        }

        if (*cursor == '\\' && *(cursor + 1) != '\0')
        {
            ++cursor;
        }

        out[write_index++] = *cursor;
    }

    out[write_index] = '\0';
    return write_index > 0;
}

static int parse_amap_weather_json(Cabin_Data *data, const char *json, const char *requested_city, const char *requested_adcode)
{
    if (data == NULL || json == NULL || json[0] == '\0')
    {
        return 0;
    }

    char status[16];
    if (!json_get_string(json, "status", status, sizeof(status)) || strcmp(status, "1") != 0)
    {
        return 0;
    }

    char city[CABIN_TEXT_LEN];
    char weather[CABIN_TEXT_LEN];
    char temperature[CABIN_TEXT_LEN];
    char humidity[CABIN_TEXT_LEN];
    char wind_direction[CABIN_TEXT_LEN];
    char wind_power[CABIN_TEXT_LEN];
    char report_time[CABIN_TEXT_LEN];

    if (!json_get_string(json, "city", city, sizeof(city)) ||
        !json_get_string(json, "weather", weather, sizeof(weather)) ||
        !json_get_string(json, "temperature", temperature, sizeof(temperature)) ||
        !json_get_string(json, "humidity", humidity, sizeof(humidity)))
    {
        return 0;
    }

    if (!json_get_string(json, "winddirection", wind_direction, sizeof(wind_direction)))
    {
        copy_text(wind_direction, sizeof(wind_direction), "未知");
    }
    if (!json_get_string(json, "windpower", wind_power, sizeof(wind_power)))
    {
        copy_text(wind_power, sizeof(wind_power), "未知");
    }
    if (!json_get_string(json, "reporttime", report_time, sizeof(report_time)))
    {
        copy_text(report_time, sizeof(report_time), "--");
    }

    const char *display_city = city[0] != '\0' ? city : requested_city;
    const char *display_adcode = requested_adcode != NULL ? requested_adcode : "";
    const float parsed_temperature = (float)atof(temperature);
    const float parsed_humidity = (float)atof(humidity);

    apply_weather_values(data,
                         display_city,
                         display_adcode,
                         weather,
                         parsed_temperature,
                         parsed_humidity,
                         wind_direction,
                         wind_power,
                         report_time,
                         "API");
    copy_text(data->api_error_message, sizeof(data->api_error_message), "");
    data->api_weather_loaded = 1;
    data->api_weather_failed = 0;

    Cabin_Weather_Cache *cache = find_weather_cache(display_adcode);
    if (cache == NULL)
    {
        cache = alloc_weather_cache();
    }
    if (cache != NULL)
    {
        cache->valid = 1;
        copy_text(cache->city, sizeof(cache->city), display_city);
        copy_text(cache->adcode, sizeof(cache->adcode), display_adcode);
        copy_text(cache->weather, sizeof(cache->weather), weather);
        cache->temperature = parsed_temperature;
        cache->humidity = parsed_humidity;
        copy_text(cache->wind_direction, sizeof(cache->wind_direction), wind_direction);
        copy_text(cache->wind_power, sizeof(cache->wind_power), wind_power);
        copy_text(cache->report_time, sizeof(cache->report_time), report_time);
    }

    return 1;
}

static void print_weather_response_error(const char *response)
{
    char status[16];
    char info[96];
    char infocode[32];

    if (response == NULL || response[0] == '\0')
    {
        printf("Cabin API: empty weather response.\n");
        return;
    }

    if (!json_get_string(response, "status", status, sizeof(status)))
    {
        copy_text(status, sizeof(status), "missing");
    }
    if (!json_get_string(response, "info", info, sizeof(info)))
    {
        copy_text(info, sizeof(info), "missing");
    }
    if (!json_get_string(response, "infocode", infocode, sizeof(infocode)))
    {
        copy_text(infocode, sizeof(infocode), "missing");
    }

    printf("Cabin API: Amap weather response status=%s info=%s infocode=%s.\n", status, info, infocode);
}

int cabin_api_update_weather_for_city(Cabin_Data *data, const char *city_name, const char *adcode)
{
    if (data == NULL)
    {
        return 0;
    }

    const char *safe_city = city_name != NULL && city_name[0] != '\0' ? city_name : "飞行途中";
    const char *safe_adcode = adcode != NULL ? adcode : "";

    printf("Cabin Weather: update request for current_city=%s adcode=%s lat=%.6f lon=%.6f.\n",
           safe_city,
           safe_adcode[0] != '\0' ? safe_adcode : "none",
           data->current_lat,
           data->current_lon);

    if (safe_adcode[0] == '\0')
    {
        printf("Cabin Weather: no city adcode for %s, use mock enroute weather and skip API request.\n", safe_city);
        set_mock_weather_for_city(data, safe_city, "", "No city adcode for current flight segment");
        return 0;
    }

    const char *api_key_source = "";
    const char *api_key = get_api_key(&api_key_source);
    if (!is_safe_api_key(api_key))
    {
        printf("Cabin API: no API key entered in dialog, fallback to mock weather.\n");
        printf("Cabin API: current key source=%s, key status=missing or placeholder.\n", api_key_source);
        set_mock_weather_for_city(data, safe_city, safe_adcode, "Amap Web Service API Key missing");
        return 0;
    }

    Cabin_Weather_Cache *cache = find_weather_cache(safe_adcode);
    if (cache != NULL)
    {
        printf("Cabin Weather: using cached weather for city=%s adcode=%s source=API.\n", cache->city, cache->adcode);
        apply_cached_weather(data, cache);
        return 1;
    }

    printf("Cabin API: Amap Web Service API key detected from %s, length=%u.\n",
           api_key_source,
           (unsigned int)strlen(api_key));
    printf("Cabin API: weather request city=%s adcode=%s.\n",
           safe_city,
           safe_adcode);
    printf("Cabin API: sending weather request to Amap.\n");

    char url[1024];
    snprintf(url,
             sizeof(url),
             "%s?key=%s&city=%s&extensions=base&output=JSON",
             CABIN_AMAP_WEATHER_URL,
             api_key,
             safe_adcode);

    char response[CABIN_HTTP_RESPONSE_MAX];
    if (!http_get_with_curl(url, response, sizeof(response)))
    {
        printf("Cabin API: HTTP request failed or curl is unavailable, fallback to mock weather.\n");
        set_mock_weather_for_city(data, safe_city, safe_adcode, "Amap weather HTTP request failed");
        return 0;
    }

    printf("Cabin API: HTTP request succeeded, response length=%u bytes.\n", (unsigned int)strlen(response));

    if (!parse_amap_weather_json(data, response, safe_city, safe_adcode))
    {
        printf("Cabin API: JSON parse failed or API returned non-success status, fallback to mock weather.\n");
        print_weather_response_error(response);
        set_mock_weather_for_city(data, safe_city, safe_adcode, "Amap weather JSON parse failed");
        return 0;
    }

    printf("Cabin API: JSON parsed: city=%s weather=%s temperature=%.1f humidity=%.1f wind=%s/%s report=%s.\n",
           data->weather_city,
           data->weather,
           data->temperature,
           data->humidity,
           data->wind_direction,
           data->wind_power,
           data->weather_report_time);

    return 1;
}

int cabin_api_update_weather(Cabin_Data *data)
{
    return cabin_api_update_weather_for_city(data, CABIN_AMAP_DEFAULT_CITY_NAME, CABIN_AMAP_DEFAULT_CITY);
}

int cabin_api_prepare_static_map(Cabin_Data *data, char *map_path, int map_path_size)
{
    const char *cache_path = NULL;
    int zoom = 0;
    char location[64];
    char zoom_text[16];

    if (data == NULL)
    {
        return 0;
    }

    if (map_path != NULL && map_path_size > 0)
    {
        map_path[0] = '\0';
    }

    if (data->map_cache_path[0] == '\0')
    {
        copy_text(data->map_cache_path, sizeof(data->map_cache_path), CABIN_MAP_DEFAULT_CACHE_PATH);
    }
    cache_path = data->map_cache_path;
    zoom = data->map_zoom > 0 ? data->map_zoom : 5;
    snprintf(location, sizeof(location), "%.6f,%.6f", data->map_center_lon, data->map_center_lat);
    snprintf(zoom_text, sizeof(zoom_text), "%d", zoom);

    if (file_exists(cache_path) && static_map_file_usable(cache_path))
    {
        printf("Cabin Map: route cache found, using %s.\n", cache_path);
        copy_text(data->map_source, sizeof(data->map_source), "CACHE");
        data->api_map_loaded = 1;
        data->api_map_failed = 0;
        copy_text(data->api_map_error_message, sizeof(data->api_map_error_message), "");
        if (map_path != NULL && map_path_size > 0)
        {
            copy_text(map_path, (size_t)map_path_size, cache_path);
        }
        return 1;
    }

    if (file_exists(cache_path))
    {
        printf("Cabin Map: cache exists but is not a usable static map, will try downloading again.\n");
        remove(cache_path);
    }
    else
    {
        printf("Cabin Map: no route cache found at %s.\n", cache_path);
    }

    const char *api_key_source = "";
    const char *api_key = get_api_key(&api_key_source);
    if (!is_safe_api_key(api_key))
    {
        printf("Cabin Map: no valid Amap Web Service API key found, skip static map request and fallback to local map.\n");
        printf("Cabin Map: current key source=%s, key status=missing or placeholder.\n", api_key_source);
        set_map_error(data, "未配置高德 API Key");
        return 0;
    }

    printf("Cabin Map: Amap Web Service API key detected from %s, length=%u.\n",
           api_key_source,
           (unsigned int)strlen(api_key));

    if (!ensure_cache_directory())
    {
        printf("Cabin Map: failed to create cache directory %s, fallback to local map.\n", CABIN_MAP_CACHE_DIR);
        set_map_error(data, "无法创建地图缓存目录");
        return 0;
    }

    char url[1400];
    snprintf(url,
             sizeof(url),
             "%s?key=%s&location=%s&zoom=%s&size=%s&scale=2",
             CABIN_AMAP_STATIC_MAP_URL,
             api_key,
             location,
             zoom_text,
             CABIN_AMAP_STATIC_SIZE);

    printf("Cabin Map: requesting Amap static map center=%s zoom=%s size=%s cache=%s.\n",
           location,
           zoom_text,
           CABIN_AMAP_STATIC_SIZE,
           cache_path);

    if (!download_file_with_curl(url, cache_path))
    {
        printf("Cabin Map: static map request failed or curl is unavailable, fallback to local map.\n");
        set_map_error(data, "静态地图下载失败");
        return 0;
    }

    if (!static_map_file_usable(cache_path))
    {
        printf("Cabin Map: downloaded file is not a usable route map, fallback to local/drawn map.\n");
        remove(cache_path);
        set_map_error(data, "静态地图返回内容不可用");
        return 0;
    }

    printf("Cabin Map: static map saved to %s.\n", cache_path);
    copy_text(data->map_source, sizeof(data->map_source), "API");
    data->api_map_loaded = 1;
    data->api_map_failed = 0;
    copy_text(data->api_map_error_message, sizeof(data->api_map_error_message), "");
    if (map_path != NULL && map_path_size > 0)
    {
        copy_text(map_path, (size_t)map_path_size, cache_path);
    }

    return 1;
}
