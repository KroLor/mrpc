#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "transport.h" 
#include "channels.h"
#include "physics.h"

using ClientTaskFunc = void (*)(void* param);

class App {
public:
    App(Physics& phys);
    ~App() {
        stop();
    };

    // Запрещаем копирование
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    /**
     * @brief Инициализация задачи приема.
     * @return true при успешном создании задачи.
     */
    bool start();

    /**
     * @brief Остановка узла.
     */
    void stop();

    /**
     * @param name Текстовое название функции.
     * @param handler Указатель на функцию-обработчик.
     * @return true если регистрация успешна.
     */
    bool regFunc(const char* name, RpcHandler handler);

    /**
     * Формирует сообщение 0x0B (запрос), отправляет его и блокирует текущую задачу RTOS.
     * ("Обеспечить возможность ожидания ответа на отправленный запрос")
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

    CallStatus stream(const char* name, const uint8_t* args, uint16_t argsLen, StreamCallback Chunk, uint32_t stepTimeoutMs = 1000);

    /**
     * @brief Проверка состояния соединения.
     */
    bool isConnected() const;

    bool startClientTask(ClientTaskFunc taskFunc,
                         const char* name = "ClientTask",
                         uint32_t stackBytes = 8192,
                         UBaseType_t priority = 1);
    bool startClientStreamTask(ClientTaskFunc taskFunc,
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