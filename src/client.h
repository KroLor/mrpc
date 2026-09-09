#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>

/**
 * @brief Клиент для работы с Named Pipe на Windows.
 * 
 * Класс реализует асинхронное подключение к серверу через Named Pipe,
 * отправку и получение данных в виде потока байт.
 */
class PipeClient {
public:
    PipeClient();
    ~PipeClient();

    /**
     * @brief Подключение к серверу по указанному имени канала.
     * @param pipeName Имя именованного канала (например, "\\\\.\\pipe\\MyPipe").
     * @return true при успешном подключении.
     */
    bool connect(const char* pipeName);
    
    /**
     * @brief Обновление состояния клиента: проверка завершения операций чтения/записи.
     */
    void update();
    
    /**
     * @brief Отправка сырого массива байт в канал.
     * @param data Вектор байтов для отправки.
     * @return true при успешной постановке данных на отправку.
     */
    bool sendData(const std::vector<byte>& data);
    
    /**
     * @brief Закрытие соединения с сервером.
     */
    void disconnect();
    
    /**
     * @brief Получение накопленных принятых данных.
     * @return Вектор байтов, полученных от сервера.
     */
    std::vector<byte> getReceivedData();
    
    /**
     * @brief Проверка состояния соединения.
     * @return true если клиент подключен.
     */
    bool isConnected() const { return state == CONNECTED; }

    /**
     * @brief Проверка наличия принятых данных в буфере.
     * @return true если есть данные для чтения.
     */
    bool hasReceivedData() const { return !receivedData.empty(); }

private:
    HANDLE hPipe;
    OVERLAPPED ov;
    OVERLAPPED ovRead;
    BYTE buffer[512];
    std::vector<byte> receivedData;
    std::vector<byte> txBuffer; // Очередь байт, ожидающих отправки
    bool isWriting;
    enum State { DISCONNECTED, CONNECTED } state;

    void pumpWrite();
    CRITICAL_SECTION m_cs;
    std::vector<byte> txWriting;
};