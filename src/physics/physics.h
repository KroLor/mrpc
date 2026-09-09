#pragma once

#include <cstdint>

/**
 * @brief Абстрактный класс физического уровня протокола.
 * 
 * Определяет интерфейс для работы со средой передачи данных.
 * Реализация зависит от платформы (например, PhysicsForWin для Windows).
 */
class Physics {
public:
    virtual ~Physics() = default;
    
    /**
     * @brief Инициализация физического уровня.
     * @return true при успешной инициализации.
     */
    virtual bool init() = 0;
    
    /**
     * @brief Деинициализация и освобождение ресурсов.
     */
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
     * @brief Обновление состояния соединения и обработка операций чтения/записи.
     */
    virtual void update() = 0;
    
    /**
     * @brief Проверка состояния соединения.
     * @return true если канал активен, false если связь потеряна.
     */
    virtual bool isConnected() const = 0;
};

// class PhysicsESP32 ::