#ifndef FNK0104B_CONFIG_H
#define FNK0104B_CONFIG_H

// Пины FNK0104B (ESP32-S3 WROOM N16R8)
// Источник: lcdwiki.com, docs.freenove.com

// LCD ILI9341 (SPI)
#define LCD_PIN_CS       10
#define LCD_PIN_DC       46
#define LCD_PIN_SCK      12
#define LCD_PIN_MOSI     11
#define LCD_PIN_MISO     13
#define LCD_PIN_BL       45
#define LCD_WIDTH        240
#define LCD_HEIGHT       320
#define LCD_SPI_HOST     SPI2_HOST

// Touch FT6336G (I2C)
#define TOUCH_PIN_SDA    16
#define TOUCH_PIN_SCL    15
#define TOUCH_PIN_RST    18
#define TOUCH_PIN_INT    17
#define TOUCH_I2C_ADDR   0x38
#define TOUCH_I2C_PORT   I2C_NUM_1

// Audio ES8311 (I2S)
#define AUDIO_PIN_MCLK   4
#define AUDIO_PIN_BCLK   5
#define AUDIO_PIN_DOUT   8   // → ES8311 DAC
#define AUDIO_PIN_LRCK   7
#define AUDIO_PIN_DIN    6   // ← ES8311 ADC (микрофон)
#define AUDIO_PIN_PA_EN  1   // Усилитель (low = вкл)
#define AUDIO_I2C_SDA    17  // ES8311 I2C (общий с тач)
#define AUDIO_I2C_SCL    18
#define AUDIO_I2C_ADDR   0x18
#define AUDIO_SAMPLE_RATE 44100

// SD-карта (опционально)
#define SD_PIN_CS        42
#define SD_PIN_SCK       40
#define SD_PIN_MOSI      41
#define SD_PIN_MISO      39

// Батарея (ADC)
#define BATT_ADC_PIN     4   // Через делитель

// Кнопка BOOT
#define BOOT_BUTTON_PIN  0

#endif // FNK0104B_CONFIG_H
