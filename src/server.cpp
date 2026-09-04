#include <iostream>
#include "server.h"

PipeServer::PipeServer() : hPipe(INVALID_HANDLE_VALUE), ov{0}, ovWrite{0}, isWriting(false), state(WAITING_CONNECT) {}


PipeServer::~PipeServer() { close(); }

bool PipeServer::init(const char* pipeName) {
    if (!pipeName) return false;

    std::string fullPath = "\\\\.\\pipe\\" + std::string(pipeName);
    
    // Создаем асинхронный канал
    hPipe = CreateNamedPipe(fullPath.c_str(), PIPE_ACCESS_DUPLEX  | FILE_FLAG_OVERLAPPED,
                            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                            1, 512, 512, 0, NULL);

    if (hPipe == INVALID_HANDLE_VALUE) return false;

    // Создаем событие для отслеживания асинхронных операций
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    ovWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    
    // Начинаем асинхронное ожидание подключения
    ConnectNamedPipe(hPipe, &ov);
    state = WAITING_CONNECT;
    isWriting = false;
    txBuffer.clear();
    return true;
}

bool PipeServer::sendData(const std::vector<byte>& data) {
    if (hPipe == INVALID_HANDLE_VALUE || state != WAITING_READ || data.empty()) return false;
    
    txBuffer.insert(txBuffer.end(), data.begin(), data.end());
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
        if (!clientDisconnected) {
            // Если в данный момент ОС отправляет данные в фоне - проверяем статус
            if (isWriting) {
                if (GetOverlappedResult(hPipe, &ovWrite, &bytesTransferred, FALSE)) {
                    isWriting = false;
                    ResetEvent(ovWrite.hEvent);
                } else if (GetLastError() != ERROR_IO_INCOMPLETE) {
                    clientDisconnected = true;
                }
            }
            // Если ОС свободна для записи и в очереди появились байты - инициируем отправку
            if (!isWriting && !txBuffer.empty()) {
                if (!WriteFile(hPipe, txBuffer.data(), static_cast<DWORD>(txBuffer.size()), NULL, &ovWrite)) {
                    DWORD err = GetLastError();
                    if (err == ERROR_IO_PENDING) {
                        isWriting = true;
                        txBuffer.clear(); // ОС скопировала данные в системный пул, очищаем локальный буфер
                    } else {
                        clientDisconnected = true;
                    }
                } else {
                    txBuffer.clear(); // Запись прошла мгновенно и синхронно
                }
            }
        }
        if (clientDisconnected) {
            std::cout << "[Server] Client disconnected. Waiting for a new one...\n";
            
            DisconnectNamedPipe(hPipe); // Отключаем старого клиента с канала
            receivedData.clear(); // Очищаем локальный буфер
            
            txBuffer.clear();
            isWriting = false;
            
            ResetEvent(ov.hEvent); // Сбрасываем триггер перед новой операцией
            ResetEvent(ovWrite.hEvent);
            
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
    if (ovWrite.hEvent) {
        CloseHandle(ovWrite.hEvent);
        ovWrite.hEvent = NULL;
    }
}

bool PipeServer::waitForData(uint32_t timeoutMs) {
    if (state != WAITING_READ || ov.hEvent == NULL) return false;

    if (WaitForSingleObject(ov.hEvent, timeoutMs) != WAIT_OBJECT_0) {
        return false;
    }

    DWORD bytesTransferred = 0;
    if (GetOverlappedResult(hPipe, &ov, &bytesTransferred, FALSE)) {
        if (bytesTransferred > 0) {
            receivedData.insert(receivedData.end(), buffer, buffer + bytesTransferred);
        }
        ResetEvent(ov.hEvent);
        ReadFile(hPipe, buffer, sizeof(buffer), NULL, &ov);
        return true;
    }
    
    // Обрыв связи
    std::cout << "[Server] Client disconnected_1.\n";
    DisconnectNamedPipe(hPipe);
    receivedData.clear();
    txBuffer.clear();
    isWriting = false;
    ResetEvent(ov.hEvent);
    ResetEvent(ovWrite.hEvent);
    ConnectNamedPipe(hPipe, &ov);
    state = WAITING_CONNECT;
    return false;
}