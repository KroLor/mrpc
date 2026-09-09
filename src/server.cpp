#include <iostream>
#include "server.h"

PipeServer::PipeServer()
    : hPipe(INVALID_HANDLE_VALUE)
    , ov{}
    , ovWrite{}
    , isWriting(false)
    , state(WAITING_CONNECT)
{
}

PipeServer::~PipeServer()
{
    close();
}

bool PipeServer::init(const char* pipeName)
{
    if (!pipeName) {
        return false;
    }

    std::string fullPath = "\\\\.\\pipe\\" + std::string(pipeName);

    // Создаем асинхронный канал
    hPipe = CreateNamedPipeA(
        fullPath.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        512,
        512,
        0,
        nullptr
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    // Создаем события для отслеживания асинхронных операций
    ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    ovWrite.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    // Начинаем асинхронное ожидание подключения
    ConnectNamedPipe(hPipe, &ov);
    state = WAITING_CONNECT;
    isWriting = false;
    txBuffer.clear();

    return true;
}

bool PipeServer::sendData(const std::vector<byte>& data)
{
    if (hPipe == INVALID_HANDLE_VALUE || state != WAITING_READ || data.empty()) {
        return false;
    }

    txBuffer.insert(txBuffer.end(), data.begin(), data.end());
    return true;
}

void PipeServer::update()
{
    if (hPipe == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD bytesTransferred = 0;

    if (state == WAITING_CONNECT) {
        // Проверяем, подключился ли клиент (без блокировки)
        if (GetOverlappedResult(hPipe, &ov, &bytesTransferred, FALSE)) {
            std::cout << "[Server] Client connected!" << std::endl;

            // Запускаем асинхронное чтение
            ResetEvent(ov.hEvent);
            ReadFile(hPipe, buffer, sizeof(buffer), nullptr, &ov);
            state = WAITING_READ;
        }
    }
    else if (state == WAITING_READ) {
        bool clientDisconnected = false;

        while (GetOverlappedResult(hPipe, &ov, &bytesTransferred, FALSE)) {
            if (bytesTransferred > 0) {
                receivedData.insert(receivedData.end(), buffer, buffer + bytesTransferred);
            }

            // Сбрасываем событие и запрашиваем следующую порцию данных
            ResetEvent(ov.hEvent);
            if (!ReadFile(hPipe, buffer, sizeof(buffer), nullptr, &ov)) {
                DWORD err = GetLastError();
                if (err == ERROR_IO_PENDING) {
                    // Чтение перешло в фоновый режим
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
            // Проверяем статус операции записи
            if (isWriting) {
                if (GetOverlappedResult(hPipe, &ovWrite, &bytesTransferred, FALSE)) {
                    isWriting = false;
                    ResetEvent(ovWrite.hEvent);
                }
                else if (GetLastError() != ERROR_IO_INCOMPLETE) {
                    clientDisconnected = true;
                }
            }

            // Инициируем запись, если есть данные и ОС свободна
            if (!isWriting && !txBuffer.empty()) {
                if (!WriteFile(hPipe, txBuffer.data(), static_cast<DWORD>(txBuffer.size()), nullptr, &ovWrite)) {
                    DWORD err = GetLastError();
                    if (err == ERROR_IO_PENDING) {
                        isWriting = true;
                        txBuffer.clear(); // Данные переданы в системный буфер
                    }
                    else {
                        clientDisconnected = true;
                    }
                }
                else {
                    txBuffer.clear(); // Запись завершена синхронно
                }
            }
        }

        if (clientDisconnected) {
            std::cout << "[Server] Client disconnected. Waiting for a new one..." << std::endl;

            DisconnectNamedPipe(hPipe);
            receivedData.clear();
            txBuffer.clear();
            isWriting = false;
            ResetEvent(ov.hEvent);
            ResetEvent(ovWrite.hEvent);
            ConnectNamedPipe(hPipe, &ov);
            state = WAITING_CONNECT;
        }
    }
}

std::vector<byte> PipeServer::getReceivedData()
{
    if (receivedData.empty()) {
        return {};
    }

    return std::move(receivedData);
}

void PipeServer::close()
{
    if (hPipe != INVALID_HANDLE_VALUE) {
        CancelIo(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
    }

    if (ov.hEvent) {
        CloseHandle(ov.hEvent);
        ov.hEvent = nullptr;
    }

    if (ovWrite.hEvent) {
        CloseHandle(ovWrite.hEvent);
        ovWrite.hEvent = nullptr;
    }
}

bool PipeServer::waitForData(uint32_t timeoutMs)
{
    if (state != WAITING_READ || ov.hEvent == nullptr) {
        return false;
    }

    if (WaitForSingleObject(ov.hEvent, timeoutMs) != WAIT_OBJECT_0) {
        return false;
    }

    DWORD bytesTransferred = 0;
    if (GetOverlappedResult(hPipe, &ov, &bytesTransferred, FALSE)) {
        if (bytesTransferred > 0) {
            receivedData.insert(receivedData.end(), buffer, buffer + bytesTransferred);
        }
        ResetEvent(ov.hEvent);
        ReadFile(hPipe, buffer, sizeof(buffer), nullptr, &ov);
        return true;
    }

    // Обрыв связи
    std::cout << "[Server] Client disconnected." << std::endl;
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