#include "channels.h"
#include <cstring>
#include <cstdlib>

Channels::Channels(Physics& physics) : m_phys(physics) {
    reset();
}

void Channels::reset() {
    m_state = State::WaitStart;
    m_payloadLen = 0;
    m_payloadPos = 0;
    m_hdrCrc = 0;
    m_pktCrc = 0;
    m_streamHead = 0;
    m_streamTail = 0;
}

// CRC8
uint8_t Channels::updateCrc8(uint8_t crc, uint8_t byte) {
    crc ^= byte;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x80) {
            crc = (crc << 1) ^ 0x16; // Полином 0x16
        } else {
            crc <<= 1;
        }
    }
    return crc;
}
uint8_t Channels::calcCrc8(const uint8_t* data, uint16_t len) {
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; ++i) {
        crc = updateCrc8(crc, data[i]);
    }
    return crc;
}

void Channels::pumpStreamBuf() {
    uint16_t used = m_streamHead - m_streamTail;
    uint16_t freeSpace = kStreamBufSize - used;

    if (freeSpace < 128) {
        return;
    }

    uint16_t writePos = m_streamHead & (kStreamBufSize - 1);
    uint16_t contiguousSpace = kStreamBufSize - writePos;
    uint16_t readLen = (freeSpace < contiguousSpace) ? freeSpace : contiguousSpace;

    if (readLen == 0) {
        return;
    }

    uint16_t received = m_phys.recv(&m_streamBuf[writePos], readLen);

    if (received > 0) {
        m_streamHead += received;
    }
}

bool Channels::readStreamByte(uint8_t* byte) {
    if (m_streamHead == m_streamTail) {
        return false;
    }

    *byte = m_streamBuf[m_streamTail & (kStreamBufSize - 1)]; // Кольцевой буфер
    m_streamTail++;
    return true;
}

bool Channels::send(const uint8_t* payload, uint16_t len) {
    if (len > kMaxPayload || !payload) return false;

    // Максимальный размер кадра: 0xFA(1 байт) + len(2) + crc(1) + 0xFB(1) + payload + crc(1) + 0xFE(1)
    // Используем динамическое выделение памяти под конкретную длину текущего payload
    uint8_t* frame = static_cast<uint8_t*>(std::malloc(5 + len + 2));
    if (!frame) return false;
    
    uint16_t pos = 0;

    // Заголовок
    frame[pos++] = 0xFA;
    frame[pos++] = len & 0xFF; // l_l (незначащие нули отбрасываются (uint8_t))
    frame[pos++] = (len >> 8) & 0xFF; // l_h (старший байт в первом/левом байте длины)

    // CRC заголовка
    uint8_t hdrCrc = calcCrc8(frame, pos);
    frame[pos++] = hdrCrc;

    // Маркер начала данных
    frame[pos++] = 0xFB;

    // Полезная нагрузка
    std::memcpy(&frame[pos], payload, len);
    pos += len;

    // CRC всего пакета
    uint8_t pktCrc = calcCrc8(frame, pos);
    frame[pos++] = pktCrc;

    // Стоповый байт
    frame[pos++] = 0xFE;

    bool result = m_phys.send(frame, pos);
    std::free(frame); // Освобождаем
    return result;
}

bool Channels::poll(uint8_t* out, uint16_t outSize, uint16_t* outLen, uint32_t timeoutMs) {
    uint8_t byte;

    // Сначала пытаемся получить байт из буфера
    if (!readStreamByte(&byte)) {
        pumpStreamBuf();

        if (!readStreamByte(&byte)) {
            return false;
        }
    }

    switch (m_state) {
        case State::WaitStart:
            if (byte == 0xFA) {
                m_state = State::ReadLenLo;
                m_hdrCrc = updateCrc8(0, 0xFA);
                m_pktCrc = updateCrc8(0, 0xFA);
            }
            // Если не 0xFA, остаемся в WaitStart
            break;

        case State::ReadLenLo:
            m_payloadLen = byte;
            m_hdrCrc = updateCrc8(m_hdrCrc, byte);
            m_pktCrc = updateCrc8(m_pktCrc, byte);
            m_state = State::ReadLenHi;
            break;

        case State::ReadLenHi:
            m_payloadLen |= (static_cast<uint16_t>(byte) << 8); // Складываем две части длины пакета
            m_hdrCrc = updateCrc8(m_hdrCrc, byte);
            m_pktCrc = updateCrc8(m_pktCrc, byte);
            
            // Проверка на переполнение буфера
            if (m_payloadLen > kMaxPayload) {
                m_state = State::WaitStart;
            } else {
                m_state = State::ReadHdrCrc;
            }
            break;

        case State::ReadHdrCrc:
            if (byte == m_hdrCrc) {
                m_pktCrc = updateCrc8(m_pktCrc, byte);
                m_state = State::WaitData;
            } else {
                m_state = State::WaitStart; // Ошибка CRC заголовка
            }
            break;

        case State::WaitData:
            if (byte == 0xFB) {
                m_pktCrc = updateCrc8(m_pktCrc, byte);
                m_state = State::ReadPayload;
                m_payloadPos = 0;
            } else {
                m_state = State::WaitStart; // Неверный маркер данных
            }
            break;

        case State::ReadPayload:
            m_payloadBuf[m_payloadPos++] = byte;
            m_pktCrc = updateCrc8(m_pktCrc, byte);
            
            if (m_payloadPos == m_payloadLen) {
                m_state = State::ReadPktCrc;
            }
            break;

        case State::ReadPktCrc:
            if (byte == m_pktCrc) {
                m_state = State::WaitStop;
            } else {
                m_state = State::WaitStart; // Ошибка CRC пакета
            }
            break;

        case State::WaitStop:
            if (byte == 0xFE) {
                // Пакет успешно собран и проверен
                if (m_payloadLen <= outSize && out != nullptr) {
                    std::memcpy(out, m_payloadBuf, m_payloadLen);
                    *outLen = m_payloadLen;
                    m_state = State::WaitStart;
                    return true;
                } else {
                    // Буфер слишком мал
                    m_state = State::WaitStart;
                    return false; 
                }
            } else {
                m_state = State::WaitStart; // Неверный стоповый байт
            }
            break;
    }

    return false;
}