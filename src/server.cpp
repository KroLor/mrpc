#include "server.h"
#include <iostream>

PipeServer::PipeServer() : hPipe(INVALID_HANDLE_VALUE), ov{0}, state(WAITING_CONNECT) {}

PipeServer::~PipeServer() { close(); }

bool PipeServer::init(const char* pipeName) {
    if (!pipeName) return false;

    std::string fullPath = "\\\\.\\pipe\\" + std::string(pipeName);
    
    // Создаем асинхронный канал
    hPipe = CreateNamedPipe(fullPath.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                            1, 512, 512, 0, NULL);

    if (hPipe == INVALID_HANDLE_VALUE) return false;

    // Создаем событие для отслеживания асинхронных операций
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    
    // Начинаем асинхронное ожидание подключения
    ConnectNamedPipe(hPipe, &ov);
    state = WAITING_CONNECT;
    return true;
}

void PipeServer::update() {
    if (hPipe == INVALID_HANDLE_VALUE) return;

    DWORD bytesTransferred = 0;

    if (state == WAITING_CONNECT) {
        // Проверяем, подключился ли клиент (без блокировки, FALSE в последнем параметре)
        if (GetOverlappedResult(hPipe, &ov, &bytesTransferred, FALSE)) {
            std::cout << "[Server] Client connected!\n";
            
            // Сразу запускаем асинхронное чтение
            ResetEvent(ov.hEvent);
            ReadFile(hPipe, buffer, sizeof(buffer), NULL, &ov);
            state = WAITING_READ;
        }
    } 
    else if (state == WAITING_READ) {
        bool clientDisconnected = false;

        while (GetOverlappedResult(hPipe, &ov, &bytesTransferred, FALSE)) {
            if (bytesTransferred > 0) {
                receivedData.insert(receivedData.end(), buffer, buffer + bytesTransferred);
            }

            // Сбрасываем триггер и сразу запрашиваем следующую порцию данных
            ResetEvent(ov.hEvent);
            if (!ReadFile(hPipe, buffer, sizeof(buffer), NULL, &ov)) {
                DWORD err = GetLastError();
                if (err == ERROR_IO_PENDING) {
                    // Данные в канале кончились, Windows перевела чтение в фоновое ожидание
                    break; 
                }
                else if (err == ERROR_BROKEN_PIPE) {
                    clientDisconnected = true;
                    break;
                }
            }
        } 

        if (!clientDisconnected && GetLastError() == ERROR_BROKEN_PIPE) {
            clientDisconnected = true;
        }
        if (clientDisconnected) {
            std::cout << "[Server] Client disconnected. Waiting for a new one...\n";
            
            DisconnectNamedPipe(hPipe); // Отключаем старого клиента с канала
            receivedData.clear(); // Очищаем локальный буфер
            ResetEvent(ov.hEvent); // Сбрасываем триггер перед новой операцией
            ConnectNamedPipe(hPipe, &ov); // Запускаем ожидание нового клиента в фоне
            
            state = WAITING_CONNECT;
        }
    }
}

std::vector<byte> PipeServer::getReceivedData() {
    if (receivedData.empty()) return {};

    std::vector<byte> output = std::move(receivedData);
    
    return output;
}

void PipeServer::close() {
    if (hPipe != INVALID_HANDLE_VALUE) {
        CancelIo(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
    }
    if (ov.hEvent) {
        CloseHandle(ov.hEvent);
        ov.hEvent = NULL;
    }
}