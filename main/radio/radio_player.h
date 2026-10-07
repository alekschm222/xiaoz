#ifndef RADIO_PLAYER_H
#define RADIO_PLAYER_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_http_client.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

// События радио
#define RADIO_EVENT_STOP        (1 << 0)
#define RADIO_EVENT_PAUSE       (1 << 1)
#define RADIO_EVENT_RESUME      (1 << 2)
#define RADIO_EVENT_ALL         (RADIO_EVENT_STOP | RADIO_EVENT_PAUSE | RADIO_EVENT_RESUME)

// Инициализация плеера
void radio_player_init(void);

// Запустить воспроизведение URL
void radio_player_play(const char* url);

// Остановить воспроизведение
void radio_player_stop(void);

// Пауза (не закрывает соединение)
void radio_player_pause(void);

// Возобновление после паузы
void radio_player_resume(void);

// Идёт ли воспроизведение
bool radio_player_is_playing(void);

// Было ли воспроизведение до паузы
bool radio_player_was_playing(void);

// Получить текущие сэмплы для спектроанализатора
// Возвращает количество стерео-сэмплов, скопированных в буфер
int radio_player_get_pcm(int16_t* buf, int max_samples);

// Задача FreeRTOS для декодирования
void radio_player_task(void* arg);

#ifdef __cplusplus
}
#endif

#endif // RADIO_PLAYER_H
