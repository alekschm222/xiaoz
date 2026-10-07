#include "radio_player.h"
#include "mp3dec.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "RADIO_PLAYER";

// Размеры буферов
#define HTTP_BUFFER_SIZE    (2048)
#define PCM_BUFFER_SIZE      (2304 * 2)  // Макс. MP3 кадр: 1152 сэмпла * 2 канала
#define RING_BUFFER_SIZE     (PCM_BUFFER_SIZE * 4)  // Кольцевой буфер для спектра
#define MAX_RECONNECT_TRIES  5
#define RECONNECT_DELAY_MS   2000

// Состояние
static bool s_is_playing = false;
static bool s_is_paused = false;
static bool s_was_playing = false;
static char s_current_url[256] = {0};
static TaskHandle_t s_task_handle = NULL;
static EventGroupHandle_t s_event_group = NULL;

// Буферы в PSRAM
static uint8_t* s_http_buffer = NULL;      // Буфер для HTTP-данных
static int16_t* s_pcm_buffer = NULL;       // Буфер для декодированного PCM
static int16_t* s_ring_buffer = NULL;     // Кольцевой буфер для спектра
static volatile int s_ring_write_idx = 0;
static volatile int s_ring_read_idx = 0;

// I2S (объявления из board config)
extern void board_i2s_write(int16_t* data, size_t samples);
extern void board_i2s_tx_enable(void);
extern void board_i2s_tx_disable(void);

void radio_player_init(void) {
    ESP_LOGI(TAG, "Initializing radio player");

    s_http_buffer = (uint8_t*)heap_caps_calloc(1, HTTP_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    s_pcm_buffer = (int16_t*)heap_caps_calloc(1, PCM_BUFFER_SIZE * 2, MALLOC_CAP_SPIRAM);
    s_ring_buffer = (int16_t*)heap_caps_calloc(1, RING_BUFFER_SIZE * 2, MALLOC_CAP_SPIRAM);

    if (!s_http_buffer || !s_pcm_buffer || !s_ring_buffer) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffers!");
        return;
    }

    s_event_group = xEventGroupCreate();
    if (!s_event_group) {
        ESP_LOGE(TAG, "Failed to create event group!");
        return;
    }

    ESP_LOGI(TAG, "Radio player initialized (PSRAM buffers allocated)");
}

bool radio_player_is_playing(void) {
    return s_is_playing && !s_is_paused;
}

bool radio_player_was_playing(void) {
    return s_was_playing;
}

void radio_player_play(const char* url) {
    if (s_is_playing) {
        radio_player_stop();
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    strncpy(s_current_url, url, sizeof(s_current_url) - 1);
    s_is_playing = true;
    s_is_paused = false;
    s_was_playing = true;
    s_ring_write_idx = 0;
    s_ring_read_idx = 0;

    // Создаём задачу декодирования на ядре 1
    if (s_task_handle == NULL) {
        xTaskCreatePinnedToCore(
            radio_player_task,
            "radio_player",
            8192,
            NULL,
            4,
            &s_task_handle,
            1  // Core 1
        );
    } else {
        // Сигналим возобновление
        xEventGroupSetBits(s_event_group, RADIO_EVENT_RESUME);
    }

    ESP_LOGI(TAG, "Playing: %s", url);
}

void radio_player_stop(void) {
    if (!s_is_playing) return;

    s_is_playing = false;
    s_is_paused = false;
    xEventGroupSetBits(s_event_group, RADIO_EVENT_STOP);

    board_i2s_tx_disable();
    ESP_LOGI(TAG, "Radio stopped");
}

void radio_player_pause(void) {
    if (!s_is_playing || s_is_paused) return;
    s_is_paused = true;
    xEventGroupSetBits(s_event_group, RADIO_EVENT_PAUSE);
    board_i2s_tx_disable();
    ESP_LOGI(TAG, "Radio paused");
}

void radio_player_resume(void) {
    if (!s_is_playing || !s_is_paused) return;
    s_is_paused = false;
    xEventGroupSetBits(s_event_group, RADIO_EVENT_RESUME);
    board_i2s_tx_enable();
    ESP_LOGI(TAG, "Radio resumed");
}

// Запись сэмплов в кольцевой буфер для спектроанализатора
static void write_to_ring(int16_t* data, int samples) {
    for (int i = 0; i < samples && i < RING_BUFFER_SIZE; i++) {
        s_ring_buffer[s_ring_write_idx] = data[i];
        s_ring_write_idx = (s_ring_write_idx + 1) % RING_BUFFER_SIZE;
    }
}

int radio_player_get_pcm(int16_t* buf, int max_samples) {
    int count = 0;
    while (count < max_samples && s_ring_read_idx != s_ring_write_idx) {
        buf[count] = s_ring_buffer[s_ring_read_idx];
        s_ring_read_idx = (s_ring_read_idx + 1) % RING_BUFFER_SIZE;
        count++;
    }
    return count;
}

// Главная задача декодирования
void radio_player_task(void* arg) {
    ESP_LOGI(TAG, "Radio player task started on core %d", xPortGetCoreID());

    MP3DecoderInitStruct decoder;
    HMP3Decoder mp3dec = NULL;

    while (true) {
        if (!s_is_playing) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Обработка событий
        EventBits_t bits = xEventGroupGetBits(s_event_group);
        if (bits & RADIO_EVENT_STOP) {
            xEventGroupClearBits(s_event_group, RADIO_EVENT_STOP);
            if (mp3dec) {
                MP3FreeDecoder(mp3dec);
                mp3dec = NULL;
            }
            s_was_playing = false;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (bits & RADIO_EVENT_PAUSE) {
            xEventGroupClearBits(s_event_group, RADIO_EVENT_PAUSE);
            // Ждём resume или stop
            bits = xEventGroupWaitBits(s_event_group,
                RADIO_EVENT_RESUME | RADIO_EVENT_STOP,
                pdTRUE, pdFALSE, portMAX_DELAY);
            if (bits & RADIO_EVENT_STOP) {
                if (mp3dec) { MP3FreeDecoder(mp3dec); mp3dec = NULL; }
                s_was_playing = false;
                continue;
            }
            // resume — продолжаем
        }

        // Создаём декодер если нужно
        if (!mp3dec) {
            mp3dec = MP3CreateDecoder();
            if (!mp3dec) {
                ESP_LOGE(TAG, "Failed to create MP3 decoder!");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            ESP_LOGI(TAG, "MP3 decoder created");
        }

        // HTTP соединение
        esp_http_client_config_t config = {
            .url = s_current_url,
            .timeout_ms = 10000,
            .buffer_size = HTTP_BUFFER_SIZE,
            .buffer_size_tx = 1024,
            .user_agent = "xiaozhi-radio/1.0",
            .disable_auto_redirect = false,
            .max_redirection_count = 5,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to init HTTP client");
            if (mp3dec) { MP3FreeDecoder(mp3dec); mp3dec = NULL; }
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
            continue;
        }

        int reconnect_tries = 0;
        bool connected = false;

        while (s_is_playing && reconnect_tries < MAX_RECONNECT_TRIES) {
            esp_err_t err = esp_http_client_open(client, 0);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "HTTP open failed (try %d/%d): %s",
                    reconnect_tries + 1, MAX_RECONNECT_TRIES, esp_err_to_name(err));
                reconnect_tries++;
                vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
                continue;
            }

            int content_length = esp_http_client_fetch_headers(client);
            int status = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG, "HTTP connected, status=%d, content_length=%d", status, content_length);

            if (status != 200) {
                ESP_LOGW(TAG, "Bad status %d, retrying...", status);
                esp_http_client_close(client);
                reconnect_tries++;
                vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
                continue;
            }

            connected = true;
            board_i2s_tx_enable();
            reconnect_tries = 0;

            // Главный цикл чтения и декодирования
            int http_buf_pos = 0;
            int http_buf_len = 0;

            while (s_is_playing && !s_is_paused) {
                // Чтение данных
                if (http_buf_pos >= http_buf_len) {
                    http_buf_len = esp_http_client_read(client, (char*)s_http_buffer, HTTP_BUFFER_SIZE);
                    if (http_buf_len <= 0) {
                        ESP_LOGW(TAG, "HTTP read returned %d, reconnecting...", http_buf_len);
                        break;
                    }
                    http_buf_pos = 0;
                }

                // Поиск синхрон-слова MP3
                int bytes_left = http_buf_len - http_buf_pos;
                int offset = MP3FindSyncWord(s_http_buffer + http_buf_pos, bytes_left);
                if (offset < 0) {
                    http_buf_pos = http_buf_len;
                    continue;
                }
                http_buf_pos += offset;
                bytes_left = http_buf_len - http_buf_pos;

                // Декодирование
                int16_t* pcm = s_pcm_buffer;
                int pcm_samples = 0;
                int err = MP3Decode(mp3dec, &s_http_buffer, &http_buf_pos,
                                    pcm, &pcm_samples, 0);

                if (err != 0) {
                    ESP_LOGW(TAG, "MP3 decode error %d, skipping frame", err);
                    continue;
                }

                // Корректируем http_buf_pos (MP3Decode сдвигает указатель)
                // Записываем PCM в I2S
                int stereo_samples = pcm_samples;  // pcm_samples уже учитывает каналы
                if (stereo_samples > 0) {
                    board_i2s_write(pcm, stereo_samples);

                    // Копируем в ring buffer для спектра
                    write_to_ring(pcm, stereo_samples);
                }

                // Проверка событий без блокировки
                bits = xEventGroupGetBits(s_event_group);
                if (bits & RADIO_EVENT_STOP) {
                    xEventGroupClearBits(s_event_group, RADIO_EVENT_STOP);
                    s_is_playing = false;
                    break;
                }
                if (bits & RADIO_EVENT_PAUSE) {
                    xEventGroupClearBits(s_event_group, RADIO_EVENT_PAUSE);
                    bits = xEventGroupWaitBits(s_event_group,
                        RADIO_EVENT_RESUME | RADIO_EVENT_STOP,
                        pdTRUE, pdFALSE, portMAX_DELAY);
                    if (bits & RADIO_EVENT_STOP) {
                        s_is_playing = false;
                        break;
                    }
                    // resume
                }
            }

            board_i2s_tx_disable();
            esp_http_client_close(client);
        }

        esp_http_client_cleanup(client);

        if (mp3dec) {
            MP3FreeDecoder(mp3dec);
            mp3dec = NULL;
        }

        if (s_is_playing && reconnect_tries >= MAX_RECONNECT_TRIES) {
            ESP_LOGE(TAG, "Max reconnect tries reached, stopping");
            s_is_playing = false;
            s_was_playing = false;
        }
    }
}
