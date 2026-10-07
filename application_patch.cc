//=============================================================================
// ПАТЧИ ДЛЯ main/application.cc
// Основано на анализе структуры xiaozhi-esp32 (main ветка, v2.5.x)
// DeepWiki: https://deepwiki.com/78/xiaozhi-esp32
//
// Структура application.cc (актуальная):
//   Строки 23-47:    Конструктор Application()
//   Строки 61-163:   Application::Initialize()
//   Строки 165-257:  Application::Run() — главный цикл event_group_
//   Строки 227-266:  ToggleChatState()
//   Строки 259-334:  ActivationTask / OTA
//   Строки 327-509:  Initialize() — полный
//   Строки 512-524:  Schedule()
//   Строки 538-581:  HandleNetworkConnectedEvent()
//   Строки 583-619:  StartListening()
//   Строки 621-625:  StopListening()
//   Строки 632-692:  AbortSpeaking()
//
// События (application.h строки 22-34):
//   MAIN_EVENT_SCHEDULE, MAIN_EVENT_SEND_AUDIO,
//   MAIN_EVENT_WAKE_WORD_DETECTED, MAIN_EVENT_VAD_CHANGE,
//   MAIN_EVENT_ERROR, MAIN_EVENT_CHECK_NEW_VERSION_DONE,
//   MAIN_EVENT_NETWORK_CONNECTED
//
// Состояния (application.h строки 17-33):
//   kDeviceStateStarting, kDeviceStateWifiConfiguring,
//   kDeviceStateActivating, kDeviceStateIdle,
//   kDeviceStateConnecting, kDeviceStateListening,
//   kDeviceStateSpeaking, kDeviceStateUpgrading,
//   kDeviceStateAudioTesting, kDeviceStateFatalError
//=============================================================================

//-----------------------------------------------------------------------------
// ПАТЧ 1: Добавить include и глобальный объект
// ГДЕ: В начале файла, после всех #include (примерно строка 15-20)
// ПОСЛЕ: #include "application.h"
//        #include "ota.h"
//        ... (все остальные include)
//-----------------------------------------------------------------------------

#include "radio_integration.h"

// Глобальный объект радио-интеграции
// ВНИМАНИЕ: Если application.cc использует namespace, 
// поместите это ВНЕ namespace, после using namespace

//-----------------------------------------------------------------------------
// ПАТЧ 2: Инициализация радио при старте
// ГДЕ: Application::Initialize(), в самом конце функции
//      (примерно строки 160-163, перед return или закрывающей скобкой)
// ПОСЛЕ: audio_service_.Initialize();
//        Schedule([this]() { ... CheckNewVersion ... });
//        (после всей инициализации аудио и сети)
//-----------------------------------------------------------------------------

    // === РАДИО ПАТЧ 2: Инициализация радио ===
    RadioInit();
    ESP_LOGI(TAG, "Radio integration initialized");

//-----------------------------------------------------------------------------
// ПАТЧ 3: Пауза/resume при смене состояния
// ГДЕ: Внутри SetDeviceState() или обработчика смены состояния
//      По данным DeepWiki, SetDeviceState находится примерно на строках 632-692
//      Или: в обработчике MAIN_EVENT_VAD_CHANGE / HandleStateChangedEvent
//
// Если в коде есть метод HandleStateChangedEvent(DeviceState new_state):
//   ГДЕ: начало метода, после получения new_state
//
// Если нет — добавьте в SetDeviceState(DeviceState state):
//   ГДЕ: после device_state_ = state;
//-----------------------------------------------------------------------------

    // === РАДИО ПАТЧ 3: Пауза/resume при смене состояния ===
    // kDeviceStateIdle = 0, kDeviceStateListening = 5, kDeviceStateSpeaking = 6
    // (проверьте точные значения в application.h!)
    int radio_state = 0;  // default: idle
    if (state == kDeviceStateListening) radio_state = 2;
    else if (state == kDeviceStateSpeaking) radio_state = 3;
    RadioOnStateChange(radio_state);

//-----------------------------------------------------------------------------
// ПАТЧ 4: Тап по экрану — стоп радио + слушание без wake word
// ГДЕ: В ToggleChatState() (примерно строки 227-266)
//      В самом начале метода, ДО проверки текущего состояния
//
// ToggleChatState() вызывается при:
//   - Нажатии кнопки BOOT
//   - Тапе по экрану (если поддерживается платой)
//   - Wake word "Hi ESP"
//-----------------------------------------------------------------------------

    // === РАДИО ПАТЧ 4: Тап/кнопка выключает радио ===
    if (RadioOnTouch()) {
        // Радио было активно — остановили его
        // Продолжаем выполнение ToggleChatState()
        // Это переведёт в StartListening() без необходимости wake word
        ESP_LOGI(TAG, "Radio stopped by touch/button, starting listening");
    }

//-----------------------------------------------------------------------------
// ПАТЧ 5: Пауза перед записью (дублирование для надёжности)
// ГДЕ: В начале StartListening() (примерно строки 583-619)
//      ДО: protocol_->OpenAudioChannel();
//          audio_service_->StartRecording();
//-----------------------------------------------------------------------------

    // === РАДИО ПАТЧ 5: Пауза радио перед записью ===
    if (RadioIsPlaying()) {
        RadioPause();
    }

//-----------------------------------------------------------------------------
// ПАТЧ 6: Resume после разговора
// ГДЕ: В конце StopListening() (примерно строки 621-625)
//      ПОСЛЕ: SetDeviceState(kDeviceStateIdle);
//-----------------------------------------------------------------------------

    // === РАДИО ПАТЧ 6: Resume радио после разговора ===
    RadioResume();

//-----------------------------------------------------------------------------
// ПАТЧ 7: Запрет засыпания
// ГДЕ: В Application::Run(), внутри главного цикла while(1),
//      в обработчике MAIN_EVENT_SCHEDULE (или перед проверкой таймера сна)
//      Примерно строки 184-257
//
// Вариант A: Если есть PowerSaveTimer или SleepTimer
//   ГДЕ: перед вызовом power_save_timer_->Refresh() или аналогичным
//
// Вариант B: Если есть проверка clock_ticks_ или idle_timeout
//   ГДЕ: перед if (clock_ticks_ > IDLE_TIMEOUT) или аналогичным
//-----------------------------------------------------------------------------

    // === РАДИО ПАТЧ 7: Запрет засыпания при игре радио ===
    if (RadioPreventSleep()) {
        // Сброс таймера сна — устройство не заснёт пока играет радио
        // Замените clock_ticks_ на вашу переменную таймера
        clock_ticks_ = 0;  // ИЛИ: power_save_timer_->Refresh();
        // Не return — продолжаем цикл
    }

//-----------------------------------------------------------------------------
// ПАТЧ 8: Обновление спектроанализатора на экране
// ГДЕ: В Application::Run(), в самом конце цикла while(1),
//      после всех обработчиков событий (примерно строка 255-257)
//      ПЕРЕД закрывающей скобкой цикла
//-----------------------------------------------------------------------------

    // === РАДИО ПАТЧ 8: Отрисовка спектра ===
    RadioUpdateDisplay();

//-----------------------------------------------------------------------------
// ДОПОЛНИТЕЛЬНО: Регистрация MCP tools для голосовых команд
// ГДЕ: В mcp_server.cc (или в Initialize() после создания McpServer)
//
// Если LLM на сервере xiaozhi.me должна вызывать радио-команды,
// зарегистрируйте три MCP tools:
//-----------------------------------------------------------------------------

/*
// В mcp_server.cc или аналогичном:

// Tool 1: Включить радио
mcp_server_.AddTool("play_radio",
    "Включить интернет-радио. Параметр station — название станции.",
    "{\"station\": \"название станции\"}",
    [](const cJSON* args) -> cJSON* {
        const char* name = cJSON_GetObjectItem(args, "station")->valuestring;
        bool ok = RadioPlayByName(name);
        cJSON* result = cJSON_CreateObject();
        cJSON_AddBoolToObject(result, "success", ok);
        cJSON_AddStringToObject(result, "message",
            ok ? "Радио запущено" : "Станция не найдена");
        return result;
    });

// Tool 2: Остановить радио
mcp_server_.AddTool("stop_radio",
    "Остановить интернет-радио.",
    "{}",
    [](const cJSON* args) -> cJSON* {
        RadioStop();
        cJSON* result = cJSON_CreateObject();
        cJSON_AddBoolToObject(result, "success", true);
        return result;
    });

// Tool 3: Список станций
mcp_server_.AddTool("list_radio_stations",
    "Получить список доступных радиостанций.",
    "{}",
    [](const cJSON* args) -> cJSON* {
        cJSON* result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "stations", RadioGetStationList());
        return result;
    });
*/

//-----------------------------------------------------------------------------
// ДОПОЛНИТЕЛЬНО: Обработка тача на FNK0104B
// ГДЕ: В обработчике кнопки или в цикле Run()
//      Если плата поддерживает тач через FT6336G
//
// Вариант 1: Добавить в board инициализацию обработчик тача
// Вариант 2: Опрашивать тач в главном цикле
//-----------------------------------------------------------------------------

/*
// В Application::Run(), после RadioUpdateDisplay():

    // Проверка тача
    static int touch_x = 0, touch_y = 0;
    if (board_touch_read(&touch_x, &touch_y)) {
        // Дебаунс
        static int64_t last_touch_time = 0;
        int64_t now = esp_timer_get_time() / 1000;
        if (now - last_touch_time > 300) {  // 300 мс дебаунс
            last_touch_time = now;
            if (RadioOnTouch()) {
                // Радио остановлено — запускаем слушание
                StartListening();
            }
        }
    }
*/
