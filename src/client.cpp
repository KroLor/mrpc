#include "client.h"
#include <iostream>

PipeClient::PipeClient() : hPipe(INVALID_HANDLE_VALUE), ov{0}, isWriting(false), state(DISCONNECTED) {}

PipeClient::~PipeClient() { disconnect(); }

bool PipeClient::connect(const char* pipeName) {
    if (!pipeName) return false;

    std::string fullPath = "\\\\.\\pipe\\" + std::string(pipeName);

    // Пытаемся открыть канал как файл в режиме записи
    hPipe = CreateFileA(
        fullPath.c_str(),
        GENERIC_WRITE, // Клиент только пишет
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

    // Создаем событие для отслеживания асинхронной отправки
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ov.hEvent) {
        disconnect();
        return false;
    }

    state = CONNECTED;
    isWriting = false;
    txBuffer.clear();
    std::cout << "[Client] Connected to server successfully!\n";
    return true;
}

bool PipeClient::sendData(const std::vector<byte>& data) {
    if (state != CONNECTED || data.empty()) return false;

    txBuffer.insert(txBuffer.end(), data.begin(), data.end());
    return true;
}

void PipeClient::update() {
    if (hPipe == INVALID_HANDLE_VALUE || state != CONNECTED) return;

    DWORD bytesTransferred = 0;

    // Если ОС сейчас занята фоновой отправкой, проверяем её статус
    if (isWriting) {
        if (GetOverlappedResult(hPipe, &ov, &bytesTransferred, FALSE)) {
            isWriting = false;
            ResetEvent(ov.hEvent);
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_IO_INCOMPLETE) {
                return; // Данные все еще отправляются, ждем следующий update()
            } else if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA) {
                std::cout << "[Client] Server disconnected.\n";
                disconnect();
                return;
            }
        }
    }

    // Если мы свободны и в очереди txBuffer есть данные - отправляем следующий кусок
    if (!isWriting && !txBuffer.empty()) {
        if (!WriteFile(hPipe, txBuffer.data(), static_cast<DWORD>(txBuffer.size()), NULL, &ov)) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                // Операция ушла в фон
                isWriting = true;
                txBuffer.clear(); // Очищаем локальную очередь, так как ОС забрала данные в обработку
            } else {
                std::cout << "[Client] Write error code: " << err << "\n";
                disconnect();
            }
        } else {
            txBuffer.clear();
        }
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
    state = DISCONNECTED;
    isWriting = false;
    txBuffer.clear();
}
