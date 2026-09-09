#pragma once

#include <cstdint>

#include "transport.h" 
#include "channels.h"
#include "physics.h"

/**
 * @brief Тип функции пользовательской задачи клиента.
 */
using ClientTaskFunc = void (*)(void* param);

/**
 * @brief Класс приложения уровня приложения протокола MRPC.
 * 
 * Предоставляет пользователю интерфейс для инициализации физического уровня,
 * регистрации функций, выполнения вызовов (call) и потоковой передачи (stream).
 * Управляет задачами FreeRTOS для чтения данных и пользовательскими задачами.
 */
class App {
public:
    /**
     * @brief Конструктор с привязкой к физическому уровню.
     * @param phys Ссылка на объект физического уровня.
     */
    App(Physics& phys);
    ~App() {
        stop();
    };

    // Запрещаем копирование
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    /**
     * @brief Инициализация задачи приема.
     * @param priority Приоритет задачи (по умолчанию 2).
     * @return true при успешном создании задачи.
     */
    bool start(UBaseType_t priority = 2);

    /**
     * @brief Остановка приложения и удаление задач.
     */
    void stop();

    /**
     * @brief Регистрация удаленной функции.
     * @param name Текстовое название функции.
     * @param handler Указатель на функцию-обработчик.
     * @return true если регистрация успешна.
     */
    bool regFunc(const char* name, RpcHandler handler);

    /**
     * @brief Выполнение удаленного вызова.
     * 
     * Формирует сообщение 0x0B (запрос), отправляет его и блокирует текущую задачу RTOS
     * до получения ответа ("Обеспечить возможность ожидания ответа на отправленный запрос").
     * 
     * @param name Имя удаленной функции.
     * @param args Аргументы вызова.
     * @param argsLen Длина аргументов.
     * @param out Буфер для записи ответа.
     * @param outLen [in/out] Размер буфера / полученная длина ответа.
     * @param timeoutMs Таймаут ожидания в миллисекундах.
     * @return Код статуса выполнения.
     */
    CallStatus call(const char* name, 
                    const uint8_t* args, uint16_t argsLen, 
                    uint8_t* out, uint16_t* outLen, 
                    uint32_t timeoutMs);

    /**
     * @brief Запуск потоковой передачи данных.
     * @param name Имя удаленной функции.
     * @param args Аргументы вызова.
     * @param argsLen Длина аргументов.
     * @param Chunk Колбэк для обработки чанков данных.
     * @param stepTimeoutMs Таймаут ожидания каждого чанка (по умолчанию 1000 мс).
     * @return Код статуса выполнения.
     */
    CallStatus stream(const char* name, const uint8_t* args, uint16_t argsLen, StreamCallback Chunk, uint32_t stepTimeoutMs = 1000);

    /**
     * @brief Проверка состояния соединения.
     * @return true если канал связи активен.
     */
    bool isConnected() const;

    /**
     * @brief Создание пользовательской клиентской задачи.
     * @param taskFunc Функция задачи.
     * @param name Имя задачи.
     * @param stackBytes Размер стека в байтах.
     * @param priority Приоритет задачи.
     * @return true при успешном создании.
     */
    bool createClientTask(ClientTaskFunc taskFunc,
                         const char* name = "ClientTask",
                         uint32_t stackBytes = 8192,
                         UBaseType_t priority = 1);
    
    /**
     * @brief Создание пользовательской задачи для стрима.
     * @param taskFunc Функция задачи.
     * @param name Имя задачи.
     * @param stackBytes Размер стека в байтах.
     * @param priority Приоритет задачи.
     * @return true при успешном создании.
     */
    bool createClientStreamTask(ClientTaskFunc taskFunc,
                               const char* name = "ClientStreamTask",
                               uint32_t stackBytes = 8192,
                               UBaseType_t priority = 1);

private:
    static void rxTask(void* param);
    
    // Основной цикл задачи приема
    void rxLoop();

    Physics& m_physics;
    Channels m_channels;
    Transport m_transport;
    
    // Задачи
    TaskHandle_t m_rxTaskHndl = nullptr;
    TaskHandle_t m_clientTaskHndl = nullptr;
    TaskHandle_t m_clientStreamTaskHndl = nullptr;

    bool createTask(ClientTaskFunc taskFunc, const char* name,
                    uint32_t stackBytes, UBaseType_t priority,
                    TaskHandle_t& handle);
};