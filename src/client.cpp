#include "client.h"
#include <iostream>

PipeClient::PipeClient() : hPipe(INVALID_HANDLE_VALUE), ov{0}, ovRead{0}, isWriting(false), state(DISCONNECTED) {}

PipeClient::~PipeClient() { disconnect(); }

bool PipeClient::connect(const char* pipeName) {
    if (!pipeName) return false;

    std::string fullPath = "\\\\.\\pipe\\" + std::string(pipeName);

    // Пытаемся открыть канал как файл в режиме записи
    hPipe = CreateFileA(
        fullPath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, // Без совместного доступа
        NULL, // Безопасность по умолчанию
        OPEN_EXISTING, // Открываем только если сервер уже создан
        FILE_FLAG_OVERLAPPED, // Асинхронный режим
        NULL
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        // Если сервер занят, пробуем подождать его
        if (GetLastError() == ERROR_PIPE_BUSY) {
            WaitNamedPipeA(fullPath.c_str(), 1000);
        }
        return false;
    }

    // Создаем событие для отслеживания асинхронных операций
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    ovRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    state = CONNECTED;
    isWriting = false;
    txBuffer.clear();
    receivedData.clear();
    ReadFile(hPipe, buffer, sizeof(buffer), NULL, &ovRead);
    std::cout << "[Client] Connected to server successfully!\n";
    return true;
}

std::vector<byte> PipeClient::getReceivedData() {
    if (receivedData.empty()) return {};

    std::vector<byte> output = std::move(receivedData);
    return output;
}

bool PipeClient::sendData(const std::vector<byte>& data) {
    if (state != CONNECTED || data.empty()) return false;

    txBuffer.insert(txBuffer.end(), data.begin(), data.end());
    return true;
}

void PipeClient::update() {
    if (hPipe == INVALID_HANDLE_VALUE || state != CONNECTED) return;

    DWORD bytesTransferred = 0;
    bool connectionLost = false;

    // Rece
    while (GetOverlappedResult(hPipe, &ovRead, &bytesTransferred, FALSE)) {
        if (bytesTransferred > 0) {
            receivedData.insert(receivedData.end(), buffer, buffer + bytesTransferred);
        }
        ResetEvent(ovRead.hEvent);
        if (!ReadFile(hPipe, buffer, sizeof(buffer), NULL, &ovRead)) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                break; // Данные кончились, ОС ждет новые байты в фоне
            }
            else if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA) {
                connectionLost = true;
                break;
            }
        }
    }
    if (!connectionLost && GetLastError() == ERROR_BROKEN_PIPE) {
        connectionLost = true;
    }

    // Tr
    if (!connectionLost && isWriting) {
        if (GetOverlappedResult(hPipe, &ov, &bytesTransferred, FALSE)) {
            isWriting = false;
            ResetEvent(ov.hEvent);
        } else {
            DWORD err = GetLastError();
            if (err != ERROR_IO_INCOMPLETE) {
                connectionLost = true;
            }
        }
    }
    if (!connectionLost && !isWriting && !txBuffer.empty()) {
        if (!WriteFile(hPipe, txBuffer.data(), static_cast<DWORD>(txBuffer.size()), NULL, &ov)) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                isWriting = true;
                txBuffer.clear(); 
            } else {
                connectionLost = true;
            }
        } else {
            txBuffer.clear();
        }
    }

    if (connectionLost) {
        std::cout << "[Client] Server disconnected.\n";
        disconnect();
    }
}

void PipeClient::disconnect() {
    if (hPipe != INVALID_HANDLE_VALUE) {
        CancelIo(hPipe);
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
    }
    if (ov.hEvent) {
        CloseHandle(ov.hEvent);
        ov.hEvent = NULL;
    }
    if (ovRead.hEvent) {
        CloseHandle(ovRead.hEvent);
        ovRead.hEvent = NULL;
    }
    state = DISCONNECTED;
    isWriting = false;
    txBuffer.clear();
    receivedData.clear();
}

bool PipeClient::waitForData(uint32_t timeoutMs) {
    if (state != CONNECTED || ovRead.hEvent == NULL) return false;

    // Задача RTOS отдает CPU другим задачам
    if (WaitForSingleObject(ovRead.hEvent, timeoutMs) != WAIT_OBJECT_0) {
        return false;
    }

    // Данные пришли
    DWORD bytesTrans = 0;
    if (GetOverlappedResult(hPipe, &ovRead, &bytesTrans, FALSE)) {
        if (bytesTrans > 0) {
            receivedData.insert(receivedData.end(), buffer, buffer + bytesTrans);
        }
        // Сразу перезапускаем чтение
        ResetEvent(ovRead.hEvent);
        ReadFile(hPipe, buffer, sizeof(buffer), NULL, &ovRead);
        return true;
    }

    disconnect();
    return false;
}