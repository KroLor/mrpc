#pragma once

#include "physics.h"
#include "server.h"
#include "client.h"
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

/**
 * @brief Реализация физического уровня для Windows на основе Named Pipe.
 * 
 * Класс использует асинхронный двунаправленный канал (Named Pipe) для передачи
 * потока байт между сервером и клиентом. Поддерживает работу как в режиме
 * сервера, так и клиента.
 */
class PhysicsForWin : public Physics {
public:
    /**
     * @brief Конструктор с указанием имени канала и роли.
     * @param pipeName Имя именованного канала.
     * @param isServer true для режима сервера, false для клиента.
     */
    PhysicsForWin(const char* pipeName, bool isServer);
    ~PhysicsForWin() override;
    
    /**
     * @brief Инициализация физического уровня.
     * @return true при успешной инициализации.
     */
    bool init() override;
    
    /**
     * @brief Деинициализация и освобождение ресурсов.
     */
    void deinit() override;

    /**
     * @brief Отправка сырых байтов в среду передачи.
     * @param data Указатель на данные.
     * @param len Количество байт для отправки.
     * @return true при успешной отправке.
     */
    bool send(const uint8_t* data, uint16_t len) override;
    
    /**
     * @brief Прием сырых байтов из среды передачи.
     * @param data Буфер для принятых данных.
     * @param maxSize Максимальный размер буфера.
     * @return Количество реально принятых байт.
     */
    uint16_t recv(uint8_t* data, uint16_t maxSize) override;

    /**
     * @brief Обновление состояния соединения и обработка операций чтения/записи.
     */
    void update() override;

    /**
     * @brief Проверка состояния соединения.
     * @return true если канал активен.
     */
    bool isConnected() const override;

private:
    std::string m_pipeName;
    bool m_isServer;
    std::unique_ptr<PipeServer> m_server;
    std::unique_ptr<PipeClient> m_client;
    std::vector<uint8_t> m_leftover;
    bool m_init = false;
};
