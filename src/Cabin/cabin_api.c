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
#define CABIN_AMAP_DEFAULT_CITY_NAME "Beijing"
#define CABIN_AMAP_STATIC_CENTER "116.407400,39.904200"
#define CABIN_AMAP_STATIC_ZOOM "8"
#define CABIN_AMAP_STATIC_SIZE "1024*768"
#define CABIN_MAP_CACHE_DIR "assets/cache"
#define CABIN_MAP_CACHE_PATH "assets/cache/cabin_map.png"
#define CABIN_AMAP_KEY_PLACEHOLDER ""
#define CABIN_HTTP_RESPONSE_MAX 65536

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
    const char *key = getenv("AMAP_API_KEY");
    if (key != NULL && key[0] != '\0')
    {
        if (source != NULL)
        {
            *source = "AMAP_API_KEY";
        }
        return key;
    }

    key = getenv("GAODE_API_KEY");
    if (key != NULL && key[0] != '\0')
    {
        if (source != NULL)
        {
            *source = "GAODE_API_KEY";
        }
        return key;
    }

    if (source != NULL)
    {
        *source = "built-in placeholder";
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

static void set_weather_error(Cabin_Data *data, const char *message)
{
    if (data == NULL)
    {
        return;
    }

    data->api_weather_loaded = 0;
    data->api_weather_failed = 1;
    copy_text(data->weather_source, sizeof(data->weather_source), "MOCK");
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
    copy_text(data->map_source, sizeof(data->map_source), "LOCAL");
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

static int parse_amap_weather_json(Cabin_Data *data, const char *json)
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

    copy_text(data->current_city, sizeof(data->current_city), city);
    copy_text(data->weather, sizeof(data->weather), weather);
    data->temperature = (float)atof(temperature);
    data->humidity = (float)atof(humidity);
    copy_text(data->wind_direction, sizeof(data->wind_direction), wind_direction);
    copy_text(data->wind_power, sizeof(data->wind_power), wind_power);
    copy_text(data->weather_report_time, sizeof(data->weather_report_time), report_time);
    copy_text(data->weather_source, sizeof(data->weather_source), "API");
    copy_text(data->api_error_message, sizeof(data->api_error_message), "");
    data->api_weather_loaded = 1;
    data->api_weather_failed = 0;

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

int cabin_api_update_weather(Cabin_Data *data)
{
    if (data == NULL)
    {
        return 0;
    }

    const char *api_key_source = "";
    const char *api_key = get_api_key(&api_key_source);
    if (!is_safe_api_key(api_key))
    {
        printf("Cabin API: need Amap Web Service API Key. Set environment variable AMAP_API_KEY or GAODE_API_KEY.\n");
        printf("Cabin API: current key source=%s, key status=missing or placeholder, fallback to mock weather.\n", api_key_source);
        set_weather_error(data, "Amap Web Service API Key missing");
        return 0;
    }

    printf("Cabin API: Amap Web Service API key detected from %s, length=%u.\n",
           api_key_source,
           (unsigned int)strlen(api_key));
    printf("Cabin API: weather request city=%s adcode=%s.\n",
           CABIN_AMAP_DEFAULT_CITY_NAME,
           CABIN_AMAP_DEFAULT_CITY);
    printf("Cabin API: sending weather request to Amap.\n");

    char url[1024];
    snprintf(url,
             sizeof(url),
             "%s?key=%s&city=%s&extensions=base&output=JSON",
             CABIN_AMAP_WEATHER_URL,
             api_key,
             CABIN_AMAP_DEFAULT_CITY);

    char response[CABIN_HTTP_RESPONSE_MAX];
    if (!http_get_with_curl(url, response, sizeof(response)))
    {
        printf("Cabin API: HTTP request failed or curl is unavailable, fallback to mock weather.\n");
        set_weather_error(data, "Amap weather HTTP request failed");
        return 0;
    }

    printf("Cabin API: HTTP request succeeded, response length=%u bytes.\n", (unsigned int)strlen(response));

    if (!parse_amap_weather_json(data, response))
    {
        printf("Cabin API: JSON parse failed or API returned non-success status, fallback to mock weather.\n");
        print_weather_response_error(response);
        set_weather_error(data, "Amap weather JSON parse failed");
        return 0;
    }

    printf("Cabin API: JSON parsed: city=%s weather=%s temperature=%.1f humidity=%.1f wind=%s/%s report=%s.\n",
           data->current_city,
           data->weather,
           data->temperature,
           data->humidity,
           data->wind_direction,
           data->wind_power,
           data->weather_report_time);

    return 1;
}

int cabin_api_prepare_static_map(Cabin_Data *data, char *map_path, int map_path_size)
{
    if (data == NULL)
    {
        return 0;
    }

    if (map_path != NULL && map_path_size > 0)
    {
        map_path[0] = '\0';
    }

    if (file_exists(CABIN_MAP_CACHE_PATH) && file_has_png_header(CABIN_MAP_CACHE_PATH))
    {
        printf("Cabin Map: cache found, using %s.\n", CABIN_MAP_CACHE_PATH);
        copy_text(data->map_source, sizeof(data->map_source), "CACHE");
        data->api_map_loaded = 1;
        data->api_map_failed = 0;
        copy_text(data->api_map_error_message, sizeof(data->api_map_error_message), "");
        if (map_path != NULL && map_path_size > 0)
        {
            copy_text(map_path, (size_t)map_path_size, CABIN_MAP_CACHE_PATH);
        }
        return 1;
    }

    if (file_exists(CABIN_MAP_CACHE_PATH))
    {
        printf("Cabin Map: cache exists but is not a valid PNG, will try downloading again.\n");
        remove(CABIN_MAP_CACHE_PATH);
    }
    else
    {
        printf("Cabin Map: no cache found at %s.\n", CABIN_MAP_CACHE_PATH);
    }

    const char *api_key = get_api_key(NULL);
    if (!is_safe_api_key(api_key))
    {
        printf("Cabin Map: no valid API key found, skip Amap static map request and fallback to local map.\n");
        set_map_error(data, "未配置高德 API Key");
        return 0;
    }

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
             CABIN_AMAP_STATIC_CENTER,
             CABIN_AMAP_STATIC_ZOOM,
             CABIN_AMAP_STATIC_SIZE);

    printf("Cabin Map: requesting Amap static map center=%s zoom=%s size=%s.\n",
           CABIN_AMAP_STATIC_CENTER,
           CABIN_AMAP_STATIC_ZOOM,
           CABIN_AMAP_STATIC_SIZE);

    if (!download_file_with_curl(url, CABIN_MAP_CACHE_PATH))
    {
        printf("Cabin Map: static map request failed or curl is unavailable, fallback to local map.\n");
        set_map_error(data, "静态地图下载失败");
        return 0;
    }

    if (!file_has_png_header(CABIN_MAP_CACHE_PATH))
    {
        printf("Cabin Map: downloaded file is not a valid PNG, fallback to local map.\n");
        remove(CABIN_MAP_CACHE_PATH);
        set_map_error(data, "静态地图返回内容不是 PNG");
        return 0;
    }

    printf("Cabin Map: static map saved to %s.\n", CABIN_MAP_CACHE_PATH);
    copy_text(data->map_source, sizeof(data->map_source), "API");
    data->api_map_loaded = 1;
    data->api_map_failed = 0;
    copy_text(data->api_map_error_message, sizeof(data->api_map_error_message), "");
    if (map_path != NULL && map_path_size > 0)
    {
        copy_text(map_path, (size_t)map_path_size, CABIN_MAP_CACHE_PATH);
    }

    return 1;
}
