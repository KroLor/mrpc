#pragma once

#include <windows.h>
#include <vector>

class PipeServer {
public:
    PipeServer();
    ~PipeServer();

    bool init(const char* pipeName); // Создает канал и запускает ожидание клиента
    void update(); // Обновляет буффер
    void close(); // Закрывает канал
    std::vector<byte> getReceivedData();

private:
    HANDLE hPipe;
    OVERLAPPED ov;
    byte buffer[512];
    std::vector<byte> receivedData;
    enum State { WAITING_CONNECT, WAITING_READ, CLIENT_CONNECTED } state;
};