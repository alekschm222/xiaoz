#include "spectrum_analyzer.h"
#include <math.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char* TAG = "SPECTRUM";

// Окно Хэннинга
static float s_hann_window[SPECTRUM_FFT_SIZE];
static bool s_window_init = false;

// Входной буфер
static int16_t* s_input_buffer = NULL;
static volatile int s_input_count = 0;

// Результаты
static float s_bars[SPECTRUM_NUM_BANDS];
static float s_peaks[SPECTRUM_NUM_BANDS];

// Логарифмические границы полос (индексы в DFT)
static int s_band_start[SPECTRUM_NUM_BANDS];
static int s_band_end[SPECTRUM_NUM_BANDS];

static void init_bands(void) {
    // 24 логарифмические полосы от ~80 Гц до ~16000 Гц
    // При sample_rate=44100 Гц, FFT_SIZE=512:
    // bin_freq = i * 44100 / 512 ≈ i * 86.13 Гц
    // Полезных бинов: 256 (0..255)
    float min_freq = 80.0f;
    float max_freq = 16000.0f;
    float log_min = logf(min_freq);
    float log_max = logf(max_freq);

    for (int i = 0; i < SPECTRUM_NUM_BANDS; i++) {
        float f_start = expf(log_min + (log_max - log_min) * i / SPECTRUM_NUM_BANDS);
        float f_end = expf(log_min + (log_max - log_min) * (i + 1) / SPECTRUM_NUM_BANDS);
        s_band_start[i] = (int)(f_start / 86.13f);
        s_band_end[i] = (int)(f_end / 86.13f);
        if (s_band_start[i] < 1) s_band_start[i] = 1;
        if (s_band_end[i] > SPECTRUM_FFT_SIZE / 2) s_band_end[i] = SPECTRUM_FFT_SIZE / 2;
        if (s_band_end[i] <= s_band_start[i]) s_band_end[i] = s_band_start[i] + 1;
    }
}

void spectrum_analyzer_init(void) {
    ESP_LOGI(TAG, "Initializing spectrum analyzer");

    s_input_buffer = (int16_t*)heap_caps_calloc(SPECTRUM_FFT_SIZE, sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!s_input_buffer) {
        ESP_LOGE(TAG, "Failed to allocate input buffer!");
        return;
    }

    // Окно Хэннинга
    for (int i = 0; i < SPECTRUM_FFT_SIZE; i++) {
        s_hann_window[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (SPECTRUM_FFT_SIZE - 1)));
    }
    s_window_init = true;

    init_bands();
    memset(s_bars, 0, sizeof(s_bars));
    memset(s_peaks, 0, sizeof(s_peaks));
    s_input_count = 0;

    ESP_LOGI(TAG, "Spectrum analyzer initialized");
}

void spectrum_analyzer_reset(void) {
    s_input_count = 0;
    memset(s_bars, 0, sizeof(s_bars));
    memset(s_peaks, 0, sizeof(s_peaks));
}

void spectrum_analyzer_feed(int16_t* samples, int count) {
    if (!s_input_buffer) return;

    // Копируем моно (берем левый канал)
    for (int i = 0; i < count; i += 2) {
        if (s_input_count >= SPECTRUM_FFT_SIZE) {
            // Буфер полон — обрабатываем
            s_input_count = 0;
        }
        s_input_buffer[s_input_count++] = samples[i];
    }
}

// Упрощённое DFT (не FFT, но для 512 достаточно)
static void compute_dft(int16_t* input, float* magnitudes, int n) {
    for (int k = 0; k < n / 2; k++) {
        float real = 0.0f, imag = 0.0f;
        float freq = 2.0f * M_PI * k / n;

        for (int i = 0; i < n; i++) {
            float sample = (float)input[i] * s_hann_window[i];
            real += sample * cosf(freq * i);
            imag -= sample * sinf(freq * i);
        }

        magnitudes[k] = sqrtf(real * real + imag * imag) / (n / 2);
    }
}

void spectrum_analyzer_get(float* bars, float* peaks) {
    if (!s_input_buffer || s_input_count < SPECTRUM_FFT_SIZE) {
        // Затухание если нет данных
        for (int i = 0; i < SPECTRUM_NUM_BANDS; i++) {
            s_bars[i] *= 0.85f;
            s_peaks[i] *= 0.97f;
            if (bars) bars[i] = s_bars[i];
            if (peaks) peaks[i] = s_peaks[i];
        }
        return;
    }

    // Считаем DFT
    static float magnitudes[SPECTRUM_FFT_SIZE / 2];
    compute_dft(s_input_buffer, magnitudes, SPECTRUM_FFT_SIZE);

    // Агрегируем в полосы
    for (int b = 0; b < SPECTRUM_NUM_BANDS; b++) {
        float max_mag = 0.0f;
        for (int k = s_band_start[b]; k < s_band_end[b] && k < SPECTRUM_FFT_SIZE / 2; k++) {
            if (magnitudes[k] > max_mag) max_mag = magnitudes[k];
        }

        // Нормализация (0.0 — 1.0)
        float normalized = max_mag / 32768.0f * 8.0f;  // Усиление
        if (normalized > 1.0f) normalized = 1.0f;
        if (normalized < 0.0f) normalized = 0.0f;

        // Attack: быстрый подъём; Decay: медленный спад
        if (normalized > s_bars[b]) {
            s_bars[b] = normalized;  // attack мгновенный
        } else {
            s_bars[b] = s_bars[b] * 0.85f + normalized * 0.15f;  // decay
        }

        // Peak-hold
        if (s_bars[b] > s_peaks[b]) {
            s_peaks[b] = s_bars[b];
        } else {
            s_peaks[b] *= 0.97f;  // медленный спад пика
        }
    }

    // Сбрасываем входной буфер для следующего кадра
    s_input_count = 0;

    if (bars) memcpy(bars, s_bars, sizeof(s_bars));
    if (peaks) memcpy(peaks, s_peaks, sizeof(s_peaks));
}
