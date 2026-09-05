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
        handleResponse(type, seq, args, argsLen);
    } else if (type == MsgType::Request || type == MsgType::Stream) {
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
}

bool Transport::sendMsg(MsgType type, uint8_t seq, const char* name, const uint8_t* payload, uint16_t payloadLen) {
    uint16_t nameLen = name ? strlen(name) : 0;
    uint16_t totalLen = 1 + 1 + nameLen + 1 + payloadLen; // type + seq + name + '\0' + payload

    if (totalLen > MaxMsg) return false;

    uint16_t pos = 0;
    m_txBuf[pos++] = static_cast<uint8_t>(type);
    m_txBuf[pos++] = seq;
    
    if (name) {
        strcpy((char*)&m_txBuf[pos], name);
        pos += nameLen + 1; // +1 для терминатора
    } else {
        m_txBuf[pos++] = '\0';
    }

    if (payload && payloadLen > 0) {
        memcpy(&m_txBuf[pos], payload, payloadLen);
        pos += payloadLen;
    }

    return m_channels.send(m_txBuf, pos);
}

bool Transport::parseMsg(const uint8_t* pkt, uint16_t len,
                         MsgType& type, uint8_t& seq, const char*& name,
                         const uint8_t*& args, uint16_t& argsLen) {
    if (len < 3) return false; // Минимум type, seq, '\0'

    type = static_cast<MsgType>(pkt[0]);
    seq = pkt[1];
    name = reinterpret_cast<const char*>(&pkt[2]);
    
    uint16_t nameLen = strlen(name);
    uint16_t headerSize = 2 + nameLen + 1; // type + seq + name + '\0'

    if (headerSize > len) return false; // Нет места под аргументы

    args = &pkt[headerSize];
    argsLen = len - headerSize;

    return true;
}

void Transport::handleRequest(MsgType type, uint8_t seq, const char* name, const uint8_t* args, uint16_t argsLen) {
    // Ищем функцию в реестре
    RpcHandler handler = nullptr;
    for (uint8_t i = 0; i < m_regCount; ++i) {
        if (strcmp(m_registry[i].name, name) == 0) {
            handler = m_registry[i].handler;
            break;
        }
    }
    // Если не нашли
    if (!handler) {
        sendMsg(MsgType::Error, seq, "Not find func!", nullptr, 0);
        return;
    }

    // Вызываем функцию. Используем m_txBuf как временный буфер для ответа
    uint16_t respLen = MaxMsg;
    bool success = handler(args, argsLen, m_respBuf, &respLen); // Можно возвращать только bool

    // Отправляем результат
    if (type == MsgType::Request) {
        if (success) {
            sendMsg(MsgType::Response, seq, "Response: ", m_respBuf, respLen);
        } else {
            sendMsg(MsgType::Error, seq, "Error func!", nullptr, 0);
        }
    }
}

void Transport::handleResponse(MsgType type, uint8_t seq, const uint8_t* payload, uint16_t payloadLen) {
    if (m_waitBusy && seq == m_waitSeq) {
        if (type == MsgType::Response) {
            // Копируем данные
            uint16_t copyLen = (payloadLen < m_waitSize) ? payloadLen : m_waitSize; // Данных может быть меньше
            if (copyLen > 0 && m_waitBuf) {
                memcpy(m_waitBuf, payload, copyLen);
            }
            m_waitGot = copyLen;
            m_waitStatus = CallStatus::Success;
        } else if (type == MsgType::Error) {
            m_waitStatus = CallStatus::RemoteError;
            m_waitGot = 0;
        }
        
        // Будим задачу
        xSemaphoreGive(m_waitSem);
    }
}