#pragma once

#include <windows.h>
#include <vector>
#include <cstdint>

/**
 * @brief Сервер для работы с Named Pipe на Windows.
 * 
 * Класс создает именованный канал, ожидает подключения клиента,
 * принимает и отправляет данные в асинхронном режиме.
 */
class PipeServer {
public:
    PipeServer();
    ~PipeServer();

    /**
     * @brief Инициализация сервера: создание канала и ожидание клиента.
     * @param pipeName Имя именованного канала (например, "\\\\.\\pipe\\MyPipe").
     * @return true при успешном создании канала.
     */
    bool init(const char* pipeName);
    
    /**
     * @brief Обновление состояния сервера: проверка операций чтения/записи.
     */
    void update();
    
    /**
     * @brief Закрытие канала и завершение работы сервера.
     */
    void close();
    
    /**
     * @brief Отправка сырого массива байт клиенту.
     * @param data Вектор байтов для отправки.
     * @return true при успешной постановке данных на отправку.
     */
    bool sendData(const std::vector<byte>& data);
    
    /**
     * @brief Получение накопленных принятых данных от клиента.
     * @return Вектор байтов, полученных от клиента.
     */
    std::vector<byte> getReceivedData();

    /**
     * @brief Проверка состояния соединения с клиентом.
     * @return true если клиент подключен и готов к чтению.
     */
    bool isConnected() const { return state == WAITING_READ; }

    /**
     * @brief Ожидание появления данных с таймаутом.
     * @param timeoutMs Таймаут ожидания в миллисекундах.
     * @return true если данные получены за время таймаута.
     */
    bool waitForData(uint32_t timeoutMs);

    /**
     * @brief Проверка наличия принятых данных в буфере.
     * @return true если есть данные для чтения.
     */
    bool hasReceivedData() const { return !receivedData.empty(); }

private:
    HANDLE hPipe;
    OVERLAPPED ov;
    OVERLAPPED ovWrite;
    byte buffer[512];
    std::vector<byte> receivedData;
    std::vector<byte> txBuffer;
    bool isWriting;
    enum State { WAITING_CONNECT, WAITING_READ, CLIENT_CONNECTED } state;
};