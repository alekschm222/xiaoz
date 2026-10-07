#ifndef RADIO_STATIONS_H
#define RADIO_STATIONS_H

#include <string.h>

typedef struct {
    const char* name;
    const char* url;
} radio_station_t;

static const radio_station_t radio_stations[] = {
    {"Маруся FM",      "https://marusya.hostingradio.ru/marusya128.mp3"},
    {"Русское Радио",  "https://rusradio.hostingradio.ru/rusradio128.mp3"},
    {"Европа Плюс",    "https://eplayer.hostingradio.ru/europaplus128.mp3"},
    {"Радио Maximum",  "https://maximum.hostingradio.ru/maximum128.mp3"},
    {"Наше Радио",     "https://nashe.hostingradio.ru/nashe128.mp3"},
    {"Радио Шансон",   "https://chanson.hostingradio.ru/chanson128.mp3"},
    {"Радио DFM",      "https://dfm.hostingradio.ru/dfm128.mp3"},
    {"Радио Маяк",     "https://mayakfm.hostingradio.ru/mayak128.mp3"},
    {"Вести FM",       "https://vestifm.hostingradio.ru/vestifm128.mp3"},
    {"Радио ENERGY",   "https://energyfm.hostingradio.ru/energy128.mp3"},
    {"Юмор FM",        "https://humorfm.hostingradio.ru/humorfm128.mp3"},
    {"Авторадио",      "https://avtoradio.hostingradio.ru/avtoradio128.mp3"},
    {"Радио Romantika","https://romantika.hostingradio.ru/romantika128.mp3"},
    {"Радио Jazz",     "https://jazzfm.hostingradio.ru/jazzfm128.mp3"},
    {"Love Radio",     "https://love.hostingradio.ru/love128.mp3"},
    {"Радио 7",        "https://radio7.hostingradio.ru/radio7128.mp3"},
};

static const int radio_stations_count = sizeof(radio_stations) / sizeof(radio_stations[0]);

// Нечёткий поиск станции по имени
// Возвращает индекс станции или -1 если не найдено
static int radio_find_station(const char* query) {
    if (!query) return -1;

    // 1. Точное совпадение (без учёта регистра)
    for (int i = 0; i < radio_stations_count; i++) {
        if (strcasecmp(query, radio_stations[i].name) == 0) {
            return i;
        }
    }

    // 2. Название станции содержит запрос
    for (int i = 0; i < radio_stations_count; i++) {
        if (strcasestr(radio_stations[i].name, query)) {
            return i;
        }
    }

    // 3. Запрос содержит название станции
    for (int i = 0; i < radio_stations_count; i++) {
        if (strcasestr(query, radio_stations[i].name)) {
            return i;
        }
    }

    // 4. Совпадение по первому слову
    char first_word[32] = {0};
    strncpy(first_word, query, sizeof(first_word) - 1);
    char* space = strchr(first_word, ' ');
    if (space) *space = '\0';

    for (int i = 0; i < radio_stations_count; i++) {
        char station_first[32] = {0};
        strncpy(station_first, radio_stations[i].name, sizeof(station_first) - 1);
        char* s = strchr(station_first, ' ');
        if (s) *s = '\0';

        if (strcasecmp(first_word, station_first) == 0) {
            return i;
        }
    }

    return -1;
}

// Получить список всех станций в виде строки
static void radio_get_station_list(char* buf, size_t buf_size) {
    buf[0] = '\0';
    for (int i = 0; i < radio_stations_count; i++) {
        char line[64];
        snprintf(line, sizeof(line), "%d. %s\n", i + 1, radio_stations[i].name);
        strncat(buf, line, buf_size - strlen(buf) - 1);
    }
}

#endif // RADIO_STATIONS_H
