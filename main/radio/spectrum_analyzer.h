#ifndef SPECTRUM_ANALYZER_H
#define SPECTRUM_ANALYZER_H

#include <stdint.h>
#include <stdbool.h>

#define SPECTRUM_NUM_BANDS  24
#define SPECTRUM_FFT_SIZE   512

// Инициализация
void spectrum_analyzer_init(void);

// Обработка PCM сэмплов
void spectrum_analyzer_feed(int16_t* samples, int count);

// Получение текущих значений полос (0.0 — 1.0)
// bars — текущие значения, peaks — пиковые значения
void spectrum_analyzer_get(float* bars, float* peaks);

// Сброс
void spectrum_analyzer_reset(void);

#endif // SPECTRUM_ANALYZER_H
