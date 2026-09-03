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
    bool sendData(const std::vector<byte>& data); 
    std::vector<byte> getReceivedData();

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