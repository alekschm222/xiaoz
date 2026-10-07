#ifndef RADIO_INTEGRATION_H
#define RADIO_INTEGRATION_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация радио-интеграции
void RadioInit(void);

// Запуск станции по имени (нечёткий поиск)
// Возвращает true если станция найдена и запущена
bool RadioPlayByName(const char* name);

// Запуск станции по индексу
void RadioPlayByIndex(int index);

// Остановка радио
void RadioStop(void);

// Пауза (при переходе в Listening/Speaking)
void RadioPause(void);

// Возобновление (при возврате в Idle)
void RadioResume(void);

// Обработка смены состояния устройства
// state: 0=Idle, 1=Connecting, 2=Listening, 3=Speaking
void RadioOnStateChange(int state);

// Обработка тача по экрану
// Возвращает true если радио было активно (и остановлено)
bool RadioOnTouch(void);

// Проверка: играет ли радио сейчас
bool RadioIsPlaying(void);

// Запрет засыпания: возвращает true если радио играет
bool RadioPreventSleep(void);

// Обновление экрана (вызывать в главном цикле ~30 FPS)
void RadioUpdateDisplay(void);

// Получить список станций строкой
const char* RadioGetStationList(void);

// Обработка текстовой команды
// Возвращает true если команда распознана как радио-команда
bool RadioHandleCommand(const char* command);

#ifdef __cplusplus
}
#endif

#endif // RADIO_INTEGRATION_H
