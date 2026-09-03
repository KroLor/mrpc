#pragma once

#include <windows.h>
#include <vector>
#include <string>

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
    bool isConnected() const { return state == CONNECTED; }

private:
    HANDLE hPipe;
    OVERLAPPED ov;
    std::vector<byte> txBuffer; // Очередь байт, ожидающих отправки
    bool isWriting;
    enum State { DISCONNECTED, CONNECTED } state;
};
