#include "channels.h"
#include <cstring>

Channels::Channels(Physics& physics) : m_phys(physics) {
    reset();
}

void Channels::reset() {
    m_state = State::WaitStart;
    m_payloadLen = 0;
    m_payloadPos = 0;
    m_hdrCrc = 0;
    m_pktCrc = 0;
}

// CRC8
uint8_t Channels::updateCrc8(uint8_t crc, uint8_t byte) {
    crc ^= byte;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x80) {
            crc = (crc << 1) ^ 0x07;
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

bool Channels::send(const uint8_t* payload, uint16_t len) {
    if (len > kMaxPayload || !payload) return false;

    // Максимальный размер кадра: 0xFA(1) + len(2) + crc(1) + 0xFB(1) + payload + crc(1) + 0xFE(1)
    uint8_t frame[5 + kMaxPayload + 2];
    uint16_t pos = 0;

    // Заголовок
    frame[pos++] = 0xFA;
    frame[pos++] = len & 0xFF; // l_l
    frame[pos++] = (len >> 8) & 0xFF; // l_h

    // CRC заголовка
    uint8_t hdrCrc = calcCrc8(frame, 3);
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

    return m_phys.send(frame, pos);
}

bool Channels::poll(uint8_t* out, uint16_t outSize, uint16_t* outLen, uint32_t timeoutMs) {
    uint8_t byte;
    
    // Пытаемся прочитать 1 байт.
    // Если байт не пришел за timeoutMs, возвращаем false (пакет не готов)
    uint16_t read = m_phys.recv(&byte, 1, timeoutMs);
    if (read == 0) return false;

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
                m_state = State::WaitData;
            } else {
                m_state = State::WaitStart; // Ошибка CRC заголовка
            }
            break;

        case State::WaitData:
            if (byte == 0xFB) {
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