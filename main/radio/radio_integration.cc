#include "radio_integration.h"
#include "radio_player.h"
#include "radio_stations.h"
#include "spectrum_analyzer.h"
#include "lcd_ili9341.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "RADIO_INT";

// Состояние
static bool s_initialized = false;
static bool s_radio_active = false;
static char s_station_list_buf[512] = {0};

// LCD размеры (из lcd_ili9341)
extern int lcd_get_width(void);
extern int lcd_get_height(void);

void RadioInit(void) {
    ESP_LOGI(TAG, "Initializing radio integration");

    radio_player_init();
    spectrum_analyzer_init();
    lcd_ili9341_init();

    radio_get_station_list(s_station_list_buf, sizeof(s_station_list_buf));
    s_initialized = true;

    ESP_LOGI(TAG, "Radio integration ready, %d stations loaded", radio_stations_count);
}

bool RadioPlayByName(const char* name) {
    if (!s_initialized) return false;

    int idx = radio_find_station(name);
    if (idx < 0) {
        ESP_LOGW(TAG, "Station not found: %s", name);
        return false;
    }

    ESP_LOGI(TAG, "Playing station %d: %s (%s)", idx, radio_stations[idx].name, radio_stations[idx].url);
    radio_player_play(radio_stations[idx].url);
    s_radio_active = true;
    return true;
}

void RadioPlayByIndex(int index) {
    if (!s_initialized || index < 0 || index >= radio_stations_count) return;
    radio_player_play(radio_stations[index].url);
    s_radio_active = true;
}

void RadioStop(void) {
    if (!s_initialized) return;
    radio_player_stop();
    s_radio_active = false;
    spectrum_analyzer_reset();
    ESP_LOGI(TAG, "Radio stopped");
}

void RadioPause(void) {
    if (!s_initialized) return;
    radio_player_pause();
    ESP_LOGI(TAG, "Radio paused");
}

void RadioResume(void) {
    if (!s_initialized) return;
    if (radio_player_was_playing()) {
        radio_player_resume();
        ESP_LOGI(TAG, "Radio resumed");
    }
}

void RadioOnStateChange(int state) {
    if (!s_initialized) return;

    // 0=Idle, 2=Listening, 3=Speaking
    switch (state) {
        case 0: // Idle
            // Возобновляем радио если было на паузе
            RadioResume();
            break;
        case 2: // Listening
        case 3: // Speaking
            // Пауза радио — не мешаем разговору
            RadioPause();
            break;
        default:
            break;
    }
}

bool RadioOnTouch(void) {
    if (!s_initialized || !RadioIsPlaying()) return false;

    ESP_LOGI(TAG, "Touch detected — stopping radio");
    RadioStop();
    return true;  // Сигнал: радио было активно, ассистент должен слушать
}

bool RadioIsPlaying(void) {
    return s_initialized && s_radio_active && radio_player_is_playing();
}

bool RadioPreventSleep(void) {
    return RadioIsPlaying();
}

void RadioUpdateDisplay(void) {
    if (!s_initialized || !s_radio_active) return;

    // Получаем PCM из ring buffer
    static int16_t pcm_buf[512];
    int samples = radio_player_get_pcm(pcm_buf, 512);
    if (samples > 0) {
        spectrum_analyzer_feed(pcm_buf, samples);
    }

    // Получаем значения полос
    float bars[SPECTRUM_NUM_BANDS];
    float peaks[SPECTRUM_NUM_BANDS];
    spectrum_analyzer_get(bars, peaks);

    // Рисуем спектр на LCD
    int width = lcd_get_width();
    int height = lcd_get_height();

    // Спектр в нижней 2/3 экрана
    int spectrum_y = height / 3;
    int spectrum_h = height - spectrum_y;

    lcd_draw_spectrum(0, spectrum_y, width, spectrum_h, bars, SPECTRUM_NUM_BANDS, peaks);
}

const char* RadioGetStationList(void) {
    return s_station_list_buf;
}

bool RadioHandleCommand(const char* command) {
    if (!s_initialized || !command) return false;

    // Проверяем команды остановки
    if (strstr(command, "стоп радио") || strstr(command, "выключи радио") ||
        strstr(command, "останови радио") || strstr(command, "stop radio")) {
        RadioStop();
        return true;
    }

    // Запрос списка станций
    if (strstr(command, "какие есть ра") || strstr(command, "список ра") ||
        strstr(command, "какие радио") || strstr(command, "какие станц")) {
        // Здесь нужно отправить список через TTS
        // Это обрабатывается на стороне сервера через MCP
        return true;
    }

    // Переключение
    if (strstr(command, "следующая") || strstr(command, "переключи")) {
        // Логика переключения на следующую станцию
        return true;
    }

    // Включение радио
    if (strstr(command, "включи радио") || strstr(command, "play radio")) {
        // Извлекаем название станции
        const char* station_name = NULL;
        if (strstr(command, "включи радио ")) {
            station_name = strstr(command, "включи радио ") + strlen("включи радио ");
        } else if (strstr(command, "play radio ")) {
            station_name = strstr(command, "play radio ") + strlen("play radio ");
        }

        if (station_name && strlen(station_name) > 0) {
            return RadioPlayByName(station_name);
        }
    }

    return false;
}
