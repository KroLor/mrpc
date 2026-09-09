#include "client.h"
#include <iostream>

PipeClient::PipeClient()
    : hPipe(INVALID_HANDLE_VALUE)
    , ov{}
    , ovRead{}
    , isWriting(false)
    , state(DISCONNECTED)
{
    InitializeCriticalSection(&m_cs);
}

PipeClient::~PipeClient()
{
    disconnect();
}

bool PipeClient::connect(const char* pipeName)
{
    if (!pipeName) {
        return false;
    }

    std::string fullPath = "\\\\.\\pipe\\" + std::string(pipeName);

    // Открываем именованный канал
    hPipe = CreateFileA(
        fullPath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,                      // Без совместного доступа
        nullptr,                // Безопасность по умолчанию
        OPEN_EXISTING,          // Открываем существующий канал
        FILE_FLAG_OVERLAPPED,   // Асинхронный режим
        nullptr
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        // Если сервер занят, ждем
        if (GetLastError() == ERROR_PIPE_BUSY) {
            WaitNamedPipeA(fullPath.c_str(), 1000);
        }
        return false;
    }

    // Создаем события для асинхронных операций
    ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    ovRead.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    state = CONNECTED;
    isWriting = false;
    txBuffer.clear();
    receivedData.clear();
    ReadFile(hPipe, buffer, sizeof(buffer), nullptr, &ovRead);

    std::cout << "[Client] Connected to server successfully!" << std::endl;
    return true;
}

std::vector<byte> PipeClient::getReceivedData()
{
    if (receivedData.empty()) {
        return {};
    }

    return std::move(receivedData);
}

bool PipeClient::sendData(const std::vector<byte>& data)
{
    if (state != CONNECTED || data.empty()) {
        return false;
    }

    EnterCriticalSection(&m_cs);
    txBuffer.insert(txBuffer.end(), data.begin(), data.end());
    pumpWrite();
    LeaveCriticalSection(&m_cs);

    return true;
}

void PipeClient::pumpWrite()
{
    if (state != CONNECTED || hPipe == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD bytes = 0;

    // Проверяем завершение предыдущей операции записи
    if (isWriting) {
        if (GetOverlappedResult(hPipe, &ov, &bytes, FALSE)) {
            isWriting = false;
            txWriting.clear();
            ResetEvent(ov.hEvent);
        }
        else if (GetLastError() != ERROR_IO_INCOMPLETE) {
            disconnect();
            return;
        }
    }

    // Инициируем новую запись, если есть данные
    if (!isWriting && !txBuffer.empty()) {
        txWriting = std::move(txBuffer);

        if (!WriteFile(hPipe, txWriting.data(), static_cast<DWORD>(txWriting.size()), nullptr, &ov)) {
            if (GetLastError() == ERROR_IO_PENDING) {
                isWriting = true;
            }
            else {
                disconnect();
            }
        }
        else {
            txWriting.clear();
        }
    }
}

void PipeClient::update()
{
    if (hPipe == INVALID_HANDLE_VALUE || state != CONNECTED) {
        return;
    }

    DWORD bytesTransferred = 0;
    bool connectionLost = false;

    // Обработка входящих данных
    while (GetOverlappedResult(hPipe, &ovRead, &bytesTransferred, FALSE)) {
        if (bytesTransferred > 0) {
            receivedData.insert(receivedData.end(), buffer, buffer + bytesTransferred);
        }
        ResetEvent(ovRead.hEvent);

        if (!ReadFile(hPipe, buffer, sizeof(buffer), nullptr, &ovRead)) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                // Чтение перешло в фоновый режим
                break;
            }
            else if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA) {
                connectionLost = true;
                break;
            }
        }
    }

    EnterCriticalSection(&m_cs);
    pumpWrite();
    LeaveCriticalSection(&m_cs);

    if (!connectionLost && GetLastError() == ERROR_BROKEN_PIPE) {
        connectionLost = true;
    }

    if (connectionLost) {
        std::cout << "[Client] Server disconnected." << std::endl;
        disconnect();
    }
}

void PipeClient::disconnect()
{
    if (hPipe != INVALID_HANDLE_VALUE) {
        CancelIo(hPipe);
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
    }

    if (ov.hEvent) {
        CloseHandle(ov.hEvent);
        ov.hEvent = nullptr;
    }

    if (ovRead.hEvent) {
        CloseHandle(ovRead.hEvent);
        ovRead.hEvent = nullptr;
    }

    state = DISCONNECTED;
    isWriting = false;
    txBuffer.clear();
    receivedData.clear();
}

bool PipeClient::waitForData(uint32_t timeoutMs)
{
    if (state != CONNECTED || ovRead.hEvent == nullptr) {
        return false;
    }

    // Ожидаем событие с таймаутом
    if (WaitForSingleObject(ovRead.hEvent, timeoutMs) != WAIT_OBJECT_0) {
        return false;
    }

    DWORD bytesTrans = 0;
    if (GetOverlappedResult(hPipe, &ovRead, &bytesTrans, FALSE)) {
        if (bytesTrans > 0) {
            receivedData.insert(receivedData.end(), buffer, buffer + bytesTrans);
        }
        // Перезапускаем чтение
        ResetEvent(ovRead.hEvent);
        ReadFile(hPipe, buffer, sizeof(buffer), nullptr, &ovRead);
        return true;
    }

    disconnect();
    return false;
}