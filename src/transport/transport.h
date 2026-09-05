#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "channels.h"

extern "C" {
    #include "FreeRTOS.h"
    #include "semphr.h"
}

// Типы сообщений (байт 0 сообщения транспортного уровня)
enum class MsgType : uint8_t {
    Request = 0x0B,
    Stream = 0x0C,
    Response = 0x16,
    Error = 0x21
};

enum class CallStatus {
    Success = 0,
    Timeout,
    InvalidArgs,
    RemoteError,
    ChannelDown,
    Error
};

using RpcHandler = bool (*)(const uint8_t* args, uint16_t argsLen, uint8_t* out, uint16_t* outLen);
using StreamCallback = void (*)(const uint8_t* data, uint16_t len);

class Transport {
public:
    Transport(Channels& channels);

    bool regFunc(const char* name, RpcHandler handler);

    CallStatus call(const char* name,
                    const uint8_t* args, uint16_t argsLen,
                    uint8_t* out, uint16_t* outLen,
                    uint32_t timeoutMs);

    CallStatus stream(const char* name, const uint8_t* args, uint16_t argsLen, StreamCallback Chunk, uint32_t stepTimeoutMs);

    // 0x16 - разбудить висящий call() с совпавшим N
    // 0x0B/0x0C - поиск в реестре, вызов, отправка ответ/ошибка
    // true - за итерацию обработано сообщение
    bool dispatchOnce(uint32_t timeoutMs);

    // (обрыв линии)
    void linkDown();

private:
    bool sendMsg(MsgType type, uint8_t seq, const char* name,
                 const uint8_t* payload, uint16_t payloadLen);
    bool parseMsg(const uint8_t* pkt, uint16_t len,
                  MsgType& type, uint8_t& seq, const char*& name,
                  const uint8_t*& args, uint16_t& argsLen);
    void handleRequest(MsgType type, uint8_t seq, const char* name,
                       const uint8_t* args, uint16_t argsLen);
    void handleResponse(MsgType type, uint8_t seq,
                        const uint8_t* payload, uint16_t payloadLen);
    void handleStream(MsgType type, uint8_t seq, const uint8_t* payload, uint16_t payloadLen);


    Channels& m_channels;

    static constexpr uint8_t MaxFunctions = 10;
    struct FuncEntry {
        const char* name;
        RpcHandler handler;
    };
    FuncEntry m_registry[MaxFunctions];
    uint8_t m_regCount = 0;

    uint8_t m_seq = 0;
    static constexpr uint16_t MaxMsg = 256;
    uint8_t m_txBuf[MaxMsg];
    uint8_t m_rxBuf[MaxMsg];
    uint8_t m_respBuf[MaxMsg];

    // Единственный слот исходящего запроса, ожидающего ответа
    SemaphoreHandle_t m_waitSem;
    bool m_waitBusy = false;
    uint8_t m_waitSeq = 0; // Кол-во/номер N
    uint8_t* m_waitBuf = nullptr; // Ожидаемые данные
    uint16_t m_waitSize = 0;
    uint16_t m_waitGot = 0;
    CallStatus m_waitStatus = CallStatus::Error;

    SemaphoreHandle_t m_streamSem;
    bool m_streamBusy = false;
    uint8_t m_streamSeq = 0;
    uint8_t m_streamLastBuf[MaxMsg];
    uint16_t m_streamLastLen = 0;
    CallStatus m_streamStatus = CallStatus::Error;
    bool m_streamDataReady = false;
};