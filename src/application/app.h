#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "transport.h" 
#include "channels.h"
#include "physics.h"

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
     * @brief Инициализация и запуск узла.
     * @return true при успешном создании задачи и инициализации.
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

    // bool stream(const char* name, const uint8_t* args, uint16_t argsLen);

    /**
     * @brief Проверка состояния соединения.
     */
    bool isConnected() const;

private:
    static void rxTaskEntry(void* param);
    
    // Основной цикл задачи приема
    void rxLoop();

    Physics& m_physics;
    Channels m_channels;
    Transport m_transport;
    
    TaskHandle_t m_rxTaskHandle;
};