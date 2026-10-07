# XiaoZhi Radio Addon для FNK0104B

Интернет-радио + цветной спектроанализатор для голосового ассистента XiaoZhi на плате FNK0104B (ESP32-S3 WROOM N16R8).

## Возможности

- 🎵 Интернет-радио (16 станций: Маруся FM, Русское Радио, Европа Плюс и др.)
- 🌈 Цветной спектроанализатор на экране ILI9341 (24 полосы, градиент синий→красный)
- 🎤 Голосовое управление: "включи радио Маруся FM", "стоп радио"
- 👆 Тап по экрану — стоп радио + мгновенное слушание команды
- 💤 Радио не даёт устройству заснуть
- 🔇 Авто-пауза во время разговора с ассистентом
- 📋 "какие есть радиостанции?" — список станций

## Установка

### 1. Клонировать базовый проект

```bash
git clone https://github.com/78/xiaozhi-esp32.git
cd xiaozhi-esp32
```

### 2. Распаковать архив

Распакуйте `xiaozhi-radio-fnk0104b-full.zip` и скопируйте папки:
- `components/helix-mp3/` → `xiaozhi-esp32/components/helix-mp3/`
- `components/lcd_ili9341/` → `xiaozhi-esp32/components/lcd_ili9341/`
- `main/radio/` → `xiaozhi-esp32/main/radio/`
- `main/boards/freenove-esp32s3-display/` → `xiaozhi-esp32/main/boards/freenove-esp32s3-display/`

### 3. Заменить MP3-декодер на полный Helix

```bash
rm -rf components/helix-mp3
git clone https://github.com/chmorgan/esp-libhelix-mp3.git components/helix-mp3
```

> Stub в архиве парсит кадры, но выводит тишину. Полный Helix даёт реальный звук.

### 4. Внести патчи в application.cc

Откройте `application_patch.cc` и следуйте 8 точкам патча:

1. Добавьте `#include "radio_integration.h"` после всех include
2. В `Initialize()`, в конце → `RadioInit();`
3. В `SetDeviceState()` → `RadioOnStateChange(state);`
4. В начале `ToggleChatState()` → `if (RadioOnTouch()) { ... }`
5. В начале `StartListening()` → `if (RadioIsPlaying()) RadioPause();`
6. В конце `StopListening()` → `RadioResume();`
7. В `Run()`, перед проверкой сна → `if (RadioPreventSleep()) clock_ticks_ = 0;`
8. В конце цикла `Run()` → `RadioUpdateDisplay();`

### 5. Обновить CMakeLists.txt

Добавьте строки из `CMakeLists_radio_addon.txt` в `main/CMakeLists.txt`:
- Файлы `radio/radio_player.cc`, `radio/spectrum_analyzer.cc`, `radio/radio_integration.cc`, `boards/freenove-esp32s3-display/config.cc` в SRCS
- `helix-mp3` и `lcd_ili9341` в REQUIRES

### 6. Настроить промпт на xiaozhi.me

1. Зайдите на https://xiaozhi.me → Console → Configure Role
2. Скопируйте содержимое `server_prompt.md`

### 7. Собрать и прошить

```bash
idf.py set-target esp32s3
idf.py menuconfig   # выбрать board type: freenove-esp32s3-display
idf.py build
idf.py -p COMx flash monitor
```

## Пины FNK0104B

| Устройство | GPIO | Описание |
|-----------|------|----------|
| LCD CS | 10 | SPI Chip Select |
| LCD DC | 46 | Data/Command |
| LCD SCK | 12 | SPI Clock |
| LCD MOSI | 11 | SPI MOSI |
| LCD MISO | 13 | SPI MISO |
| LCD BL | 45 | Подсветка |
| Touch SDA | 16 | I2C Data (FT6336G) |
| Touch SCL | 15 | I2C Clock |
| Touch RST | 18 | Reset |
| Touch INT | 17 | Interrupt |
| Audio PA | 1 | Усилитель (low = вкл) |
| Audio MCLK | 4 | I2S MCLK |
| Audio BCLK | 5 | I2S BCLK |
| Audio DOUT | 8 | I2S → ES8311 DAC |
| Audio LRCK | 7 | I2S WS |
| Audio DIN | 6 | I2S ← ES8311 ADC |

## Станции

1. Маруся FM — https://marusya.hostingradio.ru/marusya128.mp3
2. Русское Радио — https://rusradio.hostingradio.ru/rusradio128.mp3
3. Европа Плюс — https://eplayer.hostingradio.ru/europaplus128.mp3
4. Радио Maximum — https://maximum.hostingradio.ru/maximum128.mp3
5. Наше Радио — https://nashe.hostingradio.ru/nashe128.mp3
6. Радио Шансон — https://chanson.hostingradio.ru/chanson128.mp3
7. Радио DFM — https://dfm.hostingradio.ru/dfm128.mp3
8. Радио Маяк — https://mayakfm.hostingradio.ru/mayak128.mp3
9. Вести FM — https://vestifm.hostingradio.ru/vestifm128.mp3
10. Радио ENERGY — https://energyfm.hostingradio.ru/energy128.mp3
11. Юмор FM — https://humorfm.hostingradio.ru/humorfm128.mp3
12. Авторадио — https://avtoradio.hostingradio.ru/avtoradio128.mp3
13. Радио Romantika — https://romantika.hostingradio.ru/romantika128.mp3
14. Радио Jazz — https://jazzfm.hostingradio.ru/jazzfm128.mp3
15. Love Radio — https://love.hostingradio.ru/love128.mp3
16. Радио 7 — https://radio7.hostingradio.ru/radio7128.mp3

## Структура

```
xiaozhi-radio-fnk0104b-full/
├── components/
│   ├── helix-mp3/          ← MP3-декодер (stub → заменить на полный)
│   └── lcd_ili9341/        ← Драйвер ILI9341 SPI
├── main/
│   ├── radio/
│   │   ├── radio_stations.h        ← 16 станций + поиск
│   │   ├── radio_player.h/.cc      ← HTTP→MP3→I2S плеер
│   │   ├── spectrum_analyzer.h/.cc ← DFT, 24 полосы
│   │   └── radio_integration.h/.cc ← Хуки для application.cc
│   └── boards/freenove-esp32s3-display/
│       ├── config.h                ← Пины FNK0104B
│       └── config.cc               ← I2S, I2C, Touch init
├── application_patch.cc            ← 8 патчей
├── server_prompt.md                ← Промпт для xiaozhi.me
├── CMakeLists_radio_addon.txt      ← Добавки для CMakeLists.txt
├── sdkconfig.fragment              ← PSRAM, CPU
├── partitions_radio.csv             ← Таблица разделов
└── README.md                       ← Этот файл
```

## Решение проблем

| Проблема | Решение |
|---------|---------|
| Нет звука | Замените stub MP3 на полный Helix (шаг 3) |
| Чёрный экран | Проверьте пины в config.h |
| Не компилируется | Проверьте CMakeLists.txt — добавлены ли radio файлы |
| Радио не запускается | Проверьте WiFi-соединение и URL станций |
| Эхо микрофона | Уменьшите громкость или включите AEC в menuconfig |
| OOM | Убедитесь что PSRAM включена в menuconfig |
