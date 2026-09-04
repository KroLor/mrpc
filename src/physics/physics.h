#pragma once

#include <stdint.h>
#include <stdbool.h>

class Physics {
public:
    virtual ~Physics() = default;
    
    virtual bool init() = 0;
    virtual void deinit() = 0;
    
    /**
     * @brief Отправка сырых байтов в среду передачи.
     * @param data Указатель на данные.
     * @param len Количество байт для отправки.
     * @return true при успешной отправке.
     */
    virtual bool send(const uint8_t* data, uint16_t len) = 0;
    
    /**
     * @brief Прием сырых байтов из среды передачи.
     * @param data Буфер для принятых данных.
     * @param maxSize Максимальный размер буфера.
     * @param timeoutMs Таймаут ожидания в миллисекундах.
     * @return Количество реально принятых байт.
     */
    virtual uint16_t recv(uint8_t* data, uint16_t maxSize, uint32_t timeoutMs) = 0;
    
    /**
     * @brief Проверка состояния соединения.
     * @return true если канал активен, false если связь потеряна.
     */
    virtual bool isConnected() const = 0;
};