#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>

using byte = uint8_t;

class PipeClient {
public:
    PipeClient();
    ~PipeClient();

    // Подключение к серверу
    bool connect(const char* pipeName);
    // Обновление состояния
    void update();
    // Отправка сырого массива байт
    bool sendData(const std::vector<byte>& data);
    // Закрытие соединения
    void disconnect();
    std::vector<byte> getReceivedData();
    bool isConnected() const { return state == CONNECTED; }

    bool waitForData(uint32_t timeoutMs);

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