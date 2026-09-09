#pragma once

#include <cstdint>

#include "channels.h"

extern "C" {
    #include "FreeRTOS.h"
    #include "semphr.h"
}

/**
 * @brief Типы сообщений транспортного уровня.
 */
enum class MsgType : uint8_t {
    Request = 0x0B,   ///< Запрос на выполнение удаленной функции
    Stream = 0x0C,    ///< Запрос на потоковую передачу данных
    Response = 0x16,  ///< Ответ от сервера
    Error = 0x21      ///< Сообщение об ошибке
};

/**
 * @brief Статус выполнения вызова удаленной процедуры.
 */
enum class CallStatus {
    Success = 0,      ///< Успешное выполнение
    Timeout,          ///< Превышен таймаут ожидания
    InvalidArgs,      ///< Неверные аргументы
    RemoteError,      ///< Ошибка на удаленной стороне
    ChannelDown,      ///< Канал связи неактивен
    Error             ///< Общая ошибка
};

/**
 * @brief Тип обработчика зарегистрированной функции.
 * @param args Входные аргументы.
 * @param argsLen Длина входных аргументов.
 * @param out Буфер для выходных данных.
 * @param outLen Длина выходных данных.
 * @return true при успешном выполнении.
 */
using RpcHandler = bool (*)(const uint8_t* args, uint16_t argsLen, uint8_t* out, uint16_t* outLen);

/**
 * @brief Тип колбэка для потоковой передачи данных.
 * @param data Принятые данные.
 * @param len Длина данных.
 */
using StreamCallback = void (*)(const uint8_t* data, uint16_t len);

/**
 * @brief Класс транспортного уровня протокола MRPC.
 * 
 * Реализует логику RPC: регистрацию функций, выполнение вызовов (call),
 * потоковую передачу (stream) и обработку ответов. Использует семафоры
 * FreeRTOS для синхронизации задач.
 */
class Transport {
public:
    /**
     * @brief Конструктор с привязкой к канальному уровню.
     * @param channels Ссылка на объект канального уровня.
     */
    Transport(Channels& channels);

    /**
     * @brief Регистрация удаленной функции.
     * @param name Имя функции.
     * @param handler Указатель на функцию-обработчик.
     * @return true при успешной регистрации.
     */
    bool regFunc(const char* name, RpcHandler handler);

    /**
     * @brief Выполнение удаленного вызова с ожиданием ответа.
     * @param name Имя удаленной функции.
     * @param args Аргументы вызова.
     * @param argsLen Длина аргументов.
     * @param out Буфер для записи ответа.
     * @param outLen Размер буфера / полученная длина ответа.
     * @param timeoutMs Таймаут ожидания в миллисекундах.
     * @return Код статуса выполнения.
     */
    CallStatus call(const char* name,
                    const uint8_t* args, uint16_t argsLen,
                    uint8_t* out, uint16_t* outLen,
                    uint32_t timeoutMs);

    /**
     * @brief Запуск потоковой передачи данных.
     * @param name Имя удаленной функции.
     * @param args Аргументы вызова.
     * @param argsLen Длина аргументов.
     * @param Chunk Колбэк для обработки чанков данных.
     * @param stepTimeoutMs Таймаут ожидания каждого чанка.
     * @return Код статуса выполнения.
     */
    CallStatus stream(const char* name, const uint8_t* args, uint16_t argsLen, StreamCallback Chunk, uint32_t stepTimeoutMs);

    /**
     * @brief Обработка одного входящего сообщения.
     * @param timeoutMs Таймаут ожидания данных.
     * @return true если за итерацию было обработано сообщение.
     */
    bool dispatchOnce(uint32_t timeoutMs);

    /**
     * @brief Уведомление об обрыве соединения.
     */
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
    bool m_streamEnded = false;
};