#include "config.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include <string.h>

static const char* TAG = "BOARD_CONFIG";

static i2s_chan_handle_t s_tx_chan = NULL;
static i2s_chan_handle_t s_rx_chan = NULL;

// I2S запись в динамик
void board_i2s_write(int16_t* data, size_t samples) {
    if (!s_tx_chan) return;
    size_t bytes_written;
    i2s_channel_write(s_tx_chan, data, samples * 2, &bytes_written, portMAX_DELAY);
}

void board_i2s_tx_enable(void) {
    if (s_tx_chan) i2s_channel_enable(s_tx_chan);
    gpio_set_level(AUDIO_PIN_PA_EN, 0);  // Включить усилитель
}

void board_i2s_tx_disable(void) {
    if (s_tx_chan) i2s_channel_disable(s_tx_chan);
    gpio_set_level(AUDIO_PIN_PA_EN, 1);  // Выключить усилитель
}

// I2S чтение с микрофона
int board_i2s_read(int16_t* data, size_t max_samples) {
    if (!s_rx_chan) return 0;
    size_t bytes_read;
    i2s_channel_read(s_rx_chan, data, max_samples * 2, &bytes_read, pdMS_TO_TICKS(100));
    return bytes_read / 2;
}

// Чтение тача FT6336G
bool board_touch_read(int* x, int* y) {
    uint8_t data[7] = {0};
    uint8_t reg = 0x00;  // Регистр статуса тача

    i2c_master_write_read_device(TOUCH_I2C_PORT, TOUCH_I2C_ADDR,
        &reg, 1, data, 7, pdMS_TO_TICKS(100));

    uint8_t touch_count = data[2] & 0x0F;
    if (touch_count == 0) return false;

    *x = ((data[3] & 0x0F) << 8) | data[4];
    *y = ((data[5] & 0x0F) << 8) | data[6];

    // Поворот координат для ориентации дисплея
    int tmp = *x;
    *x = *y;
    *y = LCD_HEIGHT - tmp;

    return true;
}

// Инициализация I2S для ES8311
void board_i2s_init(void) {
    ESP_LOGI(TAG, "Initializing I2S for ES8311");

    // PA пин
    gpio_config_t pa_conf = {
        .pin_bit_mask = (1ULL << AUDIO_PIN_PA_EN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&pa_conf);
    gpio_set_level(AUDIO_PIN_PA_EN, 1);  // Выкл усилитель по умолчанию

    // I2S TX (в динамик)
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,
        I2S_ROLE_MASTER);
    tx_chan_cfg.dma_desc_num = 6;
    tx_chan_cfg.dma_frame_num = 240;

    i2s_new_channel(&tx_chan_cfg, &s_tx_chan, NULL);

    i2s_std_clk_config_t tx_clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE);
    i2s_std_slot_config_t tx_slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);

    gpio_set_direction(AUDIO_PIN_BCLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(AUDIO_PIN_LRCK, GPIO_MODE_OUTPUT);
    gpio_set_direction(AUDIO_PIN_DOUT, GPIO_MODE_OUTPUT);

    i2s_channel_init_std_mode(s_tx_chan, &tx_clk_cfg, &tx_slot_cfg,
        {.gpio = {
            .bclk = AUDIO_PIN_BCLK,
            .ws = AUDIO_PIN_LRCK,
            .dout = AUDIO_PIN_DOUT,
            .din = -1,
            .mclk = AUDIO_PIN_MCLK,
        }});
}

// Полная инициализация платы
void board_init(void) {
    ESP_LOGI(TAG, "Initializing FNK0104B board");

    // I2C для тача (FT6336G) и аудиокодека (ES8311)
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TOUCH_PIN_SDA,
        .scl_io_num = TOUCH_PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = 400000,
        },
        .clk_flags = 0,
    };
    i2c_param_config(TOUCH_I2C_PORT, &i2c_conf);
    i2c_driver_install(TOUCH_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);

    // RST тача
    gpio_config_t rst_conf = {
        .pin_bit_mask = (1ULL << TOUCH_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rst_conf);
    gpio_set_level(TOUCH_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TOUCH_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    // I2S
    board_i2s_init();

    ESP_LOGI(TAG, "Board initialized");
}
