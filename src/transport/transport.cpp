#include "transport.h"
#include <cstring>

Transport::Transport(Channels& channels) : 
    m_channels(channels), 
    m_regCount(0), 
    m_waitBusy(false), 
    m_waitSeq(0), 
    m_waitBuf(nullptr), 
    m_waitSize(0), 
    m_waitGot(0), 
    m_waitStatus(CallStatus::Error), 
    m_seq(0) {
    // Бинарный семафор для ожидания ответа
    m_waitSem = xSemaphoreCreateBinary();
    // Счетный семафор для потоковой передачи (максимум 10 сообщений в буфере)
    m_streamSem = xSemaphoreCreateCounting(10, 0);
}

bool Transport::regFunc(const char* name, RpcHandler handler) {
    if (!name || !handler || m_regCount >= MaxFunctions) {
        return false;
    }
    m_registry[m_regCount].name = name;
    m_registry[m_regCount].handler = handler;
    m_regCount++;
    return true;
}

CallStatus Transport::call(const char* name,
                           const uint8_t* args, uint16_t argsLen,
                           uint8_t* out, uint16_t* outLen,
                           uint32_t timeoutMs) {
    if (!name || !out || !outLen) return CallStatus::InvalidArgs;

    // Проверяем, не занят ли слот ожидания другим вызовом
    if (m_waitBusy) {
        return CallStatus::Error; // Только один исходящий вызов одновременно
    }

    // Занимаем слот
    m_waitBusy = true;
    m_waitSeq = ++m_seq; // Последовательное увеличение N
    m_waitBuf = out;
    m_waitSize = *outLen;
    m_waitGot = 0;
    m_waitStatus = CallStatus::Timeout; // По умолчанию

    // Формируем и отправляем запрос
    if (!sendMsg(MsgType::Request, m_waitSeq, name, args, argsLen)) {
        m_waitBusy = false;
        return CallStatus::Error;
    }

    // Блокируем задачу RTOS до получения ответа или таймаута
    if (xSemaphoreTake(m_waitSem, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        *outLen = m_waitGot; // Фактическая длина ответа
    } else {
        // Таймаут
        *outLen = 0;
    }

    // Освобождаем слот для следующих вызовов
    m_waitBusy = false;
    return m_waitStatus;
}

CallStatus Transport::stream(const char* name, const uint8_t* args, uint16_t argsLen, StreamCallback Chunk, uint32_t stepTimeoutMs) {
    if (!name) return CallStatus::InvalidArgs;
    
    // Только один стрим одновременно
    if (m_streamBusy) {
        return CallStatus::Error;
    }
    
    // Занимаем слот стрима
    m_streamBusy = true;
    m_streamSeq = ++m_seq;
    m_streamStatus = CallStatus::Timeout;
    m_streamLastLen = 0;
    m_streamEnded = false;
    
    // Отправляем stream-запрос (0x0C)
    if (!sendMsg(MsgType::Stream, m_streamSeq, name, args, argsLen)) {
        m_streamBusy = false;
        return CallStatus::Error;
    }
    
    for (;;) {
        if (xSemaphoreTake(m_streamSem, pdMS_TO_TICKS(stepTimeoutMs)) != pdTRUE) {
            m_streamStatus = CallStatus::Timeout;
            break;
        }
        // Проверяем, не пришел ли конец стрима (Response/Error)
        if (m_streamEnded) {
            break;
        }

        // Обрабатываем данные стрима
        if (Chunk && m_streamLastLen > 0) {
            Chunk(m_streamLastBuf, m_streamLastLen);
            m_streamLastLen = 0;
        }
        break;
    }
    
    m_streamBusy = false;
    return m_streamStatus;
}

bool Transport::dispatchOnce(uint32_t timeoutMs) {
    uint16_t rxLen = 0;

    // Пытаемся получить целый пакет от канального уровня
    // Если таймаут истек или пакет битый, poll вернет false
    if (!m_channels.poll(m_rxBuf, MaxMsg, &rxLen, timeoutMs)) {
        return false; 
    }

    // Парсим транспортный уровень
    MsgType type;
    uint8_t seq;
    const char* name;
    const uint8_t* args;
    uint16_t argsLen;

    if (!parseMsg(m_rxBuf, rxLen, type, seq, name, args, argsLen)) {
        return true; // Пакет был, но битый
    }

    // Направляем
    if (type == MsgType::Response || type == MsgType::Error) {
        if (m_streamBusy && seq == m_streamSeq) {
            m_streamStatus = (type == MsgType::Response) ? CallStatus::Success : CallStatus::RemoteError;
            m_streamDataReady = false;
            m_streamEnded = true;
            xSemaphoreGive(m_streamSem);
        }
        else {
            handleResponse(type, seq, args, argsLen);
        }
    }
    else if (type == MsgType::Stream && m_streamBusy && seq == m_streamSeq) {
        handleStream(type, seq, args, argsLen);
    }
    else {
        handleRequest(type, seq, name, args, argsLen);
    }

    return true;
}

void Transport::linkDown() {
    if (m_waitBusy) {
        m_waitStatus = CallStatus::ChannelDown;
        m_waitGot = 0;
        // Принудительно будим задачу, которая ждет в call()
        xSemaphoreGive(m_waitSem);
    }

    if (m_streamBusy) {
        m_streamStatus = CallStatus::ChannelDown;
        m_streamDataReady = false;
        m_streamEnded = true;
        xSemaphoreGive(m_streamSem);
    }
}

bool Transport::sendMsg(MsgType type, uint8_t seq, const char* name, const uint8_t* payload, uint16_t payloadLen) {
    const char* _name = name ? name : "";
    uint16_t nameLen = strlen(_name);
    uint16_t totalLen = 1 + 1 + nameLen + 1 + payloadLen; // type + seq + name + '\0' + payload

    if (totalLen > MaxMsg) return false;

    uint16_t pos = 0;
    m_txBuf[pos++] = static_cast<uint8_t>(type);
    m_txBuf[pos++] = seq;
    
    strcpy(reinterpret_cast<char*>(&m_txBuf[pos]), _name);
    pos += nameLen + 1; // +1 для терминатора строки

    if (payload && payloadLen > 0) {
        memcpy(&m_txBuf[pos], payload, payloadLen);
        pos += payloadLen;
    }

    return m_channels.send(m_txBuf, pos);
}

bool Transport::parseMsg(const uint8_t* pkt, uint16_t len,
                         MsgType& type, uint8_t& seq, const char*& name,
                         const uint8_t*& args, uint16_t& argsLen) {
    if (!pkt || len < 3) return false; // Минимум type, seq, '\0'

    type = static_cast<MsgType>(pkt[0]);
    seq = pkt[1];
    name = reinterpret_cast<const char*>(&pkt[2]);
    
    // Проверяем, чтобы имя не выходило за пределы пакета
    uint16_t nameLen = 0;
    for (uint16_t i = 2; i < len && name[nameLen] != '\0'; ++i, ++nameLen) {}

    uint16_t headerSize = 2 + nameLen + 1; // type + seq + name + '\0'

    if (headerSize > len) return false; // Нет места под аргументы

    args = &pkt[headerSize];
    argsLen = len - headerSize;

    return true;
}

void Transport::handleRequest(MsgType type, uint8_t seq, const char* name, const uint8_t* args, uint16_t argsLen) {
    if (!name) {
        sendMsg(MsgType::Error, seq, "Null name", nullptr, 0);
        return;
    }
    // Ищем функцию в реестре
    RpcHandler handler = nullptr;
    for (uint8_t i = 0; i < m_regCount; i++) {
        if (m_registry[i].name && strcmp(m_registry[i].name, name) == 0) {
            handler = m_registry[i].handler;
            break;
        }
    }
    // Если не нашли
    if (!handler) {
        sendMsg(MsgType::Error, seq, "Not find func!", nullptr, 0); // Сообщения Error не выводятся на сервере
        return;
    }

    // Вызываем функцию. Используем m_txBuf как временный буфер для ответа
    uint16_t respLen = MaxMsg;
    bool success = handler(args, argsLen, m_respBuf, &respLen); // Можно возвращать только bool в call режиме (при обычном Request)
    
    // Отправляем результат
    if (type == MsgType::Request) {
        if (success) {
            sendMsg(MsgType::Response, seq, "Response: ", m_respBuf, respLen);
        } else {
            sendMsg(MsgType::Error, seq, "Error func!", nullptr, 0);
        }
    }
    else if (type == MsgType::Stream) {
        if (success) {
            for (uint8_t i = 0; i < 5; i++) {
                sendMsg(MsgType::Stream, seq, "", m_respBuf, respLen);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            sendMsg(MsgType::Response, seq, "", nullptr, 0); // Конец стрима
        } else {
            sendMsg(MsgType::Error, seq, "Error func!", nullptr, 0);
        }
    }
}

void Transport::handleResponse(MsgType type, uint8_t seq, const uint8_t* payload, uint16_t payloadLen) {
    if (!m_waitBusy || seq != m_waitSeq) {
        return;
    }

    if (type == MsgType::Response) {
        // Копируем данные
        uint16_t copyLen = (payloadLen < m_waitSize) ? payloadLen : m_waitSize; // Данных может быть меньше
        if (copyLen > 0 && m_waitBuf && payload) {
            memcpy(m_waitBuf, payload, copyLen);
        }
        m_waitGot = copyLen;
        m_waitStatus = CallStatus::Success;
    } else if (type == MsgType::Error) { // Без текста ошибки
        m_waitStatus = CallStatus::RemoteError;
        m_waitGot = 0;
    }

    // Будим задачу
    xSemaphoreGive(m_waitSem);
}

void Transport::handleStream(MsgType type, uint8_t seq, const uint8_t* payload, uint16_t payloadLen) {
    if (!payload) {
        xSemaphoreGive(m_streamSem);
        return;
    }
    
    if (type == MsgType::Stream) {
        uint16_t copyLen = (payloadLen < MaxMsg) ? payloadLen : MaxMsg;
        if (copyLen > 0) {
            memcpy(m_streamLastBuf, payload, copyLen);
        }
        m_streamLastLen = copyLen;
        m_streamDataReady = true;
        m_streamStatus = CallStatus::Success; // Cтрим продолжается
    }

    xSemaphoreGive(m_streamSem);
}